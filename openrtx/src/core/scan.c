/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <string.h>

#include "core/bandplan.h"
#include "core/event.h"
#include "core/preset_channels.h"
#include "core/repeater.h"
#include "core/scan.h"
#include "core/state.h"
#include "core/ui.h"
#include "interfaces/cps_io.h"
#include "interfaces/delays.h"
#include "interfaces/platform.h"
#include "rtx/rtx.h"

#define SCAN_SETTLE_DELAY_MS 200
#define SCAN_RESUME_DELAY_MS 1000

typedef struct
{
    bool active;
    bool primed_step;
    bool squelch_open;
    long long next_step_tick;
    long long resume_tick;
} scan_runtime_t;

static scan_runtime_t runtime = {0};

static bool _scan_freq_allowed(freq_t freq)
{
    return bandplanIsFrequencyInHardwareRange(platform_getHwInfo(), freq);
}

static bool _scan_channel_allowed(const channel_t *channel)
{
    if(channel == NULL)
        return false;

    if(_scan_freq_allowed(channel->rx_frequency) == false)
        return false;

    if(channel->rx_only)
        return true;

    return _scan_freq_allowed(channel->tx_frequency);
}

static bool _scan_getBankCount(uint16_t bank_index, uint16_t *count)
{
    if(count == NULL)
        return false;

    if(state.bank_is_virtual && (bank_index == REPEATER_NEAREST_BANK))
    {
        *count = repeater_getNearestCount(&state.gps_data);
        return (*count > 0U);
    }

    if(state.bank_is_virtual && presetChannelsIsPresetBank(bank_index))
    {
        *count = presetChannelsGetCount(bank_index);
        return (*count > 0U);
    }

    bankHdr_t bank = {0};
    if(cps_readBankHeader(&bank, bank_index) == -1)
        return false;

    *count = bank.ch_count;
    return (*count > 0U);
}

static bool _scan_loadBankChannel(uint16_t bank_index, uint16_t channel_pos,
                                  channel_t *channel)
{
    int32_t storage_index = -1;

    if(channel == NULL)
        return false;

    if(state.bank_is_virtual && presetChannelsIsPresetBank(bank_index))
    {
        if(presetChannelsGetChannelForBandplan(bank_index,
                                               (bandplan_t) state.settings.bandplan,
                                               channel_pos, channel) == -1)
            return false;

        return _scan_channel_allowed(channel);
    }

    if(state.bank_is_virtual && (bank_index == REPEATER_NEAREST_BANK))
    {
        storage_index = repeater_getNearestChannelIndex(&state.gps_data, channel_pos);
    }
    else
    {
        storage_index = cps_readBankData(bank_index, channel_pos);
    }

    if(storage_index < 0)
        return false;

    if(cps_readChannel(channel, (uint16_t) storage_index + 1U) == -1)
        return false;

    return _scan_channel_allowed(channel);
}

static bool _scan_getCurrentBand(freq_t freq, freq_t *low, freq_t *high)
{
    const hwInfo_t *hw = platform_getHwInfo();

    if((hw == NULL) || (low == NULL) || (high == NULL))
        return false;

    if(hw->vhf_band)
    {
        freq_t vhf_low = (freq_t) hw->vhf_minFreq * 1000000U;
        freq_t vhf_high = (freq_t) hw->vhf_maxFreq * 1000000U;

        if((freq >= vhf_low) && (freq <= vhf_high))
        {
            *low = vhf_low;
            *high = vhf_high;
            return true;
        }
    }

    if(hw->uhf_band)
    {
        freq_t uhf_low = (freq_t) hw->uhf_minFreq * 1000000U;
        freq_t uhf_high = (freq_t) hw->uhf_maxFreq * 1000000U;

        if((freq >= uhf_low) && (freq <= uhf_high))
        {
            *low = uhf_low;
            *high = uhf_high;
            return true;
        }
    }

    return false;
}

static bool _scan_stepVfo(void)
{
    freq_t band_low = 0;
    freq_t band_high = 0;
    const freq_t step = (state.step_index < n_freq_steps)
                      ? (freq_t) freq_steps[state.step_index]
                      : 12500U;
    const int64_t rx_tx_delta = (int64_t) state.channel.tx_frequency
                              - (int64_t) state.channel.rx_frequency;

    if((step == 0U) || !_scan_getCurrentBand(state.channel.rx_frequency, &band_low, &band_high))
        return false;

    uint32_t attempts = (uint32_t) (((band_high - band_low) / step) + 2U);
    if(attempts < 2U)
        attempts = 2U;

    freq_t candidate_rx = state.channel.rx_frequency;
    freq_t candidate_tx = state.channel.tx_frequency;

    while(attempts-- > 0U)
    {
        if((candidate_rx + step) > band_high)
            candidate_rx = band_low;
        else
            candidate_rx += step;

        int64_t candidate_tx_i64 = (int64_t) candidate_rx + rx_tx_delta;
        if(candidate_tx_i64 < 0)
            continue;

        candidate_tx = (freq_t) candidate_tx_i64;

        if(_scan_freq_allowed(candidate_rx) && _scan_freq_allowed(candidate_tx))
        {
            state.channel.rx_frequency = candidate_rx;
            state.channel.tx_frequency = candidate_tx;
            return true;
        }
    }

    return false;
}

static bool _scan_stepChannel(void)
{
    uint16_t count = 0;
    channel_t channel = {0};

    if((state.bank_enabled == false) || !_scan_getBankCount(state.bank, &count))
        return false;

    uint16_t next_index = state.channel_index;

    for(uint16_t attempt = 0; attempt < count; attempt++)
    {
        next_index++;
        if(next_index >= count)
            next_index = 0;

        if(_scan_loadBankChannel(state.bank, next_index, &channel))
        {
            state.channel_index = next_index;
            state.channel = channel;
            return true;
        }
    }

    return false;
}

void scan_reset(void)
{
    memset(&runtime, 0, sizeof(runtime));
}

void scan_notifyModeChange(void)
{
    runtime.active = ((state.tuner_mode == SCAN) || (state.tuner_mode == CHSCAN));
    runtime.primed_step = runtime.active;
    runtime.squelch_open = false;
    runtime.next_step_tick = getTick() + SCAN_SETTLE_DELAY_MS;
    runtime.resume_tick = 0;
}

void scan_task(void)
{
    const bool should_scan = ((state.tuner_mode == SCAN) || (state.tuner_mode == CHSCAN));
    const bool squelch_open = rtx_rxSquelchOpen();
    const long long now = getTick();

    if(!should_scan)
    {
        runtime.active = false;
        runtime.squelch_open = false;
        runtime.resume_tick = 0;
        return;
    }

    if(runtime.active == false)
        scan_notifyModeChange();

    if(runtime.primed_step)
    {
        bool stepped = false;

        if(now < runtime.next_step_tick)
            return;

        if(state.tuner_mode == SCAN)
            stepped = _scan_stepVfo();
        else if(state.tuner_mode == CHSCAN)
            stepped = _scan_stepChannel();

        runtime.primed_step = false;
        runtime.next_step_tick = now + SCAN_SETTLE_DELAY_MS;

        if(stepped)
        {
            state.rtx_sync_pending = true;
            ui_pushEvent(EVENT_STATUS, 0);
        }

        return;
    }

    if(squelch_open)
    {
        runtime.squelch_open = true;
        runtime.resume_tick = 0;
        return;
    }

    if(runtime.squelch_open)
    {
        runtime.squelch_open = false;
        runtime.resume_tick = now + SCAN_RESUME_DELAY_MS;
        return;
    }

    if((runtime.resume_tick != 0) && (now < runtime.resume_tick))
        return;

    if(now < runtime.next_step_tick)
        return;

    bool stepped = false;
    if(state.tuner_mode == SCAN)
        stepped = _scan_stepVfo();
    else if(state.tuner_mode == CHSCAN)
        stepped = _scan_stepChannel();

    if(stepped)
    {
        runtime.next_step_tick = now + SCAN_SETTLE_DELAY_MS;
        runtime.resume_tick = 0;
        state.rtx_sync_pending = true;
        ui_pushEvent(EVENT_STATUS, 0);
    }
}

bool scan_isActive(void)
{
    return runtime.active;
}
