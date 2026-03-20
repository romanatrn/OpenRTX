/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wchar.h"
#include <string.h>
#include "core/nvmem_access.h"
#include "core/dev_console.h"
#include "core/nvmem_device.h"
#include "core/power.h"
#include "interfaces/cps_io.h"
#include "core/utils.h"
#include "cps_data_MDUV3x0.h"
#include "drivers/NVM/W25Qx.h"

extern const struct nvmDevice eflash;

static const uint32_t zoneBaseAddr       = 0x149E0;  /**< Base address of zones                                 */
static const uint32_t zoneExtBaseAddr    = 0x31000;  /**< Base address of zone extensions                       */
static const uint32_t chDataBaseAddr     = 0x110000; /**< Base address of channel data                          */
static const uint32_t contactBaseAddr    = 0x140000; /**< Base address of contacts                              */
static const uint32_t maxNumChannels     = 3000;     /**< Maximum number of channels in memory                  */
static const uint32_t maxNumZones        = 250;      /**< Maximum number of zones and zone extensions in memory */
static const uint32_t maxNumContacts     = 10000;    /**< Maximum number of contacts in memory                  */

static inline void W25Qx_readData(uint32_t addr, void *buf, size_t len)
{
    nvm_devRead(&eflash, addr, buf, len);
}

static inline int W25Qx_writeData(uint32_t addr, const void *buf, size_t len)
{
    return nvm_devWrite(&eflash, addr, buf, len);
}

static inline int W25Qx_eraseSector(uint32_t addr)
{
    return nvm_devErase(&eflash, addr, 0x1000);
}

static int _clearRange(uint32_t baseAddr, size_t len)
{
    static const uint8_t emptySector[0x1000] = {0};
    const uint32_t sectorSize = sizeof(emptySector);
    const uint32_t startAddr = baseAddr & ~(sectorSize - 1u);
    const uint32_t endAddr = (baseAddr + (uint32_t) len + sectorSize - 1u)
                           & ~(sectorSize - 1u);

    for(uint32_t addr = startAddr; addr < endAddr; addr += sectorSize)
    {
        if(W25Qx_eraseSector(addr) < 0)
            return -1;

        if(W25Qx_writeData(addr, emptySector, sectorSize) < 0)
            return -1;
    }

    return 0;
}

static uint32_t _binToBcd(uint32_t value)
{
    uint32_t bcd = 0;
    uint8_t shift = 0;

    while(value > 0)
    {
        bcd |= (value % 10) << shift;
        value /= 10;
        shift += 4;
    }

    return bcd;
}

static int _writeChannelAtAddress(const uint32_t addr, const void *buf, const size_t len)
{
    static uint8_t sector[0x1000];
    const uint32_t sectorAddr = addr & ~0x0FFFu;
    const uint32_t offset = addr - sectorAddr;

    if((offset + len) > sizeof(sector))
        return -1;

    if(nvm_devRead(&eflash, sectorAddr, sector, sizeof(sector)) < 0)
        return -1;

    memcpy(&sector[offset], buf, len);

    if(W25Qx_eraseSector(sectorAddr) < 0)
        return -1;

    if(W25Qx_writeData(sectorAddr, sector, sizeof(sector)) < 0)
        return -1;

    return 0;
}

static int _writeZoneAtAddress(const uint32_t addr, const void *buf, const size_t len)
{
    return _writeChannelAtAddress(addr, buf, len);
}

static int _readZoneMembers(uint16_t bank_pos, uint16_t members[64])
{
    mduv3x0Zone_t zoneData;
    mduv3x0ZoneExt_t zoneExtData;
    uint32_t zoneAddr = zoneBaseAddr + (bank_pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (bank_pos + 1) * sizeof(mduv3x0ZoneExt_t);

    if(bank_pos >= maxNumZones)
        return -1;

    W25Qx_readData(zoneAddr, ((uint8_t *) &zoneData), sizeof(zoneData));
    W25Qx_readData(zoneExtAddr, ((uint8_t *) &zoneExtData), sizeof(zoneExtData));

    #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if(wcslen((wchar_t *) zoneData.name) == 0)
        return -1;

    memcpy(&members[0], &zoneData.member_a[0], sizeof(zoneData.member_a));
    memcpy(&members[16], &zoneExtData.ext_a[0], sizeof(zoneExtData.ext_a));

    return 0;
}

static int _writeZoneMembers(uint16_t bank_pos, const uint16_t members[64])
{
    mduv3x0Zone_t zoneData;
    mduv3x0ZoneExt_t zoneExtData;
    uint32_t zoneAddr = zoneBaseAddr + (bank_pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (bank_pos + 1) * sizeof(mduv3x0ZoneExt_t);

    if(bank_pos >= maxNumZones)
        return -1;

    W25Qx_readData(zoneAddr, ((uint8_t *) &zoneData), sizeof(zoneData));
    W25Qx_readData(zoneExtAddr, ((uint8_t *) &zoneExtData), sizeof(zoneExtData));

    memcpy(&zoneData.member_a[0], &members[0], sizeof(zoneData.member_a));
    memcpy(&zoneExtData.ext_a[0], &members[16], sizeof(zoneExtData.ext_a));

    if(_writeZoneAtAddress(zoneAddr, &zoneData, sizeof(zoneData)) < 0)
        return -1;

    if(_writeZoneAtAddress(zoneExtAddr, &zoneExtData, sizeof(zoneExtData)) < 0)
        return -1;

    return 0;
}

static uint16_t _countZoneMembers(const uint16_t members[64])
{
    uint16_t count = 0;

    while((count < 64) && (members[count] != 0))
        count++;

    return count;
}

static void _clearAndCopyWideName(uint16_t *dst, size_t len, const char *src)
{
    memset(dst, 0, len * sizeof(uint16_t));

    for(size_t i = 0; (i < len) && (src[i] != '\0'); i++)
        dst[i] = (uint8_t) src[i];
}

static int _compactBankReferences(uint16_t deletedChannel, uint16_t movedChannel)
{
    for(uint16_t bank = 0; bank < maxNumZones; bank++)
    {
        uint16_t members[64];
        uint16_t compacted[64] = {0};
        uint16_t out = 0;

        if(_readZoneMembers(bank, members) < 0)
            continue;

        for(uint16_t i = 0; i < 64; i++)
        {
            uint16_t member = members[i];

            if(member == 0)
                continue;

            if(member == (deletedChannel + 1))
                continue;

            if(member == (movedChannel + 1))
                member = deletedChannel + 1;

            compacted[out++] = member;
        }

        if(_writeZoneMembers(bank, compacted) < 0)
            return -1;
    }

    return 0;
}

static int _channelToMemory(mduv3x0Channel_t *dst, const channel_t *src)
{
    memset(dst, 0, sizeof(*dst));

    if((src->mode != OPMODE_FM)
#if defined(CONFIG_M17)
       && (src->mode != OPMODE_M17)
#endif
       && (src->mode != OPMODE_DMR))
        return -1;

    dst->channel_mode = src->mode;
    dst->bandwidth = (src->bandwidth == BW_12_5) ? 0 : 1;
    dst->rx_only = src->rx_only;
    dst->scan_list_index = src->scanList_index;
    dst->group_list_index = src->groupList_index;
    dst->rx_frequency = _binToBcd(src->rx_frequency / 10);
    dst->tx_frequency = _binToBcd(src->tx_frequency / 10);

    uint32_t powerMilliWatt = powerGetDisplayMilliWatt(src->power, src->tx_frequency);

    if(powerMilliWatt <= 1000)
        dst->power = 0;
    else if(powerMilliWatt <= 2500)
        dst->power = 2;
    else
        dst->power = 3;

    for(uint16_t i = 0; i < 16; i++)
        dst->name[i] = (uint8_t) src->name[i];

    if(src->mode == OPMODE_FM)
    {
        if(src->fm.rxToneEn)
            dst->ctcss_dcs_receive = (uint16_t) _binToBcd(ctcss_tone[src->fm.rxTone]);

        if(src->fm.txToneEn)
            dst->ctcss_dcs_transmit = (uint16_t) _binToBcd(ctcss_tone[src->fm.txTone]);
    }
    else
    {
        dst->channel_mode = OPMODE_DMR;

#if defined(CONFIG_M17)
        if(src->mode == OPMODE_M17)
        {
            dst->contact_name_index = src->m17.contact_index;
            dst->repeater_slot = (src->m17.mode == 0) ? DIGITAL_VOICE
                                                      : (src->m17.mode & 0x03);
            dst->colorcode = src->m17.txCan & 0x0F;
        }
        else
#endif
        {
            dst->contact_name_index = src->dmr.contact_index;
            dst->repeater_slot = src->dmr.dmr_timeslot;
            dst->colorcode = src->dmr.txColorCode;
        }
    }

    return 0;
}

/**
 * Used to read channel data from SPI flash into a channel_t struct
 */
static int _readChannelAtAddress(channel_t *channel, uint32_t addr)
{
    mduv3x0Channel_t chData;
    W25Qx_readData(addr, ((uint8_t *) &chData), sizeof(mduv3x0Channel_t));

    // Check if the channel is empty
    #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if(wcslen((wchar_t *) chData.name) == 0) return -1;

    channel->mode            = chData.channel_mode;
    channel->bandwidth       = (chData.bandwidth == 0) ? 0 : 1;     // Consider 20kHz as 25kHz
    channel->rx_only         = chData.rx_only;
    channel->rx_frequency    = bcdToBin(chData.rx_frequency) * 10;
    channel->tx_frequency    = bcdToBin(chData.tx_frequency) * 10;
    channel->scanList_index  = chData.scan_list_index;
    channel->groupList_index = chData.group_list_index;

    if(chData.power == 3)
    {
        channel->power = powerNormalizeStoredValue(5000, POWER_PROFILE_20W_SWEEP);
    }
    else if(chData.power == 2)
    {
        channel->power = powerNormalizeStoredValue(2500, POWER_PROFILE_20W_SWEEP);
    }
    else
    {
        channel->power = powerNormalizeStoredValue(1000, POWER_PROFILE_20W_SWEEP);
    }

    /*
     * Brutally convert channel name from unicode to char by truncating the most
     * significant byte
     */
    for(uint16_t i = 0; i < 16; i++)
    {
        channel->name[i] = ((char) (chData.name[i] & 0x00FF));
    }

    /* Load mode-specific parameters */
    if(channel->mode == OPMODE_FM)
    {
        channel->fm.txToneEn = 0;
        channel->fm.rxToneEn = 0;
        uint16_t rx_css = chData.ctcss_dcs_receive;
        uint16_t tx_css = chData.ctcss_dcs_transmit;

        // TODO: Implement binary search to speed up this lookup
        if((rx_css != 0) && (rx_css != 0xFFFF))
        {
            for(int i = 0; i < CTCSS_FREQ_NUM; i++)
            {
                if(ctcss_tone[i] == ((uint16_t) bcdToBin(rx_css)))
                {
                    channel->fm.rxTone = i;
                    channel->fm.rxToneEn = 1;
                    break;
                }
            }
        }

        if((tx_css != 0) && (tx_css != 0xFFFF))
        {
            for(int i = 0; i < CTCSS_FREQ_NUM; i++)
            {
                if(ctcss_tone[i] == ((uint16_t) bcdToBin(tx_css)))
                {
                    channel->fm.txTone = i;
                    channel->fm.txToneEn = 1;
                    break;
                }
            }
        }

        // TODO: Implement warning screen if tone was not found
    }
    else if(channel->mode == OPMODE_DMR)
    {
#if defined(CONFIG_M17)
        channel->mode = OPMODE_M17;
        channel->m17.contact_index = chData.contact_name_index;
        channel->m17.rxCan = chData.colorcode & 0x0F;
        channel->m17.txCan = chData.colorcode & 0x0F;
        channel->m17.mode = (chData.repeater_slot == 0) ? DIGITAL_VOICE
                                                        : (chData.repeater_slot & 0x03);
        channel->m17.encr = PLAIN;
        channel->m17.gps_mode = NO_GPS;
#else
        channel->dmr.contact_index = chData.contact_name_index;
        channel->dmr.dmr_timeslot      = chData.repeater_slot;
        channel->dmr.rxColorCode       = chData.colorcode;
        channel->dmr.txColorCode       = chData.colorcode;
#endif
    }

    return 0;
}


/**
 * This function does not apply to address-based codeplugs
 */
int cps_open(char *cps_name)
{
    (void) cps_name;
    devConsole_log(DEVLOG_DEBUG, "CPS", "MD-UV3x0 CPS open");
    return 0;
}

/**
 * This function does not apply to address-based codeplugs
 */
void cps_close()
{
}

/**
 * This function does not apply to address-based codeplugs
 */
int cps_create(char *cps_name)
{
    (void) cps_name;

    devConsole_log(DEVLOG_WARN, "CPS", "Creating MD-UV3x0 CPS image");

    if(_clearRange(zoneBaseAddr, (maxNumZones + 1) * sizeof(mduv3x0Zone_t)) < 0)
        return -1;

    if(_clearRange(zoneExtBaseAddr, (maxNumZones + 1) * sizeof(mduv3x0ZoneExt_t)) < 0)
        return -1;

    if(_clearRange(chDataBaseAddr, (maxNumChannels + 1) * sizeof(mduv3x0Channel_t)) < 0)
        return -1;

    if(_clearRange(contactBaseAddr, (maxNumContacts + 1) * sizeof(mduv3x0Contact_t)) < 0)
        return -1;

    devConsole_log(DEVLOG_INFO, "CPS", "CPS image cleared");
    return 0;
}

int cps_readChannel(channel_t *channel, uint16_t pos)
{
    if(pos >= maxNumChannels) return -1;

    memset(channel, 0x00, sizeof(channel_t));

    // Note: pos is 1-based because an empty slot in a zone contains index 0
    uint32_t readAddr = chDataBaseAddr + pos * sizeof(mduv3x0Channel_t);
    return _readChannelAtAddress(channel, readAddr);
}

int cps_readBankHeader(bankHdr_t *b_header, uint16_t pos)
{
    if(pos >= maxNumZones) return -1;

    mduv3x0Zone_t zoneData;
    mduv3x0ZoneExt_t zoneExtData;
    uint16_t members[64] = {0};
    uint32_t zoneAddr = zoneBaseAddr + (pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (pos + 1) * sizeof(mduv3x0ZoneExt_t);
    W25Qx_readData(zoneAddr, ((uint8_t *) &zoneData), sizeof(mduv3x0Zone_t));
    W25Qx_readData(zoneExtAddr, ((uint8_t *) &zoneExtData), sizeof(mduv3x0ZoneExt_t));

    // Check if zone is empty
    #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if(wcslen((wchar_t *) zoneData.name) == 0) return -1;
    /*
     * Brutally convert channel name from unicode to char by truncating the most
     * significant byte
     */
    for(uint16_t i = 0; i < 16; i++)
    {
        b_header->name[i] = ((char) (zoneData.name[i] & 0x00FF));
    }

    memcpy(&members[0], &zoneData.member_a[0], sizeof(zoneData.member_a));
    memcpy(&members[16], &zoneExtData.ext_a[0], sizeof(zoneExtData.ext_a));
    b_header->ch_count = _countZoneMembers(members);

    return 0;
}

int cps_readBankData(uint16_t bank_pos, uint16_t ch_pos)
{
    if(bank_pos >= maxNumZones) return -1;

    mduv3x0Zone_t zoneData;
    mduv3x0ZoneExt_t zoneExtData;
    uint32_t zoneAddr = zoneBaseAddr + (bank_pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (bank_pos + 1) * sizeof(mduv3x0ZoneExt_t);
    W25Qx_readData(zoneAddr, ((uint8_t *) &zoneData), sizeof(mduv3x0Zone_t));
    W25Qx_readData(zoneExtAddr, ((uint8_t *) &zoneExtData), sizeof(mduv3x0ZoneExt_t));

    // Check if zone is empty
    #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if(wcslen((wchar_t *) zoneData.name) == 0) return -1;
    // Channel index in zones are 1 based because an empty zone contains 0s
    // OpenRTX CPS interface is 0-based so we decrease by one
    if (ch_pos < 16)
        return zoneData.member_a[ch_pos] - 1;
    else
        return zoneExtData.ext_a[ch_pos - 16] - 1;
}

int cps_readContact(contact_t *contact, uint16_t pos)
{
    if(pos >= maxNumContacts) return -1;

    mduv3x0Contact_t contactData;
    // Note: pos is 1-based to be consistent with channels
    uint32_t contactAddr = contactBaseAddr + pos * sizeof(mduv3x0Contact_t);
    W25Qx_readData(contactAddr, ((uint8_t *) &contactData), sizeof(mduv3x0Contact_t));

    // Check if contact is empty
    if(wcslen((wchar_t *) contactData.name) == 0) return -1;
    /*
     * Brutally convert channel name from unicode to char by truncating the most
     * significant byte
     */
    for(uint16_t i = 0; i < 16; i++)
    {
        contact->name[i] = ((char) (contactData.name[i] & 0x00FF));
    }

    contact->mode = OPMODE_DMR;
#if defined(CONFIG_M17)
    contact->mode = OPMODE_M17;
#endif

    // Copy contact DMR ID
    contact->info.dmr.id = contactData.id[0]
                         | (contactData.id[1] << 8)
                         | (contactData.id[2] << 16);

    // Copy contact details
    contact->info.dmr.contactType = contactData.type;
    contact->info.dmr.rx_tone     = contactData.receive_tone ? true : false;

    return 0;
}

int cps_writeChannel(channel_t channel, uint16_t pos)
{
    mduv3x0Channel_t chData;

    if(pos >= maxNumChannels)
        return -1;

    if(_channelToMemory(&chData, &channel) < 0)
        return -1;

    return _writeChannelAtAddress(chDataBaseAddr + (pos + 1) * sizeof(mduv3x0Channel_t),
                                  &chData, sizeof(chData));
}

int cps_insertChannel(channel_t channel, uint16_t pos)
{
    channel_t existing;

    if(pos >= maxNumChannels)
        return -1;

    if(cps_readChannel(&existing, pos + 1) == 0)
        return -1;

    return cps_writeChannel(channel, pos);
}

int cps_deleteChannel(channel_t channel, uint16_t pos)
{
    mduv3x0Channel_t lastData;
    mduv3x0Channel_t emptyData = {0};
    channel_t existing;
    uint16_t last = pos;

    (void) channel;

    if(pos >= maxNumChannels)
        return -1;

    if(cps_readChannel(&existing, pos + 1) < 0)
        return -1;

    while(((uint32_t) (last + 1)) < maxNumChannels)
    {
        channel_t next;

        if(cps_readChannel(&next, last + 2) < 0)
            break;

        last++;
    }

    if(pos != last)
    {
        W25Qx_readData(chDataBaseAddr + (last + 1) * sizeof(mduv3x0Channel_t),
                       &lastData, sizeof(lastData));

        if(_writeChannelAtAddress(chDataBaseAddr + (pos + 1) * sizeof(mduv3x0Channel_t),
                                  &lastData, sizeof(lastData)) < 0)
            return -1;
    }

    if(_writeChannelAtAddress(chDataBaseAddr + (last + 1) * sizeof(mduv3x0Channel_t),
                              &emptyData, sizeof(emptyData)) < 0)
        return -1;

    if(_compactBankReferences(pos, last) < 0)
        return -1;

    return 0;
}

int cps_writeContact(contact_t contact, uint16_t pos)
{
    mduv3x0Contact_t contactData = {0};

    if(pos >= maxNumContacts)
        return -1;

    W25Qx_readData(contactBaseAddr + pos * sizeof(mduv3x0Contact_t),
                   &contactData, sizeof(contactData));

    _clearAndCopyWideName(contactData.name, 16, contact.name);

    return _writeChannelAtAddress(contactBaseAddr + pos * sizeof(mduv3x0Contact_t),
                                  &contactData, sizeof(contactData));
}

int cps_insertContact(contact_t contact, uint16_t pos)
{
    contact_t existing;

    if(pos >= maxNumContacts)
        return -1;

    if(cps_readContact(&existing, pos) == 0)
        return -1;

    return cps_writeContact(contact, pos);
}

int cps_deleteContact(uint16_t pos)
{
    mduv3x0Contact_t emptyData = {0};

    if(pos >= maxNumContacts)
        return -1;

    return _writeChannelAtAddress(contactBaseAddr + pos * sizeof(mduv3x0Contact_t),
                                  &emptyData, sizeof(emptyData));
}

int cps_writeBankHeader(bankHdr_t b_header, uint16_t pos)
{
    mduv3x0Zone_t zoneData = {0};
    mduv3x0ZoneExt_t zoneExtData = {0};
    uint32_t zoneAddr = zoneBaseAddr + (pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (pos + 1) * sizeof(mduv3x0ZoneExt_t);

    if(pos >= maxNumZones)
        return -1;

    W25Qx_readData(zoneAddr, &zoneData, sizeof(zoneData));
    W25Qx_readData(zoneExtAddr, &zoneExtData, sizeof(zoneExtData));

    _clearAndCopyWideName(zoneData.name, 16, b_header.name);

    if(_writeZoneAtAddress(zoneAddr, &zoneData, sizeof(zoneData)) < 0)
        return -1;

    return _writeZoneAtAddress(zoneExtAddr, &zoneExtData, sizeof(zoneExtData));
}

int cps_insertBankHeader(bankHdr_t b_header, uint16_t pos)
{
    bankHdr_t existing;
    mduv3x0ZoneExt_t zoneExtData = {0};
    uint32_t zoneExtAddr = zoneExtBaseAddr + (pos + 1) * sizeof(mduv3x0ZoneExt_t);

    if(pos >= maxNumZones)
        return -1;

    if(cps_readBankHeader(&existing, pos) == 0)
        return -1;

    if(cps_writeBankHeader(b_header, pos) < 0)
        return -1;

    return _writeZoneAtAddress(zoneExtAddr, &zoneExtData, sizeof(zoneExtData));
}

int cps_deleteBankHeader(uint16_t pos)
{
    mduv3x0Zone_t zoneData = {0};
    mduv3x0ZoneExt_t zoneExtData = {0};
    uint32_t zoneAddr = zoneBaseAddr + (pos + 1) * sizeof(mduv3x0Zone_t);
    uint32_t zoneExtAddr = zoneExtBaseAddr + (pos + 1) * sizeof(mduv3x0ZoneExt_t);

    if(pos >= maxNumZones)
        return -1;

    if(_writeZoneAtAddress(zoneAddr, &zoneData, sizeof(zoneData)) < 0)
        return -1;

    return _writeZoneAtAddress(zoneExtAddr, &zoneExtData, sizeof(zoneExtData));
}

int cps_insertBankData(uint32_t ch, uint16_t bank_pos, uint16_t pos)
{
    uint16_t members[64] = {0};

    if((bank_pos >= maxNumZones) || (pos >= 64) || (ch >= maxNumChannels))
        return -1;

    if(_readZoneMembers(bank_pos, members) < 0)
        return -1;

    for(uint16_t i = 0; i < 64; i++)
    {
        if(members[i] == (ch + 1))
            return 0;
    }

    for(uint16_t i = 63; i > pos; i--)
        members[i] = members[i - 1];

    members[pos] = ch + 1;
    return _writeZoneMembers(bank_pos, members);
}

int cps_deleteBankData(uint16_t bank_pos, uint16_t pos)
{
    uint16_t members[64] = {0};

    if((bank_pos >= maxNumZones) || (pos >= 64))
        return -1;

    if(_readZoneMembers(bank_pos, members) < 0)
        return -1;

    for(uint16_t i = pos; i < 63; i++)
        members[i] = members[i + 1];

    members[63] = 0;
    return _writeZoneMembers(bank_pos, members);
}
