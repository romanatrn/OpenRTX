/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/scan.h"

#include "core/state.h"
#include "interfaces/cps_io.h"
#include "interfaces/delays.h"
#include "interfaces/platform.h"
#include "rtx/rtx.h"

static bool scanWasOpen;
static long long scanNextTick;
static long long scanResumeTick;
static uint8_t activeScanList;

static bool loadChannelForScan(uint16_t index, channel_t *channel)
{
    if(state.bank_enabled)
    {
        bankHdr_t bank = {0};

        if(cps_readBankHeader(&bank, state.bank) != 0)
            return false;
        if(index >= bank.ch_count)
            return false;

        const int32_t channelIndex = cps_readBankData(state.bank, index);
        if(channelIndex < 0)
            return false;

        return cps_readChannel(channel, channelIndex + 1) == 0;
    }

    return cps_readChannel(channel, index + 1) == 0;
}

static bool freqWithinLimits(freq_t freq)
{
    bool valid = false;
    const hwInfo_t *hwinfo = platform_getHwInfo();

    if(hwinfo->vhf_band)
    {
        if((freq >= (hwinfo->vhf_minFreq * 1000000u)) &&
           (freq <= (hwinfo->vhf_maxFreq * 1000000u)))
            valid = true;
    }

    if(hwinfo->uhf_band)
    {
        if((freq >= (hwinfo->uhf_minFreq * 1000000u)) &&
           (freq <= (hwinfo->uhf_maxFreq * 1000000u)))
            valid = true;
    }

    return valid;
}

static void getScanBounds(const freq_t current, freq_t *minFreq, freq_t *maxFreq)
{
    const hwInfo_t *hwinfo = platform_getHwInfo();

    *minFreq = 0;
    *maxFreq = 0;

    if(hwinfo->vhf_band &&
       (current >= (hwinfo->vhf_minFreq * 1000000u)) &&
       (current <= (hwinfo->vhf_maxFreq * 1000000u)))
    {
        *minFreq = hwinfo->vhf_minFreq * 1000000u;
        *maxFreq = hwinfo->vhf_maxFreq * 1000000u;
        return;
    }

    if(hwinfo->uhf_band &&
       (current >= (hwinfo->uhf_minFreq * 1000000u)) &&
       (current <= (hwinfo->uhf_maxFreq * 1000000u)))
    {
        *minFreq = hwinfo->uhf_minFreq * 1000000u;
        *maxFreq = hwinfo->uhf_maxFreq * 1000000u;
        return;
    }

    if(hwinfo->vhf_band)
    {
        *minFreq = hwinfo->vhf_minFreq * 1000000u;
        *maxFreq = hwinfo->vhf_maxFreq * 1000000u;
        return;
    }

    if(hwinfo->uhf_band)
    {
        *minFreq = hwinfo->uhf_minFreq * 1000000u;
        *maxFreq = hwinfo->uhf_maxFreq * 1000000u;
    }
}

static void stopScanLocked(void)
{
    if((state.tuner_mode != SCAN) && (state.tuner_mode != CHSCAN))
        return;

    state.tuner_mode = (state.tuner_mode == SCAN) ? VFO : CH;
    scanWasOpen = false;
    scanNextTick = 0;
    scanResumeTick = 0;
    state.rtx_sync_pending = true;
}

static void stepVfoScanLocked(void)
{
    freq_t minFreq;
    freq_t maxFreq;
    const freq_t step = freq_steps[state.step_index];
    const int32_t shift = (int32_t) state.channel.tx_frequency
                        - (int32_t) state.channel.rx_frequency;

    getScanBounds(state.channel.rx_frequency, &minFreq, &maxFreq);
    if((minFreq == 0) || (maxFreq == 0))
        return;

    if((state.channel.rx_frequency + step) > maxFreq)
        state.channel.rx_frequency = minFreq;
    else
        state.channel.rx_frequency += step;

    state.channel.tx_frequency = state.channel.rx_frequency + shift;
    if(!freqWithinLimits(state.channel.tx_frequency))
        state.channel.tx_frequency = state.channel.rx_frequency;

    state.rtx_sync_pending = true;
}

static bool loadScannableChannel(uint16_t index)
{
    channel_t channel;

    while(index < UINT16_MAX)
    {
        if(!loadChannelForScan(index, &channel))
            return false;

        if((channel.scanList_index == 0) || (activeScanList == 0) ||
           (channel.scanList_index == activeScanList))
        {
            state.channel_index = index;
            state.channel = channel;
            return true;
        }

        index += 1;
    }

    return false;
}

static void stepChannelScanLocked(void)
{
    const uint16_t start = state.channel_index + 1;

    if(loadScannableChannel(start))
    {
        state.rtx_sync_pending = true;
        return;
    }

    if(loadScannableChannel(0))
        state.rtx_sync_pending = true;
}

void scan_reset()
{
    scanWasOpen = false;
    scanNextTick = 0;
    scanResumeTick = 0;
    activeScanList = 0;
}

void scan_notifyModeChange()
{
    if((state.tuner_mode == SCAN) || (state.tuner_mode == CHSCAN))
    {
        scanWasOpen = false;
        scanNextTick = getTick();
        scanResumeTick = scanNextTick;
        activeScanList = state.channel.scanList_index;
    }
    else
    {
        scan_reset();
    }
}

bool scan_isActive()
{
    return (state.tuner_mode == SCAN) || (state.tuner_mode == CHSCAN);
}

void scan_task()
{
    const long long now = getTick();

    pthread_mutex_lock(&state_mutex);

    if((state.tuner_mode != SCAN) && (state.tuner_mode != CHSCAN))
    {
        scan_reset();
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    if((state.devStatus != RUNNING) || (state.rtxStatus == RTX_TX))
    {
        stopScanLocked();
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    if(rtx_rxSquelchOpen())
    {
        scanWasOpen = true;
        scanResumeTick = now + 1500;
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    if(scanWasOpen && (now < scanResumeTick))
    {
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    if(scanWasOpen)
    {
        scanWasOpen = false;
        scanNextTick = now;
    }

    if(now < scanNextTick)
    {
        pthread_mutex_unlock(&state_mutex);
        return;
    }

    if(state.tuner_mode == SCAN)
        stepVfoScanLocked();
    else
        stepChannelScanLocked();

    scanNextTick = now + 200;
    pthread_mutex_unlock(&state_mutex);
}
