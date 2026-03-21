/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/dev_console.h"

#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/datetime.h"
#include "core/bandplan.h"
#include "core/state.h"
#include "interfaces/cps_io.h"
#include "rtx/rtx.h"
#include "interfaces/delays.h"
#include "interfaces/platform.h"

#ifndef PLATFORM_LINUX
#include "drivers/usb_vcom.h"
#endif

#define DEV_CONSOLE_MAX_LINES 64
#define DEV_CONSOLE_USB_QUEUE_LEN 16
#define DEV_CONSOLE_CMD_BUF_LEN 64
#define DEV_CONSOLE_WRITE_UNLOCK_MS 60000

typedef enum
{
    DEV_DUMP_NONE = 0,
    DEV_DUMP_CPS_CHANNELS,
    DEV_DUMP_CPS_CONTACTS,
    DEV_DUMP_CPS_BANKS
} devConsoleDumpType_t;

static pthread_mutex_t devConsoleMutex = PTHREAD_MUTEX_INITIALIZER;
static char devConsoleLines[DEV_CONSOLE_MAX_LINES][DEV_CONSOLE_LINE_LEN];
static char devConsoleUsbQueue[DEV_CONSOLE_USB_QUEUE_LEN][DEV_CONSOLE_USB_LINE_LEN + 2];
static size_t devConsoleStart = 0;
static size_t devConsoleCount = 0;
static size_t devConsoleUsbRead = 0;
static size_t devConsoleUsbWrite = 0;
static size_t devConsoleUsbCount = 0;
static bool devConsoleUsbExport = false;
static bool devConsoleReady = false;
static bool devConsoleJsonMode = false;
static char devConsoleCmdBuf[DEV_CONSOLE_CMD_BUF_LEN];
static size_t devConsoleCmdLen = 0;
static long long devConsoleWriteUnlockUntil = 0;
static devConsoleDumpType_t devConsoleDumpType = DEV_DUMP_NONE;
static size_t devConsoleDumpIndex = 0;
static size_t devConsoleDumpEnd = 0;
static bool devConsoleDumpDone = false;

static void devConsoleQueueUsbResponse(const char *fmt, ...);

static bool devConsoleWritesUnlocked(void)
{
    return getTick() < devConsoleWriteUnlockUntil;
}

static void devConsoleUnlockWrites(void)
{
    devConsoleWriteUnlockUntil = getTick() + DEV_CONSOLE_WRITE_UNLOCK_MS;
}

static size_t devConsoleCountChannels(void)
{
    size_t count = 0;
    channel_t channel;

    while((count < UINT16_MAX) && (cps_readChannel(&channel, count + 1) == 0))
        count++;

    return count;
}

static size_t devConsoleCountContacts(void)
{
    size_t count = 0;
    contact_t contact;

    while((count < UINT16_MAX) && (cps_readContact(&contact, count + 1) == 0))
        count++;

    return count;
}

static size_t devConsoleCountBanks(void)
{
    size_t count = 0;
    bankHdr_t bank;

    while((count < UINT16_MAX) && (cps_readBankHeader(&bank, count) == 0))
        count++;

    return count;
}

static int devConsoleFindChannelZone(uint16_t channelIndex)
{
    size_t bankCount = devConsoleCountBanks();

    for(uint16_t bank = 0; bank < bankCount; bank++)
    {
        bankHdr_t header = {0};

        if(cps_readBankHeader(&header, bank) != 0)
            continue;

        for(uint16_t pos = 0; pos < header.ch_count; pos++)
        {
            if(cps_readBankData(bank, pos) == (int) (channelIndex - 1))
                return bank;
        }
    }

    return -1;
}

static int devConsoleSetChannelZone(uint16_t channelIndex, int zoneIndex)
{
    size_t bankCount = devConsoleCountBanks();

    for(uint16_t bank = 0; bank < bankCount; bank++)
    {
        bankHdr_t header = {0};

        if(cps_readBankHeader(&header, bank) != 0)
            continue;

        for(uint16_t pos = 0; pos < header.ch_count; pos++)
        {
            if(cps_readBankData(bank, pos) == (int) (channelIndex - 1))
            {
                if(cps_deleteBankData(bank, pos) != 0)
                    return -1;
                header.ch_count--;
                pos--;
            }
        }
    }

    if(zoneIndex < 0)
        return 0;

    if(cps_readBankHeader(&(bankHdr_t){0}, zoneIndex) != 0)
        return -1;

    bankHdr_t header = {0};
    if(cps_readBankHeader(&header, zoneIndex) != 0)
        return -1;

    return cps_insertBankData(channelIndex - 1, zoneIndex, header.ch_count);
}

static bool devConsoleGetChannelLocationE4(const channel_t *channel, int32_t *lat_e4,
                                           int32_t *lon_e4)
{
    if(channel == NULL)
        return false;

    if((channel->ch_location.ch_lat_int == 0) && (channel->ch_location.ch_lat_dec == 0) &&
       (channel->ch_location.ch_lon_int == 0) && (channel->ch_location.ch_lon_dec == 0))
    {
        if(lat_e4 != NULL) *lat_e4 = 0;
        if(lon_e4 != NULL) *lon_e4 = 0;
        return false;
    }

    int32_t lat = (int32_t) channel->ch_location.ch_lat_int * 10000;
    int32_t lon = (int32_t) channel->ch_location.ch_lon_int * 10000;

    lat += (channel->ch_location.ch_lat_int < 0)
         ? -(int32_t) channel->ch_location.ch_lat_dec
         : (int32_t) channel->ch_location.ch_lat_dec;
    lon += (channel->ch_location.ch_lon_int < 0)
         ? -(int32_t) channel->ch_location.ch_lon_dec
         : (int32_t) channel->ch_location.ch_lon_dec;

    if(lat_e4 != NULL) *lat_e4 = lat;
    if(lon_e4 != NULL) *lon_e4 = lon;
    return true;
}

static bool devConsoleSetChannelLocationE4(channel_t *channel, int32_t lat_e4, int32_t lon_e4)
{
    int32_t lat_int;
    int32_t lon_int;
    int32_t lat_dec;
    int32_t lon_dec;

    if(channel == NULL)
        return false;
    if((lat_e4 < -900000) || (lat_e4 > 900000))
        return false;
    if((lon_e4 < -1800000) || (lon_e4 > 1800000))
        return false;

    lat_int = lat_e4 / 10000;
    lon_int = lon_e4 / 10000;
    lat_dec = lat_e4 % 10000;
    lon_dec = lon_e4 % 10000;

    if(lat_dec < 0) lat_dec = -lat_dec;
    if(lon_dec < 0) lon_dec = -lon_dec;

    channel->ch_location.ch_lat_int = (int8_t) lat_int;
    channel->ch_location.ch_lat_dec = (uint16_t) lat_dec;
    channel->ch_location.ch_lon_int = (int16_t) lon_int;
    channel->ch_location.ch_lon_dec = (uint16_t) lon_dec;
    return true;
}

static bool devConsoleParseSignedE4(const char *value, int32_t *out)
{
    long whole;
    long frac = 0;
    char *end;
    bool negative;
    const char *dot;
    char fracBuf[5] = "0000";

    if((value == NULL) || (out == NULL))
        return false;

    whole = strtol(value, &end, 10);
    if((end == value) || ((*end != '\0') && (*end != '.')))
        return false;

    negative = (whole < 0) || (value[0] == '-');
    dot = strchr(value, '.');
    if(dot != NULL)
    {
        size_t fracLen = strlen(dot + 1);
        if(fracLen > 4)
            fracLen = 4;
        memcpy(fracBuf, dot + 1, fracLen);
        frac = strtol(fracBuf, NULL, 10);
    }

    if(whole < 0)
        whole = -whole;

    *out = (int32_t) (whole * 10000 + frac);
    if(negative)
        *out = -*out;

    return true;
}

static void devConsoleFormatE4(char *buf, size_t len, int32_t value_e4)
{
    int32_t whole = value_e4 / 10000;
    int32_t frac = value_e4 % 10000;

    if(frac < 0)
        frac = -frac;

    snprintf(buf, len, "%ld.%04ld", (long) whole, (long) frac);
}

static void devConsoleStartDump(devConsoleDumpType_t type, size_t start, size_t end)
{
    devConsoleDumpType = type;
    devConsoleDumpIndex = start;
    devConsoleDumpEnd = end;
    devConsoleDumpDone = false;
}

static void devConsoleStopDump(void)
{
    devConsoleDumpType = DEV_DUMP_NONE;
    devConsoleDumpIndex = 0;
    devConsoleDumpEnd = 0;
    devConsoleDumpDone = false;
}

static const char *devConsoleModeName(uint8_t mode)
{
    switch(mode)
    {
        case OPMODE_FM: return "fm";
        case OPMODE_DMR: return "dmr";
        case OPMODE_M17: return "m17";
        default: return "none";
    }
}

static bool devConsoleParseMode(const char *value, uint8_t *mode)
{
    if((value == NULL) || (mode == NULL))
        return false;

    if(strcmp(value, "fm") == 0)
    {
        *mode = OPMODE_FM;
        return true;
    }
    if(strcmp(value, "dmr") == 0)
    {
        *mode = OPMODE_DMR;
        return true;
    }
    if(strcmp(value, "m17") == 0)
    {
        *mode = OPMODE_M17;
        return true;
    }

    return false;
}

static void devConsoleReplyChannelJson(uint16_t index, const channel_t *channel)
{
    int32_t lat_e4 = 0;
    int32_t lon_e4 = 0;
    int zone = devConsoleFindChannelZone(index);
    bool hasLocation = devConsoleGetChannelLocationE4(channel, &lat_e4, &lon_e4);

    devConsoleQueueUsbResponse("{\"type\":\"channel\",\"index\":%u,\"name\":\"%s\",\"mode\":\"%s\",\"rx\":%u,\"tx\":%u,\"bw\":%u,\"power\":%u,\"scanlist\":%u,\"zone\":%d,\"rxOnly\":%u,\"lat_e4\":%ld,\"lon_e4\":%ld,\"alt\":%u,\"fm\":{\"rxToneEn\":%u,\"rxTone\":%u,\"txToneEn\":%u,\"txTone\":%u},\"m17\":{\"rxCan\":%u,\"txCan\":%u,\"mode\":%u,\"encr\":%u,\"gps\":%u,\"key\":%u,\"sub\":%u,\"contact\":%u},\"hasLocation\":%u}",
                               index, channel->name, devConsoleModeName(channel->mode),
                               channel->rx_frequency, channel->tx_frequency,
                               channel->bandwidth, channel->power, channel->scanList_index,
                               zone, channel->rx_only ? 1 : 0,
                               (long) lat_e4, (long) lon_e4, channel->ch_location.ch_altitude,
                               channel->fm.rxToneEn, channel->fm.rxTone,
                               channel->fm.txToneEn, channel->fm.txTone,
                               channel->m17.rxCan, channel->m17.txCan, channel->m17.mode,
                               channel->m17.encr, channel->m17.gps_mode,
                               channel->m17.key_index, channel->m17.enc_subtype,
                               channel->m17.contact_index, hasLocation ? 1 : 0);
}

static void devConsoleReplyContactJson(uint16_t index, const contact_t *contact)
{
    char m17Addr[13] = {0};

    for(size_t i = 0; i < sizeof(contact->info.m17.address); i++)
        snprintf(m17Addr + (i * 2), sizeof(m17Addr) - (i * 2), "%02X", contact->info.m17.address[i]);

    devConsoleQueueUsbResponse("{\"type\":\"contact\",\"index\":%u,\"name\":\"%s\",\"mode\":\"%s\",\"dmr\":{\"id\":%lu,\"type\":%u,\"rxtone\":%u},\"m17\":{\"address\":\"%s\"}}",
                               index, contact->name, devConsoleModeName(contact->mode),
                               (unsigned long) contact->info.dmr.id,
                               contact->info.dmr.contactType,
                               contact->info.dmr.rx_tone,
                               m17Addr);
}

static void devConsoleReplyBankJson(uint16_t index, const bankHdr_t *bank)
{
    devConsoleQueueUsbResponse("{\"type\":\"bank\",\"index\":%u,\"name\":\"%s\",\"count\":%u}",
                               index, bank->name, bank->ch_count);
}

static void devConsoleReplyChannel(uint16_t index)
{
    channel_t channel;

    if(cps_readChannel(&channel, index) != 0)
    {
        devConsoleQueueUsbResponse("ERR no channel %u", index);
        return;
    }

    if(devConsoleJsonMode)
    {
        devConsoleReplyChannelJson(index, &channel);
        return;
    }

    int32_t lat_e4 = 0;
    int32_t lon_e4 = 0;
    char latBuf[16] = "";
    char lonBuf[16] = "";
    bool hasLocation = devConsoleGetChannelLocationE4(&channel, &lat_e4, &lon_e4);

    if(hasLocation)
    {
        devConsoleFormatE4(latBuf, sizeof(latBuf), lat_e4);
        devConsoleFormatE4(lonBuf, sizeof(lonBuf), lon_e4);
    }

    devConsoleQueueUsbResponse("ch=%u name=%s mode=%s rx=%u tx=%u bw=%u pwr=%u scan=%u zone=%d rxOnly=%u lat=%s lon=%s alt=%u",
                               index, channel.name, devConsoleModeName(channel.mode),
                               channel.rx_frequency, channel.tx_frequency,
                               channel.bandwidth, channel.power, channel.scanList_index,
                               devConsoleFindChannelZone(index), channel.rx_only ? 1 : 0,
                               hasLocation ? latBuf : "", hasLocation ? lonBuf : "",
                               channel.ch_location.ch_altitude);
    devConsoleQueueUsbResponse("fm rxEn=%u rxTone=%u txEn=%u txTone=%u m17 rxCan=%u txCan=%u mode=%u encr=%u gps=%u key=%u sub=%u contact=%u",
                               channel.fm.rxToneEn, channel.fm.rxTone,
                               channel.fm.txToneEn, channel.fm.txTone,
                               channel.m17.rxCan, channel.m17.txCan,
                               channel.m17.mode, channel.m17.encr,
                               channel.m17.gps_mode, channel.m17.key_index,
                               channel.m17.enc_subtype, channel.m17.contact_index);
}

static void devConsoleReplyContact(uint16_t index)
{
    contact_t contact;

    if(cps_readContact(&contact, index) != 0)
    {
        devConsoleQueueUsbResponse("ERR no contact %u", index);
        return;
    }

    if(devConsoleJsonMode)
    {
        devConsoleReplyContactJson(index, &contact);
        return;
    }

    devConsoleQueueUsbResponse("contact=%u name=%s mode=%s", index,
                               contact.name, devConsoleModeName(contact.mode));
    devConsoleQueueUsbResponse("dmr id=%lu type=%u rxtone=%u m17addr=%02X%02X%02X%02X%02X%02X",
                               (unsigned long) contact.info.dmr.id,
                               contact.info.dmr.contactType,
                               contact.info.dmr.rx_tone,
                               contact.info.m17.address[0], contact.info.m17.address[1],
                               contact.info.m17.address[2], contact.info.m17.address[3],
                               contact.info.m17.address[4], contact.info.m17.address[5]);
}

static void devConsoleReplyBank(uint16_t index)
{
    bankHdr_t bank;

    if(cps_readBankHeader(&bank, index) != 0)
    {
        devConsoleQueueUsbResponse("ERR no bank %u", index);
        return;
    }

    if(devConsoleJsonMode)
    {
        devConsoleReplyBankJson(index, &bank);
        return;
    }

    devConsoleQueueUsbResponse("bank=%u name=%s count=%u", index,
                               bank.name, bank.ch_count);
}

static void devConsoleReplyBankMembers(uint16_t index)
{
    bankHdr_t bank;

    if(cps_readBankHeader(&bank, index) != 0)
    {
        devConsoleQueueUsbResponse("ERR no bank %u", index);
        return;
    }

    if(devConsoleJsonMode)
        devConsoleQueueUsbResponse("{\"type\":\"bank_members_start\",\"index\":%u,\"count\":%u}", index, bank.ch_count);
    else
        devConsoleQueueUsbResponse("bank=%u members=%u", index, bank.ch_count);

    for(uint16_t pos = 0; pos < bank.ch_count; pos++)
    {
        int member = cps_readBankData(index, pos);
        if(member < 0)
            continue;

        if(devConsoleJsonMode)
            devConsoleQueueUsbResponse("{\"type\":\"bank_member\",\"bank\":%u,\"pos\":%u,\"channel\":%u}", index, pos, (unsigned int) member + 1);
        else
            devConsoleQueueUsbResponse("bank=%u pos=%u channel=%u", index, pos, (unsigned int) member + 1);
    }

    if(devConsoleJsonMode)
        devConsoleQueueUsbResponse("{\"type\":\"bank_members_end\",\"index\":%u}", index);
    else
        devConsoleQueueUsbResponse("ok bank members complete");
}

static void devConsoleReplySetting(const char *key)
{
    if(strcmp(key, "all") == 0)
    {
        if(devConsoleJsonMode)
        {
            devConsoleQueueUsbResponse("{\"type\":\"settings\",\"brightness\":%u,\"contrast\":%u,\"sql\":%u,\"vox\":%u,\"timezone\":%d,\"gps\":%u,\"gpssettime\":%u,\"callsign\":\"%s\",\"displaytimer\":%u,\"vplevel\":%u,\"vpphonetic\":%u,\"macrolatch\":%u,\"powerprofile\":%u,\"theme\":%u,\"bandplan\":%u,\"showbatteryicon\":%u,\"m17can\":%u,\"m17canrx\":%u,\"m17dest\":\"%s\",\"m17enc\":%u,\"m17encsub\":%u,\"m17keyindex\":%u,\"m17key1\":\"%s\",\"m17key2\":\"%s\",\"m17key3\":\"%s\",\"m17key4\":\"%s\",\"usbLog\":%u,\"metatext\":\"%s\",\"ppm\":%d}",
                                        state.settings.brightness,
                                       state.settings.contrast,
                                       state.settings.sqlLevel,
                                       state.settings.voxLevel,
                                       state.settings.utc_timezone,
                                       state.settings.gps_enabled ? 1 : 0,
                                       state.settings.gpsSetTime ? 1 : 0,
                                       state.settings.callsign,
                                       state.settings.display_timer,
                                       state.settings.vpLevel,
                                       state.settings.vpPhoneticSpell ? 1 : 0,
                                        state.settings.macroMenuLatch ? 1 : 0,
                                        state.settings.powerProfile,
                                        state.settings.theme,
                                        state.settings.bandplan,
                                        state.settings.showBatteryIcon ? 1 : 0,
                                       state.settings.m17_can,
                                       state.settings.m17_can_rx ? 1 : 0,
                                       state.settings.m17_dest,
                                       state.settings.m17_default_encryption,
                                       state.settings.m17_default_enc_subtype,
                                       state.settings.m17_default_key_index,
                                       state.settings.m17_keys[0],
                                       state.settings.m17_keys[1],
                                       state.settings.m17_keys[2],
                                       state.settings.m17_keys[3],
                                       devConsole_getUsbExportEnabled() ? 1 : 0,
                                       state.settings.M17_meta_text,
                                       state.settings.ppm_offset);
        }
        else
        {
            devConsoleQueueUsbResponse("brightness=%u", state.settings.brightness);
            devConsoleQueueUsbResponse("contrast=%u", state.settings.contrast);
            devConsoleQueueUsbResponse("sql=%u", state.settings.sqlLevel);
            devConsoleQueueUsbResponse("vox=%u", state.settings.voxLevel);
            devConsoleQueueUsbResponse("timezone=%d", state.settings.utc_timezone);
            devConsoleQueueUsbResponse("gps=%u", state.settings.gps_enabled ? 1 : 0);
            devConsoleQueueUsbResponse("gpssettime=%u", state.settings.gpsSetTime ? 1 : 0);
            devConsoleQueueUsbResponse("callsign=%s", state.settings.callsign);
            devConsoleQueueUsbResponse("displaytimer=%u", state.settings.display_timer);
            devConsoleQueueUsbResponse("vplevel=%u", state.settings.vpLevel);
            devConsoleQueueUsbResponse("vpphonetic=%u", state.settings.vpPhoneticSpell ? 1 : 0);
            devConsoleQueueUsbResponse("macrolatch=%u", state.settings.macroMenuLatch ? 1 : 0);
            devConsoleQueueUsbResponse("powerprofile=%u", state.settings.powerProfile);
            devConsoleQueueUsbResponse("theme=%u", state.settings.theme);
            devConsoleQueueUsbResponse("bandplan=%u", state.settings.bandplan);
            devConsoleQueueUsbResponse("showbatteryicon=%u", state.settings.showBatteryIcon ? 1 : 0);
            devConsoleQueueUsbResponse("m17can=%u", state.settings.m17_can);
            devConsoleQueueUsbResponse("m17canrx=%u", state.settings.m17_can_rx ? 1 : 0);
            devConsoleQueueUsbResponse("m17dest=%s", state.settings.m17_dest);
            devConsoleQueueUsbResponse("m17enc=%u", state.settings.m17_default_encryption);
            devConsoleQueueUsbResponse("m17encsub=%u", state.settings.m17_default_enc_subtype);
            devConsoleQueueUsbResponse("m17keyindex=%u", state.settings.m17_default_key_index);
            devConsoleQueueUsbResponse("m17key1=%s", state.settings.m17_keys[0]);
            devConsoleQueueUsbResponse("m17key2=%s", state.settings.m17_keys[1]);
            devConsoleQueueUsbResponse("m17key3=%s", state.settings.m17_keys[2]);
            devConsoleQueueUsbResponse("m17key4=%s", state.settings.m17_keys[3]);
            devConsoleQueueUsbResponse("usbLog=%u", devConsole_getUsbExportEnabled() ? 1 : 0);
            devConsoleQueueUsbResponse("metatext=%s", state.settings.M17_meta_text);
            devConsoleQueueUsbResponse("ppm=%d", state.settings.ppm_offset);
        }
        return;
    }

    if(strcmp(key, "brightness") == 0)
        devConsoleQueueUsbResponse("brightness=%u", state.settings.brightness);
    else if(strcmp(key, "contrast") == 0)
        devConsoleQueueUsbResponse("contrast=%u", state.settings.contrast);
    else if(strcmp(key, "sql") == 0)
        devConsoleQueueUsbResponse("sql=%u", state.settings.sqlLevel);
    else if(strcmp(key, "vox") == 0)
        devConsoleQueueUsbResponse("vox=%u", state.settings.voxLevel);
    else if(strcmp(key, "timezone") == 0)
        devConsoleQueueUsbResponse("timezone=%d", state.settings.utc_timezone);
    else if(strcmp(key, "gps") == 0)
        devConsoleQueueUsbResponse("gps=%u", state.settings.gps_enabled ? 1 : 0);
    else if(strcmp(key, "gpssettime") == 0)
        devConsoleQueueUsbResponse("gpssettime=%u", state.settings.gpsSetTime ? 1 : 0);
    else if(strcmp(key, "callsign") == 0)
        devConsoleQueueUsbResponse("callsign=%s", state.settings.callsign);
    else if(strcmp(key, "displaytimer") == 0)
        devConsoleQueueUsbResponse("displaytimer=%u", state.settings.display_timer);
    else if(strcmp(key, "vplevel") == 0)
        devConsoleQueueUsbResponse("vplevel=%u", state.settings.vpLevel);
    else if(strcmp(key, "vpphonetic") == 0)
        devConsoleQueueUsbResponse("vpphonetic=%u", state.settings.vpPhoneticSpell ? 1 : 0);
    else if(strcmp(key, "macrolatch") == 0)
        devConsoleQueueUsbResponse("macrolatch=%u", state.settings.macroMenuLatch ? 1 : 0);
    else if(strcmp(key, "powerprofile") == 0)
        devConsoleQueueUsbResponse("powerprofile=%u", state.settings.powerProfile);
    else if(strcmp(key, "theme") == 0)
        devConsoleQueueUsbResponse("theme=%u", state.settings.theme);
    else if(strcmp(key, "bandplan") == 0)
        devConsoleQueueUsbResponse("bandplan=%u", state.settings.bandplan);
    else if(strcmp(key, "showbatteryicon") == 0)
        devConsoleQueueUsbResponse("showbatteryicon=%u", state.settings.showBatteryIcon ? 1 : 0);
    else if(strcmp(key, "m17can") == 0)
        devConsoleQueueUsbResponse("m17can=%u", state.settings.m17_can);
    else if(strcmp(key, "m17canrx") == 0)
        devConsoleQueueUsbResponse("m17canrx=%u", state.settings.m17_can_rx ? 1 : 0);
    else if(strcmp(key, "m17dest") == 0)
        devConsoleQueueUsbResponse("m17dest=%s", state.settings.m17_dest);
    else if(strcmp(key, "m17enc") == 0)
        devConsoleQueueUsbResponse("m17enc=%u", state.settings.m17_default_encryption);
    else if(strcmp(key, "m17encsub") == 0)
        devConsoleQueueUsbResponse("m17encsub=%u", state.settings.m17_default_enc_subtype);
    else if(strcmp(key, "m17keyindex") == 0)
        devConsoleQueueUsbResponse("m17keyindex=%u", state.settings.m17_default_key_index);
    else if(strcmp(key, "m17key1") == 0)
        devConsoleQueueUsbResponse("m17key1=%s", state.settings.m17_keys[0]);
    else if(strcmp(key, "m17key2") == 0)
        devConsoleQueueUsbResponse("m17key2=%s", state.settings.m17_keys[1]);
    else if(strcmp(key, "m17key3") == 0)
        devConsoleQueueUsbResponse("m17key3=%s", state.settings.m17_keys[2]);
    else if(strcmp(key, "m17key4") == 0)
        devConsoleQueueUsbResponse("m17key4=%s", state.settings.m17_keys[3]);
    else if(strcmp(key, "usbLog") == 0)
        devConsoleQueueUsbResponse("usbLog=%u", devConsole_getUsbExportEnabled() ? 1 : 0);
    else if(strcmp(key, "metatext") == 0)
        devConsoleQueueUsbResponse("metatext=%s", state.settings.M17_meta_text);
    else if(strcmp(key, "ppm") == 0)
        devConsoleQueueUsbResponse("ppm=%d", state.settings.ppm_offset);
    else
        devConsoleQueueUsbResponse("ERR unknown setting %s", key);
}

static void devConsoleSetSetting(const char *key, const char *value)
{
    long parsed;
    char *end = NULL;

    if(!devConsoleWritesUnlocked())
    {
        devConsoleQueueUsbResponse("ERR write locked");
        return;
    }

    if((key == NULL) || (value == NULL))
    {
        devConsoleQueueUsbResponse("ERR bad args");
        return;
    }

    parsed = strtol(value, &end, 10);

    if(strcmp(key, "brightness") == 0 && end != value && parsed >= 5 && parsed <= 100)
        state.settings.brightness = parsed;
    else if(strcmp(key, "contrast") == 0 && end != value && parsed >= 0 && parsed <= 255)
        state.settings.contrast = parsed;
    else if(strcmp(key, "sql") == 0 && end != value && parsed >= 0 && parsed <= 15)
        state.settings.sqlLevel = parsed;
    else if(strcmp(key, "vox") == 0 && end != value && parsed >= 0 && parsed <= 15)
        state.settings.voxLevel = parsed;
    else if(strcmp(key, "timezone") == 0 && end != value && parsed >= -24 && parsed <= 28)
        state.settings.utc_timezone = parsed;
    else if(strcmp(key, "gps") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.gps_enabled = parsed;
    else if(strcmp(key, "gpssettime") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.gpsSetTime = parsed;
    else if(strcmp(key, "displaytimer") == 0 && end != value && parsed >= 0 && parsed <= 15)
        state.settings.display_timer = parsed;
    else if(strcmp(key, "vplevel") == 0 && end != value && parsed >= 0 && parsed <= 7)
        state.settings.vpLevel = parsed;
    else if(strcmp(key, "vpphonetic") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.vpPhoneticSpell = parsed;
    else if(strcmp(key, "macrolatch") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.macroMenuLatch = parsed;
    else if(strcmp(key, "powerprofile") == 0 && end != value && parsed >= 0 && parsed <= 3)
        state.settings.powerProfile = parsed;
    else if(strcmp(key, "theme") == 0 && end != value && parsed >= 0 && parsed <= 255)
        state.settings.theme = parsed;
    else if(strcmp(key, "bandplan") == 0 && end != value && bandplanIsValid((uint8_t) parsed))
        state.settings.bandplan = parsed;
    else if(strcmp(key, "showbatteryicon") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.showBatteryIcon = parsed;
    else if(strcmp(key, "m17can") == 0 && end != value && parsed >= 0 && parsed <= 15)
        state.settings.m17_can = parsed;
    else if(strcmp(key, "m17canrx") == 0 && end != value && (parsed == 0 || parsed == 1))
        state.settings.m17_can_rx = parsed;
    else if(strcmp(key, "m17enc") == 0 && end != value && parsed >= 0 && parsed <= 2)
        state.settings.m17_default_encryption = parsed;
    else if(strcmp(key, "m17encsub") == 0 && end != value && parsed >= 0 && parsed <= 255)
        state.settings.m17_default_enc_subtype = parsed;
    else if(strcmp(key, "m17keyindex") == 0 && end != value && parsed >= 0 && parsed <= 4)
        state.settings.m17_default_key_index = parsed;
    else if(strcmp(key, "usbLog") == 0 && end != value && (parsed == 0 || parsed == 1))
    {
        devConsole_setUsbExportEnabled(parsed == 1);
        state.settings.usbLogExport = parsed == 1;
    }
    else if(strcmp(key, "callsign") == 0)
        snprintf(state.settings.callsign, sizeof(state.settings.callsign), "%s", value);
    else if(strcmp(key, "m17dest") == 0)
        snprintf(state.settings.m17_dest, sizeof(state.settings.m17_dest), "%s", value);
    else if(strcmp(key, "metatext") == 0)
        snprintf(state.settings.M17_meta_text, sizeof(state.settings.M17_meta_text), "%s", value);
    else if(strcmp(key, "m17key1") == 0)
        snprintf(state.settings.m17_keys[0], sizeof(state.settings.m17_keys[0]), "%s", value);
    else if(strcmp(key, "m17key2") == 0)
        snprintf(state.settings.m17_keys[1], sizeof(state.settings.m17_keys[1]), "%s", value);
    else if(strcmp(key, "m17key3") == 0)
        snprintf(state.settings.m17_keys[2], sizeof(state.settings.m17_keys[2]), "%s", value);
    else if(strcmp(key, "m17key4") == 0)
        snprintf(state.settings.m17_keys[3], sizeof(state.settings.m17_keys[3]), "%s", value);
    else if(strcmp(key, "ppm") == 0 && end != value && parsed >= -32768 && parsed <= 32767)
        state.settings.ppm_offset = parsed;
    else
    {
        devConsoleQueueUsbResponse("ERR bad setting/value");
        return;
    }

    state_saveSettings();
    state.rtx_sync_pending = true;
    devConsoleQueueUsbResponse("ok");
}

static void devConsoleProcessDump(void)
{
    channel_t channel;
    contact_t contact;
    bankHdr_t bank;

    if((devConsoleDumpType == DEV_DUMP_NONE) || (devConsoleUsbCount != 0))
        return;

    if(devConsoleDumpIndex >= devConsoleDumpEnd)
    {
        if(devConsoleDumpDone == false)
        {
            devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                       "{\"type\":\"dump_end\"}" :
                                       "ok dump complete");
            devConsoleDumpDone = true;
            return;
        }

        devConsoleStopDump();
        return;
    }

    switch(devConsoleDumpType)
    {
        case DEV_DUMP_CPS_CHANNELS:
            if(cps_readChannel(&channel, devConsoleDumpIndex) == 0)
            {
                if(devConsoleJsonMode)
                    devConsoleReplyChannelJson(devConsoleDumpIndex, &channel);
                else
                    devConsoleQueueUsbResponse("ch=%u name=%s mode=%s rx=%u tx=%u bw=%u pwr=%u",
                                               (unsigned int) devConsoleDumpIndex,
                                               channel.name, devConsoleModeName(channel.mode),
                                               channel.rx_frequency, channel.tx_frequency,
                                               channel.bandwidth, channel.power);
            }
            break;
        case DEV_DUMP_CPS_CONTACTS:
            if(cps_readContact(&contact, devConsoleDumpIndex) == 0)
            {
                if(devConsoleJsonMode)
                    devConsoleReplyContactJson(devConsoleDumpIndex, &contact);
                else
                    devConsoleQueueUsbResponse("contact=%u name=%s mode=%s",
                                               (unsigned int) devConsoleDumpIndex,
                                               contact.name, devConsoleModeName(contact.mode));
            }
            break;
        case DEV_DUMP_CPS_BANKS:
            if(cps_readBankHeader(&bank, devConsoleDumpIndex) == 0)
            {
                if(devConsoleJsonMode)
                    devConsoleReplyBankJson(devConsoleDumpIndex, &bank);
                else
                    devConsoleQueueUsbResponse("bank=%u name=%s count=%u",
                                               (unsigned int) devConsoleDumpIndex,
                                               bank.name, bank.ch_count);
            }
            break;
        default:
            break;
    }

    devConsoleDumpIndex++;
}

static void devConsoleQueueUsbLine(const char *line)
{
    size_t lineLen;

    if((line == NULL) || (line[0] == '\0'))
        return;

    lineLen = strlen(line);

    if(devConsoleUsbCount == DEV_CONSOLE_USB_QUEUE_LEN)
    {
        devConsoleUsbRead = (devConsoleUsbRead + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
        devConsoleUsbCount--;
    }

    snprintf(devConsoleUsbQueue[devConsoleUsbWrite],
             sizeof(devConsoleUsbQueue[devConsoleUsbWrite]),
             ((lineLen >= 1) && (line[lineLen - 1] == '\n')) ? "%s" : "%s\r\n",
             line);
    devConsoleUsbWrite = (devConsoleUsbWrite + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
    devConsoleUsbCount++;
}

static void devConsoleQueueUsbResponse(const char *fmt, ...)
{
    char line[DEV_CONSOLE_USB_LINE_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    pthread_mutex_lock(&devConsoleMutex);
    devConsoleQueueUsbLine(line);
    pthread_mutex_unlock(&devConsoleMutex);
}

static void devConsoleHandleCommand(const char *command)
{
    char line[DEV_CONSOLE_LINE_LEN];
    char field[16];
    char value[40];
    char text[40];
    int32_t coord_e4;
    rtxStatus_t rtx;
    unsigned int tailCount = 0;
    unsigned int index = 0;
    unsigned int index2 = 0;
    size_t count = 0;
    channel_t channel;
    contact_t contact;
    bankHdr_t bank;
    uint8_t mode = OPMODE_NONE;

    if((command == NULL) || (command[0] == '\0'))
        return;

    if(strcmp(command, "help") == 0)
    {
        devConsoleQueueUsbResponse("Commands: help status version battery gps m17 clear");
        devConsoleQueueUsbResponse("          log on log off logs tail <n> reboot cps unlock");
        devConsoleQueueUsbResponse("          json on|off cps info|get|list|members settings get all");
        devConsoleQueueUsbResponse("          settings get <key> | settings set <key> <val>");
        return;
    }

    if(strcmp(command, "json on") == 0)
    {
        devConsoleJsonMode = true;
        devConsoleQueueUsbResponse("ok");
        return;
    }

    if(strcmp(command, "json off") == 0)
    {
        devConsoleJsonMode = false;
        devConsoleQueueUsbResponse("ok");
        return;
    }

    if(strcmp(command, "status") == 0)
    {
        rtx = rtx_getCurrentStatus();
        devConsoleQueueUsbResponse("state=%u mode=%u op=%u rx=%u tx=%u rssi=%d",
                                   state.devStatus, rtx.opMode, rtx.opStatus,
                                   rtx.rxFrequency, rtx.txFrequency, state.rssi);
        devConsoleQueueUsbResponse("battery=%umV charge=%u gps=%u usbLog=%u",
                                   state.v_bat, state.charge,
                                   state.settings.gps_enabled ? 1 : 0,
                                   devConsole_getUsbExportEnabled() ? 1 : 0);
        return;
    }

    if(strcmp(command, "version") == 0)
    {
        devConsoleQueueUsbResponse("OpenRTX %s", GIT_VERSION);
        return;
    }

    if(strcmp(command, "battery") == 0)
    {
        devConsoleQueueUsbResponse("battery=%umV charge=%u volume=%u rssi=%d",
                                   state.v_bat, state.charge, state.volume, state.rssi);
        return;
    }

    if(strcmp(command, "gps") == 0)
    {
        devConsoleQueueUsbResponse("gps=%u detected=%u fixq=%u fixtype=%u sats=%u/%u",
                                   state.settings.gps_enabled ? 1 : 0,
                                   state.gpsDetected ? 1 : 0,
                                   state.gps_data.fix_quality,
                                   state.gps_data.fix_type,
                                   state.gps_data.satellites_tracked,
                                   state.gps_data.satellites_in_view);
        devConsoleQueueUsbResponse("lat=%d lon=%d alt=%d speed=%u hdop=%u",
                                   state.gps_data.latitude,
                                   state.gps_data.longitude,
                                   state.gps_data.altitude,
                                   state.gps_data.speed,
                                   state.gps_data.hdop);
        return;
    }

    if(strcmp(command, "m17") == 0)
    {
        rtx = rtx_getCurrentStatus();
        devConsoleQueueUsbResponse("mode=%u op=%u lsf=%u src=%s dst=%s",
                                   rtx.opMode, rtx.opStatus, rtx.lsfOk ? 1 : 0,
                                   rtx.M17_src, rtx.M17_dst);
        devConsoleQueueUsbResponse("link=%s refl=%s route=%s can tx=%u rx=%u chk=%u",
                                   rtx.M17_link, rtx.M17_refl, rtx.destination_address,
                                   rtx.txCan, rtx.rxCan, rtx.canRxEn ? 1 : 0);
        return;
    }

    if(strcmp(command, "clear") == 0)
    {
        devConsole_clear();
        devConsole_log(DEVLOG_INFO, "USB", "Console cleared from USB");
        devConsoleQueueUsbResponse("ok");
        return;
    }

    if(strcmp(command, "log on") == 0)
    {
        devConsole_setUsbExportEnabled(true);
        state.settings.usbLogExport = true;
        devConsole_log(DEVLOG_INFO, "USB", "USB log export enabled");
        devConsoleQueueUsbResponse("ok");
        return;
    }

    if(strcmp(command, "log off") == 0)
    {
        devConsole_setUsbExportEnabled(false);
        state.settings.usbLogExport = false;
        devConsoleQueueUsbResponse("ok");
        devConsole_log(DEVLOG_INFO, "USB", "USB log export disabled");
        return;
    }

    if(sscanf(command, "logs tail %u", &tailCount) == 1)
    {
        size_t total = devConsole_getLineCount();
        size_t count = tailCount;
        size_t start;

        if(count == 0)
            count = 10;
        if(count > 20)
            count = 20;

        start = (total > count) ? (total - count) : 0;
        for(size_t i = start; i < total; i++)
        {
            if(devConsole_getLine(i, line, sizeof(line)))
                devConsoleQueueUsbResponse("%s", line);
        }
        devConsoleQueueUsbResponse("ok tail=%u", (unsigned int) (total - start));
        return;
    }

    if(strcmp(command, "reboot") == 0)
    {
        devConsoleQueueUsbResponse("rebooting");
        devConsole_log(DEVLOG_WARN, "USB", "Reboot requested from USB");
        state.devStatus = SHUTDOWN;
        return;
    }

    if(strcmp(command, "cps unlock") == 0)
    {
        devConsoleUnlockWrites();
        devConsoleQueueUsbResponse("ok writes enabled for %u sec",
                                   DEV_CONSOLE_WRITE_UNLOCK_MS / 1000);
        return;
    }

    if(strcmp(command, "cps info") == 0)
    {
        if(devConsoleJsonMode)
            devConsoleQueueUsbResponse("{\"type\":\"cps_info\",\"channels\":%u,\"contacts\":%u,\"banks\":%u,\"unlocked\":%u}",
                                       (unsigned int) devConsoleCountChannels(),
                                       (unsigned int) devConsoleCountContacts(),
                                       (unsigned int) devConsoleCountBanks(),
                                       devConsoleWritesUnlocked() ? 1 : 0);
        else
            devConsoleQueueUsbResponse("channels=%u contacts=%u banks=%u unlocked=%u",
                                       (unsigned int) devConsoleCountChannels(),
                                       (unsigned int) devConsoleCountContacts(),
                                       (unsigned int) devConsoleCountBanks(),
                                       devConsoleWritesUnlocked() ? 1 : 0);
        return;
    }

    if(sscanf(command, "cps channel list %u %u", &index, &index2) == 2)
    {
        if(index == 0)
            index = 1;
        if(index2 == 0)
            index2 = 10;
        devConsoleStartDump(DEV_DUMP_CPS_CHANNELS, index, index + index2);
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"channels\"}" :
                                   "ok channel list");
        return;
    }

    if(sscanf(command, "cps contact list %u %u", &index, &index2) == 2)
    {
        if(index == 0)
            index = 1;
        if(index2 == 0)
            index2 = 10;
        devConsoleStartDump(DEV_DUMP_CPS_CONTACTS, index, index + index2);
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"contacts\"}" :
                                   "ok contact list");
        return;
    }

    if(sscanf(command, "cps bank list %u %u", &index, &index2) == 2)
    {
        if(index2 == 0)
            index2 = 10;
        devConsoleStartDump(DEV_DUMP_CPS_BANKS, index, index + index2);
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"banks\"}" :
                                   "ok bank list");
        return;
    }

    if(strcmp(command, "cps channel get all") == 0)
    {
        devConsoleStartDump(DEV_DUMP_CPS_CHANNELS, 1, devConsoleCountChannels() + 1);
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"channels\"}" :
                                   "ok channel dump");
        return;
    }

    if(strcmp(command, "cps contact get all") == 0)
    {
        devConsoleStartDump(DEV_DUMP_CPS_CONTACTS, 1, devConsoleCountContacts() + 1);
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"contacts\"}" :
                                   "ok contact dump");
        return;
    }

    if(strcmp(command, "cps bank get all") == 0)
    {
        devConsoleStartDump(DEV_DUMP_CPS_BANKS, 0, devConsoleCountBanks());
        devConsoleQueueUsbResponse(devConsoleJsonMode ?
                                   "{\"type\":\"dump_start\",\"target\":\"banks\"}" :
                                   "ok bank dump");
        return;
    }

    if(sscanf(command, "cps channel get %u", &index) == 1)
    {
        devConsoleReplyChannel(index);
        return;
    }

    if(sscanf(command, "cps contact get %u", &index) == 1)
    {
        devConsoleReplyContact(index);
        return;
    }

    if(sscanf(command, "cps bank get %u", &index) == 1)
    {
        devConsoleReplyBank(index);
        return;
    }

    if(sscanf(command, "cps bank members %u", &index) == 1)
    {
        devConsoleReplyBankMembers(index);
        return;
    }

    if(sscanf(command, "cps channel create %39[^\n]", text) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }

        channel = cps_getDefaultChannel();
        snprintf(channel.name, sizeof(channel.name), "%s", text);
        count = devConsoleCountChannels();
        if(cps_insertChannel(channel, count) == 0)
        {
            devConsoleQueueUsbResponse("ok created channel %u", (unsigned int) (count + 1));
            devConsole_log(DEVLOG_INFO, "USB", "USB created CH%u %s", (unsigned int) (count + 1), channel.name);
            state.rtx_sync_pending = true;
        }
        else
            devConsoleQueueUsbResponse("ERR create failed");
        return;
    }

    if(sscanf(command, "cps contact create %39[^\n]", text) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }

        memset(&contact, 0, sizeof(contact));
        contact.mode = OPMODE_M17;
        snprintf(contact.name, sizeof(contact.name), "%s", text);
        count = devConsoleCountContacts();
        if(cps_insertContact(contact, count + 1) == 0)
            devConsoleQueueUsbResponse("ok created contact %u", (unsigned int) (count + 1));
        else
            devConsoleQueueUsbResponse("ERR create failed");
        return;
    }

    if(sscanf(command, "cps bank create %39[^\n]", text) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }

        memset(&bank, 0, sizeof(bank));
        snprintf(bank.name, sizeof(bank.name), "%s", text);
        count = devConsoleCountBanks();
        if(cps_insertBankHeader(bank, count) == 0)
            devConsoleQueueUsbResponse("ok created bank %u", (unsigned int) count);
        else
            devConsoleQueueUsbResponse("ERR create failed");
        return;
    }

    if(sscanf(command, "cps channel delete %u confirm", &index) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_readChannel(&channel, index) != 0)
        {
            devConsoleQueueUsbResponse("ERR no channel %u", index);
            return;
        }
        if(cps_deleteChannel(channel, index - 1) == 0)
        {
            devConsoleQueueUsbResponse("ok deleted channel %u", index);
            devConsole_log(DEVLOG_WARN, "USB", "USB deleted CH%u %s", index, channel.name);
            state.rtx_sync_pending = true;
        }
        else
            devConsoleQueueUsbResponse("ERR delete failed");
        return;
    }

    if(sscanf(command, "cps contact delete %u confirm", &index) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_deleteContact(index) == 0)
            devConsoleQueueUsbResponse("ok deleted contact %u", index);
        else
            devConsoleQueueUsbResponse("ERR delete failed");
        return;
    }

    if(sscanf(command, "cps bank delete %u confirm", &index) == 1)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_deleteBankHeader(index) == 0)
            devConsoleQueueUsbResponse("ok deleted bank %u", index);
        else
            devConsoleQueueUsbResponse("ERR delete failed");
        return;
    }

    if(sscanf(command, "cps channel set %u %15s %39[^\n]", &index, field, value) == 3)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_readChannel(&channel, index) != 0)
        {
            devConsoleQueueUsbResponse("ERR no channel %u", index);
            return;
        }

        if(strcmp(field, "name") == 0)
            snprintf(channel.name, sizeof(channel.name), "%s", value);
        else if(strcmp(field, "rx") == 0)
        {
            freq_t freq = strtoul(value, NULL, 10);
            if(!bandplanIsFrequencyAllowed(platform_getHwInfo(),
                                           (bandplan_t) state.settings.bandplan,
                                           freq))
            {
                devConsoleQueueUsbResponse("ERR bad rx frequency");
                return;
            }

            channel.rx_frequency = freq;
        }
        else if(strcmp(field, "tx") == 0)
        {
            freq_t freq = strtoul(value, NULL, 10);
            if(!bandplanIsFrequencyAllowed(platform_getHwInfo(),
                                           (bandplan_t) state.settings.bandplan,
                                           freq))
            {
                devConsoleQueueUsbResponse("ERR bad tx frequency");
                return;
            }

            channel.tx_frequency = freq;
        }
        else if(strcmp(field, "mode") == 0 && devConsoleParseMode(value, &mode))
            channel.mode = mode;
        else if((strcmp(field, "bandwidth") == 0) || (strcmp(field, "bw") == 0))
        {
            if((strcmp(value, "12.5") == 0) || (strcmp(value, "12") == 0) || (strcmp(value, "0") == 0))
                channel.bandwidth = BW_12_5;
            else if((strcmp(value, "25") == 0) || (strcmp(value, "1") == 0))
                channel.bandwidth = BW_25;
            else
            {
                devConsoleQueueUsbResponse("ERR bad bandwidth");
                return;
            }
        }
        else if(strcmp(field, "power") == 0)
            channel.power = strtoul(value, NULL, 10);
        else if(strcmp(field, "scanlist") == 0)
            channel.scanList_index = strtoul(value, NULL, 10);
        else if(strcmp(field, "zone") == 0)
        {
            if(devConsoleSetChannelZone(index, strtol(value, NULL, 10)) != 0)
            {
                devConsoleQueueUsbResponse("ERR bad zone");
                return;
            }
        }
        else if(strcmp(field, "lat") == 0)
        {
            int32_t lon_e4 = 0;
            devConsoleGetChannelLocationE4(&channel, NULL, &lon_e4);
            if(!devConsoleParseSignedE4(value, &coord_e4) || !devConsoleSetChannelLocationE4(&channel, coord_e4, lon_e4))
            {
                devConsoleQueueUsbResponse("ERR bad latitude");
                return;
            }
        }
        else if(strcmp(field, "lon") == 0)
        {
            int32_t lat_e4 = 0;
            devConsoleGetChannelLocationE4(&channel, &lat_e4, NULL);
            if(!devConsoleParseSignedE4(value, &coord_e4) || !devConsoleSetChannelLocationE4(&channel, lat_e4, coord_e4))
            {
                devConsoleQueueUsbResponse("ERR bad longitude");
                return;
            }
        }
        else if(strcmp(field, "clearlocation") == 0)
        {
            memset(&channel.ch_location, 0, sizeof(channel.ch_location));
        }
        else
        {
            devConsoleQueueUsbResponse("ERR bad channel field");
            return;
        }

        if(cps_writeChannel(channel, index - 1) == 0)
        {
            devConsoleQueueUsbResponse("ok");
            state.rtx_sync_pending = true;
        }
        else
            devConsoleQueueUsbResponse("ERR write failed");
        return;
    }

    if(sscanf(command, "cps contact set %u %15s %39[^\n]", &index, field, value) == 3)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_readContact(&contact, index) != 0)
        {
            devConsoleQueueUsbResponse("ERR no contact %u", index);
            return;
        }
        if(strcmp(field, "name") == 0)
            snprintf(contact.name, sizeof(contact.name), "%s", value);
        else if(strcmp(field, "mode") == 0 && devConsoleParseMode(value, &mode))
            contact.mode = mode;
        else if(strcmp(field, "dmrid") == 0)
            contact.info.dmr.id = strtoul(value, NULL, 10);
        else if(strcmp(field, "dmrtype") == 0)
            contact.info.dmr.contactType = strtoul(value, NULL, 10) & 0x03;
        else if(strcmp(field, "dmrrxtone") == 0)
            contact.info.dmr.rx_tone = strtoul(value, NULL, 10) ? 1 : 0;
        else if(strcmp(field, "m17addr") == 0)
        {
            if(strlen(value) != 12)
            {
                devConsoleQueueUsbResponse("ERR bad m17addr");
                return;
            }
            for(size_t i = 0; i < 6; i++)
            {
                char byteStr[3] = { value[i * 2], value[i * 2 + 1], '\0' };
                contact.info.m17.address[i] = (uint8_t) strtoul(byteStr, NULL, 16);
            }
        }
        else
        {
            devConsoleQueueUsbResponse("ERR bad contact field");
            return;
        }
        if(cps_writeContact(contact, index) == 0)
            devConsoleQueueUsbResponse("ok");
        else
            devConsoleQueueUsbResponse("ERR write failed");
        return;
    }

    if(sscanf(command, "cps bank set %u %15s %39[^\n]", &index, field, value) == 3)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_readBankHeader(&bank, index) != 0)
        {
            devConsoleQueueUsbResponse("ERR no bank %u", index);
            return;
        }
        if(strcmp(field, "name") == 0)
            snprintf(bank.name, sizeof(bank.name), "%s", value);
        else
        {
            devConsoleQueueUsbResponse("ERR bad bank field");
            return;
        }
        if(cps_writeBankHeader(bank, index) == 0)
            devConsoleQueueUsbResponse("ok");
        else
            devConsoleQueueUsbResponse("ERR write failed");
        return;
    }

    if(sscanf(command, "cps bank add %u %u", &index, &index2) == 2)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_readBankHeader(&bank, index) != 0)
        {
            devConsoleQueueUsbResponse("ERR no bank %u", index);
            return;
        }
        if(cps_insertBankData(index2 - 1, index, bank.ch_count) == 0)
            devConsoleQueueUsbResponse("ok");
        else
            devConsoleQueueUsbResponse("ERR add failed");
        return;
    }

    if(sscanf(command, "cps bank remove %u %u", &index, &index2) == 2)
    {
        if(!devConsoleWritesUnlocked())
        {
            devConsoleQueueUsbResponse("ERR write locked");
            return;
        }
        if(cps_deleteBankData(index, index2) == 0)
            devConsoleQueueUsbResponse("ok");
        else
            devConsoleQueueUsbResponse("ERR remove failed");
        return;
    }

    if(sscanf(command, "settings get %15s", field) == 1)
    {
        devConsoleReplySetting(field);
        return;
    }

    if(sscanf(command, "settings set %15s %39[^\n]", field, value) == 2)
    {
        devConsoleSetSetting(field, value);
        return;
    }

    devConsoleQueueUsbResponse("unknown command: %s", command);
}

static void devConsoleProcessUsbCommands(void)
{
    char rxBuf[32];
    ssize_t rxLen;

    if(vcom_isConnected() == false)
    {
        devConsoleCmdLen = 0;
        return;
    }

    rxLen = vcom_readBlock(rxBuf, sizeof(rxBuf));
    for(ssize_t i = 0; i < rxLen; i++)
    {
        char c = rxBuf[i];

        if((c == '\r') || (c == '\n'))
        {
            if(devConsoleCmdLen > 0)
            {
                devConsoleCmdBuf[devConsoleCmdLen] = '\0';
                devConsoleHandleCommand(devConsoleCmdBuf);
                devConsoleCmdLen = 0;
            }
            continue;
        }

        if((c == '\b') || (c == 0x7f))
        {
            if(devConsoleCmdLen > 0)
                devConsoleCmdLen--;
            continue;
        }

        if((c < 32) || (c > 126))
            continue;

        if(devConsoleCmdLen < (DEV_CONSOLE_CMD_BUF_LEN - 1))
            devConsoleCmdBuf[devConsoleCmdLen++] = c;
    }
}

static size_t devConsoleWrappedRows(const char *line, size_t wrapWidth)
{
    size_t lineLen;

    if((line == NULL) || (wrapWidth == 0))
        return 0;

    lineLen = strlen(line);
    if(lineLen == 0)
        return 1;

    return ((lineLen - 1) / wrapWidth) + 1;
}

static const char *devConsoleLevelName(devLogLevel_t level)
{
    switch(level)
    {
        case DEVLOG_ERROR: return "ERR";
        case DEVLOG_WARN:  return "WRN";
        case DEVLOG_INFO:  return "INF";
        case DEVLOG_DEBUG: return "DBG";
        default:           return "LOG";
    }
}

void devConsole_init(void)
{
    devConsoleStart = 0;
    devConsoleCount = 0;
    devConsoleUsbRead = 0;
    devConsoleUsbWrite = 0;
    devConsoleUsbCount = 0;
    devConsoleUsbExport = false;
    devConsoleCmdLen = 0;
    devConsoleReady = true;
}

void devConsole_setUsbExportEnabled(bool enabled)
{
    pthread_mutex_lock(&devConsoleMutex);
    devConsoleUsbExport = enabled;
    pthread_mutex_unlock(&devConsoleMutex);
}

bool devConsole_getUsbExportEnabled(void)
{
    bool enabled;

    pthread_mutex_lock(&devConsoleMutex);
    enabled = devConsoleUsbExport;
    pthread_mutex_unlock(&devConsoleMutex);

    return enabled;
}

void devConsole_clear(void)
{
    pthread_mutex_lock(&devConsoleMutex);
    devConsoleStart = 0;
    devConsoleCount = 0;
    devConsoleUsbRead = 0;
    devConsoleUsbWrite = 0;
    devConsoleUsbCount = 0;
    devConsoleCmdLen = 0;
    pthread_mutex_unlock(&devConsoleMutex);
}

void devConsole_log(devLogLevel_t level, const char *tag, const char *fmt, ...)
{
    long long tick;
    datetime_t now;
    datetime_t localTime;
    char message[DEV_CONSOLE_LINE_LEN];
    char line[DEV_CONSOLE_LINE_LEN];
    char usbLine[DEV_CONSOLE_USB_LINE_LEN + 2];
    size_t prefixLen;
    bool haveDateTime = false;

    if(!devConsoleReady)
        devConsole_init();

    tick = getTick();
    now = platform_getCurrentTime();

    if((now.year >= 24) && (now.month >= 1) && (now.month <= 12) &&
       (now.date >= 1) && (now.date <= 31) &&
       (now.hour >= 0) && (now.hour <= 23) &&
       (now.minute >= 0) && (now.minute <= 59) &&
       (now.second >= 0) && (now.second <= 59))
    {
        localTime = utcToLocalTime(now, state.settings.utc_timezone);
        haveDateTime = true;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    snprintf(line, sizeof(line), "%s %.6s:",
             devConsoleLevelName(level),
             (tag != NULL) ? tag : "SYS");
    strncat(line, " ", sizeof(line) - strlen(line) - 1);
    strncat(line, message, sizeof(line) - strlen(line) - 1);

    pthread_mutex_lock(&devConsoleMutex);

    size_t writeIndex = (devConsoleStart + devConsoleCount) % DEV_CONSOLE_MAX_LINES;
    if(devConsoleCount == DEV_CONSOLE_MAX_LINES)
    {
        writeIndex = devConsoleStart;
        devConsoleStart = (devConsoleStart + 1) % DEV_CONSOLE_MAX_LINES;
    }
    else
    {
        devConsoleCount++;
    }

    strncpy(devConsoleLines[writeIndex], line, DEV_CONSOLE_LINE_LEN - 1);
    devConsoleLines[writeIndex][DEV_CONSOLE_LINE_LEN - 1] = '\0';

    if(devConsoleUsbExport)
    {
        if(haveDateTime)
        {
            prefixLen = snprintf(usbLine, sizeof(usbLine), "20%02u-%02u-%02u %02d:%02d:%02d ",
                                 (unsigned int) localTime.year,
                                 (unsigned int) localTime.month,
                                 (unsigned int) localTime.date,
                                 localTime.hour,
                                 localTime.minute,
                                 localTime.second);
            if(prefixLen < sizeof(usbLine))
                snprintf(usbLine + prefixLen, sizeof(usbLine) - prefixLen, "%s\r\n", line);
        }
        else
        {
            snprintf(usbLine, sizeof(usbLine), "%08lld %s\r\n", tick, line);
        }

        devConsoleQueueUsbLine(usbLine);
    }

    pthread_mutex_unlock(&devConsoleMutex);
}

#ifndef PLATFORM_LINUX
void devConsole_process(void)
{
    char usbLine[DEV_CONSOLE_USB_LINE_LEN + 2];

    devConsoleProcessUsbCommands();
    devConsoleProcessDump();

    if((devConsoleReady == false) || (vcom_isConnected() == false))
        return;

    pthread_mutex_lock(&devConsoleMutex);

    if(devConsoleUsbCount == 0)
    {
        pthread_mutex_unlock(&devConsoleMutex);
        return;
    }

    snprintf(usbLine, sizeof(usbLine), "%s", devConsoleUsbQueue[devConsoleUsbRead]);
    pthread_mutex_unlock(&devConsoleMutex);

    if(vcom_writeBlockNonblocking(usbLine, strlen(usbLine)) <= 0)
        return;

    pthread_mutex_lock(&devConsoleMutex);
    if(devConsoleUsbCount > 0)
    {
        devConsoleUsbRead = (devConsoleUsbRead + 1) % DEV_CONSOLE_USB_QUEUE_LEN;
        devConsoleUsbCount--;
    }
    pthread_mutex_unlock(&devConsoleMutex);
}
#else
void devConsole_process(void)
{
}
#endif

size_t devConsole_getLineCount(void)
{
    size_t count;

    pthread_mutex_lock(&devConsoleMutex);
    count = devConsoleCount;
    pthread_mutex_unlock(&devConsoleMutex);

    return count;
}

bool devConsole_getLine(size_t index, char *buf, size_t len)
{
    bool valid = false;

    if((buf == NULL) || (len == 0))
        return false;

    pthread_mutex_lock(&devConsoleMutex);

    if(index < devConsoleCount)
    {
        size_t realIndex = (devConsoleStart + index) % DEV_CONSOLE_MAX_LINES;
        snprintf(buf, len, "%s", devConsoleLines[realIndex]);
        valid = true;
    }

    pthread_mutex_unlock(&devConsoleMutex);

    return valid;
}

size_t devConsole_getDisplayRowCount(size_t wrapWidth)
{
    size_t rowCount = 0;

    if(wrapWidth == 0)
        return 0;

    pthread_mutex_lock(&devConsoleMutex);

    for(size_t i = 0; i < devConsoleCount; i++)
    {
        size_t realIndex = (devConsoleStart + i) % DEV_CONSOLE_MAX_LINES;
        rowCount += devConsoleWrappedRows(devConsoleLines[realIndex], wrapWidth);
    }

    pthread_mutex_unlock(&devConsoleMutex);

    return rowCount;
}

bool devConsole_getDisplayRow(size_t rowIndex, size_t wrapWidth, char *buf, size_t len)
{
    size_t cursor = 0;

    if((buf == NULL) || (len == 0) || (wrapWidth == 0))
        return false;

    pthread_mutex_lock(&devConsoleMutex);

    for(size_t i = 0; i < devConsoleCount; i++)
    {
        size_t realIndex = (devConsoleStart + i) % DEV_CONSOLE_MAX_LINES;
        const char *line = devConsoleLines[realIndex];
        size_t rows = devConsoleWrappedRows(line, wrapWidth);

        if(rowIndex < (cursor + rows))
        {
            size_t offset = (rowIndex - cursor) * wrapWidth;
            snprintf(buf, len, "%.*s", (int) wrapWidth, line + offset);
            pthread_mutex_unlock(&devConsoleMutex);
            return true;
        }

        cursor += rows;
    }

    pthread_mutex_unlock(&devConsoleMutex);
    return false;
}
