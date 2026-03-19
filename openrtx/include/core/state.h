/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATE_H
#define STATE_H

#include "core/datatypes.h"
#include "core/settings.h"
#include <pthread.h>
#include <stdbool.h>
#include "core/cps.h"
#include "core/gps.h"

/**
 * Part of this structure has been commented because the corresponding
 * functionality is not yet implemented.
 * Uncomment once the related feature is ready
 */
typedef struct
{
    uint8_t    devStatus;
    datetime_t time;
    uint16_t   v_bat;
    uint8_t    charge;
    rssi_t     rssi;
    uint8_t    volume;

    uint8_t    ui_screen;
    uint8_t    tuner_mode;

    uint16_t   channel_index;
    channel_t  channel;
    channel_t  vfo_channel;
    bool       bank_enabled;
    bool       bank_is_virtual;
    uint16_t   bank;
    uint8_t    rtxStatus;
    bool       tone_enabled;

    bool       emergency;
    settings_t settings;
    gps_t      gps_data;
    bool       gps_set_time;
    bool       gpsDetected;
    bool       backup_eflash;
    bool       restore_eflash;
    bool       txDisable;
    bool       rtx_sync_pending;
    bool       fm_monitor;
    bool       fm_reverse;
    uint8_t    step_index;
}
state_t;

extern const uint32_t freq_steps[];
extern const size_t n_freq_steps;

enum TunerMode
{
    VFO = 0,
    CH,
    SCAN,
    CHSCAN
};

enum RtxStatus
{
    RTX_OFF = 0,
    RTX_RX,
    RTX_TX
};

enum DeviceStatus
{
    PWROFF = 0,
    STARTUP,
    RUNNING,
    DATATRANSFER,
    SHUTDOWN
};

extern state_t state;
extern pthread_mutex_t state_mutex;

/**
 * Initialise radio state mutex and radio state variable, reading the
 * informations from device drivers.
 */
void state_init();

/**
 * Terminate the radio state saving persistent settings to flash and destroy
 * the state mutex.
 */
void state_terminate();

/**
 * Update radio state fetching data from device drivers.
 */
void state_task();

/**
 * Persist current settings immediately and refresh dirty tracking.
 *
 * @return 0 on success, -1 on failure.
 */
int state_saveSettings();

/**
 * Request that the UI thread synchronises the latest state into the RTX layer.
 */
void state_requestRtxSync();

/**
 * Clear and return the pending RTX sync request flag.
 */
bool state_consumeRtxSync();

/**
 * Reset the fields of radio state containing user settings and VFO channel.
 */
void state_resetSettingsAndVfo();

#endif /* STATE_H */
