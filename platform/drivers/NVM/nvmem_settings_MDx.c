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

/*
 * Settings and VFO persistence for MDx targets.
 *
 * Data lives in internal flash sector 11 (128 KiB on STM32F405).
 * We use a bounded append-only journal so each logical save only writes one
 * record plus one flag word update, and the whole sector is erased only when
 * the journal wraps.
 */

#define SETTINGS_FLASH_SIZE   0x20000u
#define SETTINGS_FLAG_WORDS   16u
#define SETTINGS_BLOCK_COUNT  (SETTINGS_FLAG_WORDS * 32u)

typedef struct
{
    uint16_t   crc;
    settings_t settings;
    channel_t  vfoData;
}
__attribute__((packed)) dataBlock_t;

typedef struct
{
    uint32_t    magic;
    uint32_t    flags[SETTINGS_FLAG_WORDS];
    dataBlock_t data[SETTINGS_BLOCK_COUNT];
}
__attribute__((packed)) memory_t;

typedef char memoryFitsInFlash[
    (sizeof(memory_t) <= SETTINGS_FLASH_SIZE) ? 1 : -1
];

static const uint32_t MEM_MAGIC   = 0x584E504F;    // "OPNX"
static const uint32_t baseAddress = 0x080E0000;
static memory_t *memory = ((memory_t *) baseAddress);

static int findActiveBlock()
{
    if(memory->magic != MEM_MAGIC)
        return -1;

    for(uint16_t word = 0; word < SETTINGS_FLAG_WORDS; word++)
    {
        uint32_t flags = memory->flags[word];

        if(flags == 0xFFFFFFFFu)
        {
            if((word == 0) && (memory->data[0].crc == 0xFFFFu))
                return -1;

            return (word * 32u) - 1;
        }

        if(flags != 0x00000000u)
        {
            for(uint16_t bit = 0; bit < 32u; bit++)
            {
                if((flags & (1u << bit)) != 0u)
                {
                    uint16_t block = (word * 32u) + bit;
                    if(block == 0u)
                        return -1;

                    return block - 1;
                }
            }
        }
    }

    return SETTINGS_BLOCK_COUNT - 1u;
}

static int readBlock(uint16_t block, settings_t *settings, channel_t *vfo)
{
    if(block >= SETTINGS_BLOCK_COUNT)
        return -1;

    uint16_t crc = crc_ccitt(&(memory->data[block].settings),
                             sizeof(settings_t) + sizeof(channel_t));
    if(crc != memory->data[block].crc)
        return -1;

    if(settings != NULL)
        memcpy(settings, &(memory->data[block].settings), sizeof(settings_t));

    if(vfo != NULL)
        memcpy(vfo, &(memory->data[block].vfoData), sizeof(channel_t));

    return 0;
}

static int initStorage()
{
    if(flash_eraseSector(11) == false)
        return -1;

    flash_write((uint32_t) &(memory->magic), &MEM_MAGIC, sizeof(MEM_MAGIC));
    return 0;
}

int nvm_readVfoChannelData(channel_t *channel)
{
    int block = findActiveBlock();

    if(block < 0)
        return -1;

    return readBlock((uint16_t) block, NULL, channel);
}

int nvm_readSettings(settings_t *settings)
{
    int block = findActiveBlock();

    if(block < 0)
        return -1;

    return readBlock((uint16_t) block, settings, NULL);
}

int nvm_writeSettingsAndVfo(const settings_t *settings, const channel_t *vfo)
{
    int current = findActiveBlock();
    uint16_t nextBlock = 0;
    uint16_t currentBlock = 0;
    dataBlock_t nextData;

    memcpy(&(nextData.settings), settings, sizeof(settings_t));
    memcpy(&(nextData.vfoData), vfo, sizeof(channel_t));
    nextData.crc = crc_ccitt(&(nextData.settings), sizeof(settings_t) + sizeof(channel_t));

    if(current >= 0)
    {
        currentBlock = (uint16_t) current;

        if(nextData.crc == memory->data[currentBlock].crc)
        {
            if((memcmp(&(memory->data[currentBlock].settings), settings, sizeof(settings_t)) == 0) &&
               (memcmp(&(memory->data[currentBlock].vfoData), vfo, sizeof(channel_t)) == 0))
            {
                return 0;
            }
        }

        nextBlock = currentBlock + 1u;
    }

    if((current < 0) || (nextBlock >= SETTINGS_BLOCK_COUNT))
    {
        if(initStorage() < 0)
            return -1;

        nextBlock = 0;
    }

    flash_write((uint32_t) &(memory->data[nextBlock]), &nextData, sizeof(nextData));

    uint16_t word = nextBlock / 32u;
    uint16_t bit  = nextBlock % 32u;
    uint32_t newFlags = memory->flags[word] & ~(1u << bit);
    flash_write((uint32_t) &(memory->flags[word]), &newFlags, sizeof(newFlags));

    return 0;
}

int nvm_writeSettings(const settings_t *settings)
{
    channel_t vfoData;

    if(nvm_readVfoChannelData(&vfoData) < 0)
        vfoData = cps_getDefaultChannel();

    return nvm_writeSettingsAndVfo(settings, &vfoData);
}
