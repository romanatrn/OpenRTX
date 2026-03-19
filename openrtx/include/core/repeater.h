#ifndef REPEATER_H
#define REPEATER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/cps.h"
#include "core/gps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REPEATER_NEAREST_BANK UINT16_MAX
#define REPEATER_NEAREST_MAX 16

typedef struct
{
    uint16_t channel_index;
    uint32_t distance_m;
} repeater_nearest_entry_t;

bool repeater_hasGpsFix(const gps_t *gps);
bool repeater_hasValidLocation(const channel_t *channel);
bool repeater_getChannelLocation(const channel_t *channel,
                                 int32_t *latitude_e6,
                                 int32_t *longitude_e6);
bool repeater_setChannelLocationFromGps(channel_t *channel, const gps_t *gps);
void repeater_clearChannelLocation(channel_t *channel);
uint32_t repeater_distanceMeters(const gps_t *gps, const channel_t *channel);
uint16_t repeater_getChannelCount(void);
uint16_t repeater_getNearestCount(const gps_t *gps);
int32_t repeater_getNearestChannelIndex(const gps_t *gps, uint16_t pos);
uint32_t repeater_getNearestDistance(const gps_t *gps, uint16_t pos);
bool repeater_isNearestChannel(const gps_t *gps, uint16_t channel_index);
void repeater_invalidateNearestCache(void);

#ifdef __cplusplus
}
#endif

#endif
