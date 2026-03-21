/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/battery_stats.h"

#include <limits.h>
#include <string.h>

#include "interfaces/delays.h"
#include "interfaces/radio.h"

#define BATTERY_STATS_SAMPLE_PERIOD_MS      1000u
#define BATTERY_STATS_ESTIMATE_PERIOD_MS   15000u
#define BATTERY_STATS_ACTIVITY_SAMPLES       180u
#define BATTERY_STATS_BASE_RUNTIME_MIN      960u
#define BATTERY_STATS_GRAPH_MAX_MIN        1440u
#define BATTERY_STATS_WEIGHT_IDLE          1000u
#define BATTERY_STATS_WEIGHT_RX            1450u
#define BATTERY_STATS_WEIGHT_TX            4200u

typedef struct
{
    bool      initialized;
    uint8_t   chargePercent;
    uint32_t  txSeconds;
    uint32_t  rxSeconds;
    uint32_t  onSeconds;
    uint32_t  activitySum;
    uint16_t  activityWeights[BATTERY_STATS_ACTIVITY_SAMPLES];
    uint16_t  activityIndex;
    uint16_t  activityCount;
    uint8_t   graphLength;
    uint8_t   graphIndex;
    int16_t   estimateGraph[BATTERY_STATS_GRAPH_POINTS];
    long long lastSampleTick;
    long long lastEstimateTick;
}
batteryStatsRuntime_t;

static batteryStatsRuntime_t runtime;

static uint16_t _batteryStatsGetWeight(enum opstatus status)
{
    switch(status)
    {
        case TX:
            return BATTERY_STATS_WEIGHT_TX;

        case RX:
            return BATTERY_STATS_WEIGHT_RX;

        case OFF:
        default:
            return BATTERY_STATS_WEIGHT_IDLE;
    }
}

static uint16_t _batteryStatsGetAverageWeight(void)
{
    if(runtime.activityCount == 0)
        return BATTERY_STATS_WEIGHT_IDLE;

    return runtime.activitySum / runtime.activityCount;
}

static uint16_t _batteryStatsGetRemainingMinutes(void)
{
    uint32_t averageWeight = _batteryStatsGetAverageWeight();

    if(runtime.chargePercent == 0)
        return 0;

    if(averageWeight == 0)
        averageWeight = BATTERY_STATS_WEIGHT_IDLE;

    uint32_t remainingMinutes = (uint32_t) runtime.chargePercent
                              * BATTERY_STATS_BASE_RUNTIME_MIN
                              * BATTERY_STATS_WEIGHT_IDLE;
    remainingMinutes /= 100u;
    remainingMinutes /= averageWeight;

    if(remainingMinutes > UINT16_MAX)
        remainingMinutes = UINT16_MAX;

    return remainingMinutes;
}

static int16_t _batteryStatsGetGraphPoint(void)
{
    uint32_t remainingMinutes = _batteryStatsGetRemainingMinutes();

    if(remainingMinutes > BATTERY_STATS_GRAPH_MAX_MIN)
        remainingMinutes = BATTERY_STATS_GRAPH_MAX_MIN;

    int32_t scaled = 16000 - ((int32_t) remainingMinutes * 32000)
                           / BATTERY_STATS_GRAPH_MAX_MIN;

    if(scaled > SHRT_MAX)
        scaled = SHRT_MAX;
    if(scaled < SHRT_MIN)
        scaled = SHRT_MIN;

    return (int16_t) scaled;
}

static void _batteryStatsPushGraphPoint(void)
{
    runtime.estimateGraph[runtime.graphIndex] = _batteryStatsGetGraphPoint();
    runtime.graphIndex = (runtime.graphIndex + 1) % BATTERY_STATS_GRAPH_POINTS;

    if(runtime.graphLength < BATTERY_STATS_GRAPH_POINTS)
        runtime.graphLength += 1;
}

void batteryStatsInit(uint8_t chargePercent)
{
    memset(&runtime, 0, sizeof(runtime));
    runtime.initialized = true;
    runtime.chargePercent = chargePercent;
    runtime.lastSampleTick = getTick();
    runtime.lastEstimateTick = runtime.lastSampleTick;
    _batteryStatsPushGraphPoint();
}

void batteryStatsUpdate(uint8_t chargePercent)
{
    if(!runtime.initialized)
        batteryStatsInit(chargePercent);

    runtime.chargePercent = chargePercent;

    long long now = getTick();
    while((now - runtime.lastSampleTick) >= BATTERY_STATS_SAMPLE_PERIOD_MS)
    {
        enum opstatus status = radio_getStatus();
        uint16_t weight = _batteryStatsGetWeight(status);

        runtime.lastSampleTick += BATTERY_STATS_SAMPLE_PERIOD_MS;
        runtime.onSeconds += 1;

        if(status == TX)
            runtime.txSeconds += 1;
        else if(status == RX)
            runtime.rxSeconds += 1;

        if(runtime.activityCount < BATTERY_STATS_ACTIVITY_SAMPLES)
        {
            runtime.activityCount += 1;
        }
        else
        {
            runtime.activitySum -= runtime.activityWeights[runtime.activityIndex];
        }

        runtime.activityWeights[runtime.activityIndex] = weight;
        runtime.activitySum += weight;
        runtime.activityIndex = (runtime.activityIndex + 1)
                              % BATTERY_STATS_ACTIVITY_SAMPLES;
    }

    while((now - runtime.lastEstimateTick) >= BATTERY_STATS_ESTIMATE_PERIOD_MS)
    {
        runtime.lastEstimateTick += BATTERY_STATS_ESTIMATE_PERIOD_MS;
        _batteryStatsPushGraphPoint();
    }
}

void batteryStatsSnapshot(batteryStatsSnapshot_t *snapshot)
{
    if(snapshot == NULL)
        return;

    memset(snapshot, 0, sizeof(*snapshot));

    if(!runtime.initialized)
        return;

    snapshot->chargePercent = runtime.chargePercent;
    snapshot->txSeconds = runtime.txSeconds;
    snapshot->rxSeconds = runtime.rxSeconds;
    snapshot->onSeconds = runtime.onSeconds;
    snapshot->remainingMinutes = _batteryStatsGetRemainingMinutes();
    snapshot->estimateValid = runtime.activityCount >= 5;
    snapshot->graphLength = runtime.graphLength;

    for(uint8_t i = 0; i < runtime.graphLength; i++)
    {
        uint8_t src = (runtime.graphIndex + BATTERY_STATS_GRAPH_POINTS
                     - runtime.graphLength + i) % BATTERY_STATS_GRAPH_POINTS;
        snapshot->estimateGraph[i] = runtime.estimateGraph[src];
    }
}
