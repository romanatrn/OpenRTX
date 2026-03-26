/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef M17_SYNC_H
#define M17_SYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/cps.h"
#include "core/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define M17_SYNC_PROTO_VERSION 1U

enum m17SyncRole
{
    M17_SYNC_OFF = 0,
    M17_SYNC_SEND,
    M17_SYNC_RECEIVE
};

enum m17SyncFlags
{
    M17_SYNC_CONTACTS = 1U << 0,
    M17_SYNC_CHANNELS = 1U << 1,
    M17_SYNC_ZONES    = 1U << 2,
    M17_SYNC_SETTINGS = 1U << 3
};

typedef struct
{
    uint8_t version;
    uint8_t selection;
    uint8_t include_keys;
    uint8_t reserved;
    uint16_t contacts;
    uint16_t channels;
    uint16_t zones;
    uint16_t settings_bytes;
}
__attribute__((packed)) m17SyncManifest_t;

typedef struct
{
    uint16_t version;
    uint16_t length;
    uint8_t include_keys;
    uint8_t reserved[3];
    settings_t settings;
}
__attribute__((packed)) m17SyncSettingsBlob_t;

typedef struct
{
    bankHdr_t header;
    uint16_t member_count;
    uint16_t members[64];
}
__attribute__((packed)) m17SyncZoneBlob_t;

enum m17SyncCategory
{
    M17_SYNC_CAT_MANIFEST = 1,
    M17_SYNC_CAT_SETTINGS = 2,
    M17_SYNC_CAT_CONTACT  = 3,
    M17_SYNC_CAT_CHANNEL  = 4,
    M17_SYNC_CAT_ZONE     = 5
};

uint8_t m17SyncGetDefaultSelection(void);

bool m17SyncSelectionValid(uint8_t selection);

void m17SyncBuildManifest(m17SyncManifest_t *manifest, const settings_t *settings);

size_t m17SyncSerializeSettings(uint8_t *dst, size_t dstLen,
                                const settings_t *settings, bool includeKeys);

int m17SyncDeserializeSettings(settings_t *settings, const uint8_t *src, size_t len);

uint16_t m17SyncGetObjectCount(uint8_t category, const settings_t *settings);

size_t m17SyncSerializeObject(uint8_t category, uint16_t index,
                              const settings_t *settings,
                              uint8_t *dst, size_t dstLen);

int m17SyncApplyObject(uint8_t category, uint16_t index,
                       const uint8_t *src, size_t len,
                       settings_t *stagedSettings);

int m17SyncResetSelectedCodeplug(uint8_t selection);

int m17SyncJournalBegin(const settings_t *settings);

int m17SyncJournalRollback(settings_t *settings);

int m17SyncJournalRecover(settings_t *settings);

void m17SyncJournalCommit(void);

bool m17SyncJournalPending(void);

#ifdef __cplusplus
}
#endif

#endif /* M17_SYNC_H */
