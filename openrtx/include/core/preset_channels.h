#ifndef PRESET_CHANNELS_H
#define PRESET_CHANNELS_H

#include <stdbool.h>
#include <stdint.h>
#include "core/bandplan.h"
#include "core/cps.h"

#define PRESET_BANK_FRS        ((uint16_t) (UINT16_MAX - 1U))
#define PRESET_BANK_MURS       ((uint16_t) (UINT16_MAX - 2U))
#define PRESET_BANK_WEATHER_US ((uint16_t) (UINT16_MAX - 3U))
#define PRESET_BANK_WEATHER_CA ((uint16_t) (UINT16_MAX - 4U))
#define PRESET_BANK_MARINE     ((uint16_t) (UINT16_MAX - 5U))

#define PRESET_BANK_MAX_VISIBLE 5U

uint8_t presetChannelsGetVisibleBankIds(bandplan_t bandplan, uint16_t *bankIds,
                                        uint8_t maxCount);
bool presetChannelsIsPresetBank(uint16_t bankId);
const char *presetChannelsGetBankName(uint16_t bankId);
uint16_t presetChannelsGetCount(uint16_t bankId);
int presetChannelsGetChannel(uint16_t bankId, uint16_t channelIndex, channel_t *channel);
int presetChannelsGetChannelForBandplan(uint16_t bankId, bandplan_t bandplan,
                                        uint16_t channelIndex, channel_t *channel);

#endif
