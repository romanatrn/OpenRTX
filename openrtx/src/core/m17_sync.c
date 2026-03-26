/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/m17_sync.h"

#include <string.h>

#include "core/dev_console.h"
#include "core/nvmem_access.h"
#include "interfaces/cps_io.h"

#define M17_SYNC_JOURNAL_MAGIC 0x4A535931UL
#define M17_SYNC_JOURNAL_VERSION 1U
#define M17_SYNC_JOURNAL_STATE_CLEAR 0U
#define M17_SYNC_JOURNAL_STATE_PREPARED 1U
#define M17_SYNC_JOURNAL_AREA 0U
#define M17_SYNC_JOURNAL_OFFSET 0x00F00000UL
#define M17_SYNC_JOURNAL_SIZE   0x00100000UL

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint8_t state;
    uint8_t selection;
    uint8_t include_keys;
    uint8_t reserved[3];
    uint32_t used;
    uint16_t record_count;
    uint16_t settings_len;
    uint16_t contact_count;
    uint16_t channel_count;
    uint16_t zone_count;
}
__attribute__((packed)) m17SyncJournalHeader_t;

typedef struct
{
    uint8_t category;
    uint8_t reserved;
    uint16_t index;
    uint16_t len;
}
__attribute__((packed)) m17SyncJournalRecord_t;

static int m17SyncJournalReadHeader(m17SyncJournalHeader_t *header)
{
    if(header == NULL)
        return -1;

    if(nvm_read(M17_SYNC_JOURNAL_AREA, 0, M17_SYNC_JOURNAL_OFFSET,
                header, sizeof(*header)) < 0)
        return -1;

    if((header->magic != M17_SYNC_JOURNAL_MAGIC)
       || (header->version != M17_SYNC_JOURNAL_VERSION)
       || (header->used > M17_SYNC_JOURNAL_SIZE)
       || (header->used < sizeof(*header)))
        return -1;

    return 0;
}

static int m17SyncJournalWriteHeader(const m17SyncJournalHeader_t *header)
{
    if(header == NULL)
        return -1;

    return nvm_write(M17_SYNC_JOURNAL_AREA, 0, M17_SYNC_JOURNAL_OFFSET,
                     header, sizeof(*header));
}

static int m17SyncJournalErase(void)
{
    if(nvm_erase(M17_SYNC_JOURNAL_AREA, 0, M17_SYNC_JOURNAL_OFFSET,
                 M17_SYNC_JOURNAL_SIZE) < 0)
        return -1;

    return 0;
}

static int m17SyncJournalWrite(uint32_t offset, const void *data, size_t len)
{
    if((offset + len) > M17_SYNC_JOURNAL_SIZE)
        return -1;

    return nvm_write(M17_SYNC_JOURNAL_AREA, 0, M17_SYNC_JOURNAL_OFFSET + offset,
                     data, len);
}

static int m17SyncJournalRead(uint32_t offset, void *data, size_t len)
{
    if((offset + len) > M17_SYNC_JOURNAL_SIZE)
        return -1;

    return nvm_read(M17_SYNC_JOURNAL_AREA, 0, M17_SYNC_JOURNAL_OFFSET + offset,
                    data, len);
}

static int m17SyncJournalAppendRecord(m17SyncJournalHeader_t *header,
                                      uint8_t category, uint16_t index,
                                      const uint8_t *data, uint16_t len)
{
    m17SyncJournalRecord_t record = {0};
    uint32_t offset;

    if((header == NULL) || ((data == NULL) && (len != 0U)))
        return -1;

    offset = header->used;
    if((offset + sizeof(record) + len) > M17_SYNC_JOURNAL_SIZE)
        return -1;

    record.category = category;
    record.index = index;
    record.len = len;

    if(m17SyncJournalWrite(offset, &record, sizeof(record)) < 0)
        return -1;

    offset += sizeof(record);
    if((len > 0U) && (m17SyncJournalWrite(offset, data, len) < 0))
        return -1;

    header->used += sizeof(record) + len;
    header->record_count++;
    return 0;
}

static size_t m17SyncSerializeZone(uint16_t index, uint8_t *dst, size_t dstLen)
{
    m17SyncZoneBlob_t blob;
    bankHdr_t bank;

    if((dst == NULL) || (dstLen < sizeof(blob)))
        return 0U;

    if(cps_readBankHeader(&bank, index) != 0)
        return 0U;

    memset(&blob, 0, sizeof(blob));
    blob.header = bank;
    blob.member_count = bank.ch_count;

    if(blob.member_count > 64U)
        blob.member_count = 64U;

    for(uint16_t i = 0; i < blob.member_count; i++)
    {
        int member = cps_readBankData(index, i);
        if(member < 0)
            return 0U;
        blob.members[i] = (uint16_t) member;
    }

    memcpy(dst, &blob, sizeof(blob));
    return sizeof(blob);
}

static int m17SyncApplyZone(uint16_t index, const uint8_t *src, size_t len)
{
    m17SyncZoneBlob_t blob;

    if((src == NULL) || (len < sizeof(blob)))
        return -1;

    memcpy(&blob, src, sizeof(blob));

    if(blob.member_count > 64U)
        return -1;

    if(cps_insertBankHeader(blob.header, index) != 0)
        return -1;

    for(uint16_t i = 0; i < blob.member_count; i++)
    {
        if(cps_insertBankData(blob.members[i], index, i) != 0)
            return -1;
    }

    return 0;
}

static uint16_t m17SyncCountChannels(void)
{
    uint16_t count = 0;
    channel_t channel;

    while((count < UINT16_MAX) && (cps_readChannel(&channel, count + 1U) == 0))
        count++;

    return count;
}

static uint16_t m17SyncCountContacts(void)
{
    uint16_t count = 0;
    contact_t contact;

    while((count < UINT16_MAX) && (cps_readContact(&contact, count + 1U) == 0))
        count++;

    return count;
}

static uint16_t m17SyncCountZones(void)
{
    uint16_t count = 0;
    bankHdr_t bank;

    while((count < UINT16_MAX) && (cps_readBankHeader(&bank, count) == 0))
        count++;

    return count;
}

static settings_t m17SyncGetExportedSettings(const settings_t *settings, bool includeKeys)
{
    settings_t copy = *settings;

    copy.usbLogExport = false;
    copy.sqlLevel = default_settings.sqlLevel;

    if(includeKeys == false)
    {
        for(uint8_t i = 0; i < M17_KEY_SLOTS; i++)
            memset(copy.m17_keys[i], 0, sizeof(copy.m17_keys[i]));
    }

    return copy;
}

uint8_t m17SyncGetDefaultSelection(void)
{
    return M17_SYNC_CONTACTS | M17_SYNC_CHANNELS | M17_SYNC_ZONES | M17_SYNC_SETTINGS;
}

bool m17SyncSelectionValid(uint8_t selection)
{
    const uint8_t knownFlags = M17_SYNC_CONTACTS | M17_SYNC_CHANNELS
                             | M17_SYNC_ZONES | M17_SYNC_SETTINGS;

    return (selection & ~knownFlags) == 0U;
}

void m17SyncBuildManifest(m17SyncManifest_t *manifest, const settings_t *settings)
{
    if((manifest == NULL) || (settings == NULL))
        return;

    memset(manifest, 0, sizeof(*manifest));
    manifest->version = M17_SYNC_PROTO_VERSION;
    manifest->selection = settings->m17_sync_flags;
    manifest->include_keys = settings->m17_sync_include_keys ? 1U : 0U;

    if(settings->m17_sync_flags & M17_SYNC_CONTACTS)
        manifest->contacts = m17SyncCountContacts();

    if(settings->m17_sync_flags & M17_SYNC_CHANNELS)
        manifest->channels = m17SyncCountChannels();

    if(settings->m17_sync_flags & M17_SYNC_ZONES)
        manifest->zones = m17SyncCountZones();

    if(settings->m17_sync_flags & M17_SYNC_SETTINGS)
        manifest->settings_bytes = sizeof(m17SyncSettingsBlob_t);
}

size_t m17SyncSerializeSettings(uint8_t *dst, size_t dstLen,
                                const settings_t *settings, bool includeKeys)
{
    m17SyncSettingsBlob_t blob;

    if((dst == NULL) || (settings == NULL) || (dstLen < sizeof(blob)))
        return 0U;

    memset(&blob, 0, sizeof(blob));
    blob.version = M17_SYNC_PROTO_VERSION;
    blob.length = sizeof(blob);
    blob.include_keys = includeKeys ? 1U : 0U;
    blob.settings = m17SyncGetExportedSettings(settings, includeKeys);

    memcpy(dst, &blob, sizeof(blob));
    return sizeof(blob);
}

int m17SyncDeserializeSettings(settings_t *settings, const uint8_t *src, size_t len)
{
    m17SyncSettingsBlob_t blob;

    if((settings == NULL) || (src == NULL) || (len < sizeof(blob)))
        return -1;

    memcpy(&blob, src, sizeof(blob));

    if((blob.version != M17_SYNC_PROTO_VERSION) ||
       (blob.length != sizeof(blob)))
        return -1;

    settings_t updated = blob.settings;

    if(blob.include_keys == 0U)
    {
        for(uint8_t i = 0; i < M17_KEY_SLOTS; i++)
            memcpy(updated.m17_keys[i], settings->m17_keys[i], sizeof(updated.m17_keys[i]));
    }

    updated.usbLogExport = false;
    updated.sqlLevel = default_settings.sqlLevel;
    *settings = updated;
    return 0;
}

uint16_t m17SyncGetObjectCount(uint8_t category, const settings_t *settings)
{
    if(settings == NULL)
        return 0U;

    switch(category)
    {
        case M17_SYNC_CAT_MANIFEST:
            return 1U;
        case M17_SYNC_CAT_SETTINGS:
            return (settings->m17_sync_flags & M17_SYNC_SETTINGS) ? 1U : 0U;
        case M17_SYNC_CAT_CONTACT:
            return (settings->m17_sync_flags & M17_SYNC_CONTACTS) ? m17SyncCountContacts() : 0U;
        case M17_SYNC_CAT_CHANNEL:
            return (settings->m17_sync_flags & M17_SYNC_CHANNELS) ? m17SyncCountChannels() : 0U;
        case M17_SYNC_CAT_ZONE:
            return (settings->m17_sync_flags & M17_SYNC_ZONES) ? m17SyncCountZones() : 0U;
        default:
            return 0U;
    }
}

size_t m17SyncSerializeObject(uint8_t category, uint16_t index,
                              const settings_t *settings,
                              uint8_t *dst, size_t dstLen)
{
    m17SyncManifest_t manifest;
    contact_t contact;
    channel_t channel;

    if((settings == NULL) || (dst == NULL))
        return 0U;

    switch(category)
    {
        case M17_SYNC_CAT_MANIFEST:
            if(dstLen < sizeof(manifest))
                return 0U;
            m17SyncBuildManifest(&manifest, settings);
            memcpy(dst, &manifest, sizeof(manifest));
            return sizeof(manifest);

        case M17_SYNC_CAT_SETTINGS:
            return m17SyncSerializeSettings(dst, dstLen, settings,
                                            settings->m17_sync_include_keys);

        case M17_SYNC_CAT_CONTACT:
            if((dstLen < sizeof(contact)) || (cps_readContact(&contact, index + 1U) != 0))
                return 0U;
            memcpy(dst, &contact, sizeof(contact));
            return sizeof(contact);

        case M17_SYNC_CAT_CHANNEL:
            if((dstLen < sizeof(channel)) || (cps_readChannel(&channel, index + 1U) != 0))
                return 0U;
            memcpy(dst, &channel, sizeof(channel));
            return sizeof(channel);

        case M17_SYNC_CAT_ZONE:
            return m17SyncSerializeZone(index, dst, dstLen);

        default:
            return 0U;
    }
}

int m17SyncApplyObject(uint8_t category, uint16_t index,
                       const uint8_t *src, size_t len,
                       settings_t *stagedSettings)
{
    contact_t contact;
    channel_t channel;

    switch(category)
    {
        case M17_SYNC_CAT_SETTINGS:
            if(stagedSettings == NULL)
                return -1;
            return m17SyncDeserializeSettings(stagedSettings, src, len);

        case M17_SYNC_CAT_CONTACT:
            if(len < sizeof(contact))
                return -1;
            memcpy(&contact, src, sizeof(contact));
            return cps_insertContact(contact, index + 1U);

        case M17_SYNC_CAT_CHANNEL:
            if(len < sizeof(channel))
                return -1;
            memcpy(&channel, src, sizeof(channel));
            return cps_insertChannel(channel, index);

        case M17_SYNC_CAT_ZONE:
            return m17SyncApplyZone(index, src, len);

        default:
            return -1;
    }
}

int m17SyncResetSelectedCodeplug(uint8_t selection)
{
    if((selection & (M17_SYNC_CONTACTS | M17_SYNC_CHANNELS | M17_SYNC_ZONES)) == 0U)
        return 0;

    return cps_create(NULL);
}

int m17SyncJournalBegin(const settings_t *settings)
{
    m17SyncJournalHeader_t header;
    settings_t selectionSettings;
    uint16_t counts[3];
    uint16_t i;
    uint8_t buffer[sizeof(m17SyncZoneBlob_t)];
    size_t len;

    if(settings == NULL)
        return -1;

    memset(&header, 0, sizeof(header));
    header.magic = M17_SYNC_JOURNAL_MAGIC;
    header.version = M17_SYNC_JOURNAL_VERSION;
    header.state = M17_SYNC_JOURNAL_STATE_PREPARED;
    header.selection = settings->m17_sync_flags;
    header.include_keys = settings->m17_sync_include_keys ? 1U : 0U;
    header.used = sizeof(header);

    if(m17SyncJournalErase() < 0)
        return -1;

    selectionSettings = *settings;
    selectionSettings.m17_sync_flags = settings->m17_sync_flags;
    selectionSettings.m17_sync_include_keys = settings->m17_sync_include_keys;

    if(header.selection & M17_SYNC_SETTINGS)
    {
        len = m17SyncSerializeSettings(buffer, sizeof(buffer), settings,
                                       settings->m17_sync_include_keys);
        if((len == 0U) || (m17SyncJournalAppendRecord(&header, M17_SYNC_CAT_SETTINGS,
                                                      0U, buffer, len) != 0))
            goto fail;
        header.settings_len = len;
    }

    counts[0] = m17SyncGetObjectCount(M17_SYNC_CAT_CONTACT, &selectionSettings);
    counts[1] = m17SyncGetObjectCount(M17_SYNC_CAT_CHANNEL, &selectionSettings);
    counts[2] = m17SyncGetObjectCount(M17_SYNC_CAT_ZONE, &selectionSettings);
    header.contact_count = counts[0];
    header.channel_count = counts[1];
    header.zone_count = counts[2];

    for(i = 0; i < counts[0]; i++)
    {
        len = m17SyncSerializeObject(M17_SYNC_CAT_CONTACT, i, &selectionSettings,
                                     buffer, sizeof(buffer));
        if((len == 0U) || (m17SyncJournalAppendRecord(&header, M17_SYNC_CAT_CONTACT,
                                                      i, buffer, len) != 0))
            goto fail;
    }

    for(i = 0; i < counts[1]; i++)
    {
        len = m17SyncSerializeObject(M17_SYNC_CAT_CHANNEL, i, &selectionSettings,
                                     buffer, sizeof(buffer));
        if((len == 0U) || (m17SyncJournalAppendRecord(&header, M17_SYNC_CAT_CHANNEL,
                                                      i, buffer, len) != 0))
            goto fail;
    }

    for(i = 0; i < counts[2]; i++)
    {
        len = m17SyncSerializeObject(M17_SYNC_CAT_ZONE, i, &selectionSettings,
                                     buffer, sizeof(buffer));
        if((len == 0U) || (m17SyncJournalAppendRecord(&header, M17_SYNC_CAT_ZONE,
                                                      i, buffer, len) != 0))
            goto fail;
    }

    if(m17SyncJournalWriteHeader(&header) < 0)
        goto fail;

    devConsole_log(DEVLOG_INFO, "M17", "Sync journal prepared sel=%u rec=%u",
                   header.selection, header.record_count);
    return 0;

fail:
    m17SyncJournalErase();
    return -1;
}

static int m17SyncJournalReplay(settings_t *settings)
{
    m17SyncJournalHeader_t header;
    uint32_t offset = sizeof(header);
    uint16_t count = 0U;
    uint8_t buffer[sizeof(m17SyncZoneBlob_t)];

    if((settings == NULL) || (m17SyncJournalReadHeader(&header) < 0))
        return -1;

    if(m17SyncResetSelectedCodeplug(header.selection) != 0)
        return -1;

    while((offset + sizeof(m17SyncJournalRecord_t)) <= header.used)
    {
        m17SyncJournalRecord_t record;

        if(m17SyncJournalRead(offset, &record, sizeof(record)) < 0)
            return -1;

        offset += sizeof(record);
        if((offset + record.len) > header.used || (record.len > sizeof(buffer)))
            return -1;

        if((record.len > 0U) && (m17SyncJournalRead(offset, buffer, record.len) < 0))
            return -1;

        if(m17SyncApplyObject(record.category, record.index, buffer, record.len,
                              settings) != 0)
            return -1;

        offset += record.len;
        count++;
    }

    devConsole_log(DEVLOG_WARN, "M17", "Sync journal replayed %u records", count);
    return 0;
}

int m17SyncJournalRollback(settings_t *settings)
{
    if(m17SyncJournalReplay(settings) < 0)
        return -1;

    m17SyncJournalErase();
    return 0;
}

int m17SyncJournalRecover(settings_t *settings)
{
    m17SyncJournalHeader_t header;

    if(m17SyncJournalReadHeader(&header) < 0)
        return -1;

    if(header.state != M17_SYNC_JOURNAL_STATE_PREPARED)
        return -1;

    if(m17SyncJournalRollback(settings) < 0)
        return -1;

    devConsole_log(DEVLOG_WARN, "M17", "Recovered interrupted sync journal");
    return 0;
}

void m17SyncJournalCommit(void)
{
    m17SyncJournalErase();
}

bool m17SyncJournalPending(void)
{
    m17SyncJournalHeader_t header;
    return m17SyncJournalReadHeader(&header) == 0;
}
