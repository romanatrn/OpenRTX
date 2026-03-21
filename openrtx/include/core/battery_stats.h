/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BATTERY_STATS_H
#define BATTERY_STATS_H

#include <stdbool.h>
#include <stdint.h>

#define BATTERY_STATS_GRAPH_POINTS 48

typedef struct
{
    uint8_t  chargePercent;
    uint32_t txSeconds;
    uint32_t rxSeconds;
    uint32_t onSeconds;
    uint16_t remainingMinutes;
    bool     estimateValid;
    uint8_t  graphLength;
    int16_t  estimateGraph[BATTERY_STATS_GRAPH_POINTS];
}
batteryStatsSnapshot_t;

void batteryStatsInit(uint8_t chargePercent);
void batteryStatsUpdate(uint8_t chargePercent);
void batteryStatsSnapshot(batteryStatsSnapshot_t *snapshot);

#endif /* BATTERY_STATS_H */
