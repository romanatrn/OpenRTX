/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef POWER_H
#define POWER_H

#include "core/datatypes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    POWER_PROFILE_5W = 0,
    POWER_PROFILE_10W,
    POWER_PROFILE_20W_SWEEP,
    POWER_PROFILE_MAX
}
txPowerProfile_t;

typedef struct
{
    int16_t  apcScale;
    uint16_t vhfMilliWatt;
    uint16_t uhfMilliWatt;
}
powerSweepEntry_t;

bool powerIsIndexedValue(uint32_t power);
uint32_t powerMakeIndexedValue(uint8_t index);
uint8_t powerGetStepIndex(uint32_t power);
uint32_t powerNormalizeStoredValue(uint32_t power, uint8_t profile);
uint32_t powerGetDefaultStoredValue(void);
uint32_t powerGetDisplayMilliWatt(uint32_t power, freq_t txFrequency);
uint8_t powerGetApcControlValue(uint32_t power, freq_t txFrequency,
                                uint8_t lowApc, uint8_t highApc);
uint32_t powerGetNextStep(uint32_t power, int8_t direction, uint8_t profile);
uint8_t powerStepChange(uint32_t *power, int8_t direction, uint8_t profile);
size_t powerGetStepCount(void);
uint8_t powerGetProfileMaxIndex(uint8_t profile);
const char *powerGetProfileName(uint8_t profile);
const powerSweepEntry_t *powerGetSweepEntry(uint8_t index);
void powerFormatLabel(char *buf, size_t maxLen, uint32_t power,
                      freq_t txFrequency, bool compact);
void powerFormatVoiceLabel(char *buf, size_t maxLen, uint32_t power);

#ifdef __cplusplus
}
#endif

#endif /* POWER_H */
