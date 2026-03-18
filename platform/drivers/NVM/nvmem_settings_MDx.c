/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/nvmem.h"
#include <string.h>
#include "core/cps.h"
#include "core/crc.h"
#include "flash.h"

typedef struct
{
    uint32_t   magic;
    uint16_t   crc;
    settings_t settings;
    channel_t  vfoData;
}
__attribute__((packed)) persistentData_t;

static const uint32_t MEM_MAGIC   = 0x584E504F;    // "OPNX"
static const uint32_t baseAddress = 0x080E0000;
static persistentData_t *memory = ((persistentData_t *) baseAddress);

static int _readPersistentData(persistentData_t *data)
{
    if(memory->magic != MEM_MAGIC)
        return -1;

    memcpy(data, memory, sizeof(*data));

    uint16_t crc = crc_ccitt(&(data->settings),
                             sizeof(settings_t) + sizeof(channel_t));
    if(crc != data->crc)
        return -1;

    return 0;
}

int nvm_readVfoChannelData(channel_t *channel)
{
    persistentData_t data;

    if(_readPersistentData(&data) < 0)
        return -1;

    memcpy(channel, &data.vfoData, sizeof(channel_t));
    return 0;
}

int nvm_readSettings(settings_t *settings)
{
    persistentData_t data;

    if(_readPersistentData(&data) < 0)
        return -1;

    memcpy(settings, &data.settings, sizeof(settings_t));
    return 0;
}

int nvm_writeSettingsAndVfo(const settings_t *settings, const channel_t *vfo)
{
    persistentData_t current;
    persistentData_t next;

    next.magic = MEM_MAGIC;
    memcpy(&next.settings, settings, sizeof(settings_t));
    memcpy(&next.vfoData, vfo, sizeof(channel_t));
    next.crc = crc_ccitt(&(next.settings), sizeof(settings_t) + sizeof(channel_t));

    if((_readPersistentData(&current) == 0) &&
       (current.crc == next.crc) &&
       (memcmp(&current.settings, &next.settings, sizeof(settings_t)) == 0) &&
       (memcmp(&current.vfoData, &next.vfoData, sizeof(channel_t)) == 0))
    {
        return 0;
    }

    if(flash_eraseSector(11) == false)
        return -1;

    flash_write(baseAddress, &next, sizeof(next));
    return 0;
}

int nvm_writeSettings(const settings_t *settings)
{
    channel_t vfoData;

    if(nvm_readVfoChannelData(&vfoData) < 0)
        vfoData = cps_getDefaultChannel();

    return nvm_writeSettingsAndVfo(settings, &vfoData);
}
