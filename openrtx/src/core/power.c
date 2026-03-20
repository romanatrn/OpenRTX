/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/power.h"
#include "core/utils.h"
#include <string.h>

#define POWER_INDEX_FLAG 0x80000000UL

#define POWER_BAND_VHF_LO 136000000U
#define POWER_BAND_VHF_HI 174000000U
#define POWER_BAND_UHF_LO 400000000U
#define POWER_BAND_UHF_HI 470000000U

static const powerSweepEntry_t powerSweepTable[] =
{
    /*
     * MDUV3x0 sweep table for PA characterization.
     *
     * apcScale is expressed relative to the existing low/high calibration points:
     *   0    -> current low-power calibration endpoint
     *   1000 -> current high-power calibration endpoint
     *
     * Values outside the 0..1000 range intentionally allow below-1W and above-5W
     * exploration. vhfMilliWatt / uhfMilliWatt are placeholders and should be
     * updated with wattmeter measurements before relying on the displayed values.
     */
    { -238,    50,    50 },
    { -225,   100,   100 },
    { -188,   250,   250 },
    { -125,   500,   500 },
    {  -63,   750,   750 },
    {    0,  1000,  1000 },
    {   64,  1250,  1250 },
    {  128,  1500,  1500 },
    {  192,  1750,  1750 },
    {  256,  2000,  2000 },
    {  384,  2500,  2500 },
    {  512,  3000,  3000 },
    {  640,  3500,  3500 },
    {  768,  4000,  4000 },
    {  896,  4500,  4500 },
    { 1024,  5000,  5000 },
    { 1280,  6000,  6000 },
    { 1536,  7000,  7000 },
    { 1792,  8000,  8000 },
    { 2048,  9000,  9000 },
    { 2304, 10000, 10000 },
    { 2688, 11500, 11500 },
    { 3072, 13000, 13000 },
    { 3456, 14500, 14500 },
    { 3840, 16000, 16000 },
    { 4352, 17500, 17500 },
    { 4864, 19000, 19000 },
    { 5376, 20500, 20500 },
    { 5888, 22000, 22000 },
    { 6400, 23500, 23500 },
    { 6912, 25000, 25000 },
};

static const char *powerProfileNames[] =
{
    "5W",
    "10W",
    "20W Sweep"
};

static uint8_t _powerClampIndex(uint8_t index, uint8_t profile)
{
    uint8_t maxIndex = powerGetProfileMaxIndex(profile);
    if(index > maxIndex)
        return maxIndex;

    return index;
}

static uint8_t _powerIndexFromLegacyMilliWatt(uint32_t power)
{
    uint8_t bestIndex = 0;
    uint32_t bestDelta = UINT32_MAX;

    for(uint8_t i = 0; i < ARRAY_SIZE(powerSweepTable); i++)
    {
        uint32_t entryPower = powerSweepTable[i].vhfMilliWatt;
        uint32_t delta = (entryPower > power) ? (entryPower - power)
                                              : (power - entryPower);
        if(delta < bestDelta)
        {
            bestDelta = delta;
            bestIndex = i;
        }
    }

    return bestIndex;
}

static uint32_t _powerGetBandMilliWatt(uint8_t index, freq_t txFrequency)
{
    if((txFrequency >= POWER_BAND_VHF_LO) && (txFrequency <= POWER_BAND_VHF_HI))
        return powerSweepTable[index].vhfMilliWatt;

    return powerSweepTable[index].uhfMilliWatt;
}

bool powerIsIndexedValue(uint32_t power)
{
#if defined(PLATFORM_MDUV3x0)
    return (power & POWER_INDEX_FLAG) != 0;
#else
    (void) power;
    return false;
#endif
}

uint32_t powerMakeIndexedValue(uint8_t index)
{
    return POWER_INDEX_FLAG | (uint32_t) index;
}

uint8_t powerGetStepIndex(uint32_t power)
{
    if(powerIsIndexedValue(power))
    {
        uint8_t index = (uint8_t) (power & 0xFFU);
        if(index < ARRAY_SIZE(powerSweepTable))
            return index;
    }

    return _powerIndexFromLegacyMilliWatt(power);
}

uint32_t powerNormalizeStoredValue(uint32_t power, uint8_t profile)
{
#if defined(PLATFORM_MDUV3x0)
    uint8_t index = powerGetStepIndex(power);
    return powerMakeIndexedValue(_powerClampIndex(index, profile));
#else
    (void) profile;
    return power;
#endif
}

uint32_t powerGetDefaultStoredValue(void)
{
#if defined(PLATFORM_MDUV3x0)
    return powerMakeIndexedValue(_powerIndexFromLegacyMilliWatt(1000));
#else
    return 1000;
#endif
}

uint32_t powerGetDisplayMilliWatt(uint32_t power, freq_t txFrequency)
{
#if defined(PLATFORM_MDUV3x0)
    return _powerGetBandMilliWatt(powerGetStepIndex(power), txFrequency);
#else
    (void) txFrequency;
    return power;
#endif
}

uint8_t powerGetApcControlValue(uint32_t power, freq_t txFrequency,
                                uint8_t lowApc, uint8_t highApc)
{
#if defined(PLATFORM_MDUV3x0)
    (void) txFrequency;
    int32_t delta = (int32_t) highApc - (int32_t) lowApc;
    int32_t scale = powerSweepTable[powerGetStepIndex(power)].apcScale;
    int32_t apc = (int32_t) lowApc + (delta * scale) / 1000;

    if(apc < 0)
        apc = 0;
    if(apc > UINT8_MAX)
        apc = UINT8_MAX;

    return (uint8_t) apc;
#else
    (void) power;
    (void) txFrequency;
    (void) lowApc;
    (void) highApc;
    return 0;
#endif
}

uint8_t powerStepChange(uint32_t *power, int8_t direction, uint8_t profile)
{
    *power = powerGetNextStep(*power, direction, profile);
    return powerGetStepIndex(*power);
}

uint32_t powerGetNextStep(uint32_t power, int8_t direction, uint8_t profile)
{
    uint8_t index = powerGetStepIndex(power);
    uint8_t maxIndex = powerGetProfileMaxIndex(profile);

    if(direction > 0)
    {
        if(index >= maxIndex)
            index = 0;
        else
            index += 1;
    }
    else if(direction < 0)
    {
        if(index == 0)
            index = maxIndex;
        else
            index -= 1;
    }

    index = _powerClampIndex(index, profile);
    return powerMakeIndexedValue(index);
}

size_t powerGetStepCount(void)
{
    return ARRAY_SIZE(powerSweepTable);
}

uint8_t powerGetProfileMaxIndex(uint8_t profile)
{
    switch(profile)
    {
        case POWER_PROFILE_5W:
            return 15;
        case POWER_PROFILE_10W:
            return 20;
        case POWER_PROFILE_20W_SWEEP:
            return ARRAY_SIZE(powerSweepTable) - 1;
        default:
            return 11;
    }
}

const char *powerGetProfileName(uint8_t profile)
{
    if(profile >= ARRAY_SIZE(powerProfileNames))
        return powerProfileNames[0];

    return powerProfileNames[profile];
}

const powerSweepEntry_t *powerGetSweepEntry(uint8_t index)
{
    if(index >= ARRAY_SIZE(powerSweepTable))
        index = ARRAY_SIZE(powerSweepTable) - 1;

    return &powerSweepTable[index];
}

void powerFormatLabel(char *buf, size_t maxLen, uint32_t power,
                      freq_t txFrequency, bool compact)
{
#if defined(PLATFORM_MDUV3x0)
    uint8_t index = powerGetStepIndex(power);
    (void) txFrequency;
    sniprintf(buf, maxLen, compact ? "A%u" : "APC %u", index);
#else
    uint32_t milliWatt = power;
    (void) compact;
    (void) txFrequency;
    sniprintf(buf, maxLen, "%lu.%01luW",
              (unsigned long) (milliWatt / 1000U),
              (unsigned long) ((milliWatt % 1000U) / 100U));
#endif
}

void powerFormatVoiceLabel(char *buf, size_t maxLen, uint32_t power)
{
#if defined(PLATFORM_MDUV3x0)
    sniprintf(buf, maxLen, "APC %u", powerGetStepIndex(power));
#else
    sniprintf(buf, maxLen, "%lu.%lu",
              (unsigned long) (power / 1000lu),
              (unsigned long) ((power % 1000lu) / 100lu));
#endif
}
