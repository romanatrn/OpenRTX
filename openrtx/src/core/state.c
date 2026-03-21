/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/battery_stats.h"
#include "core/event.h"
#include "core/power.h"
#include "core/dev_console.h"
#include "core/state.h"
#include "core/battery.h"
#include "hwconfig.h"
#include "interfaces/platform.h"
#include "interfaces/nvmem.h"
#include "interfaces/delays.h"
#include "rtx/rtx.h"

state_t state;
pthread_mutex_t state_mutex;
static long long int lastUpdate = 0;
static settings_t lastPersistedSettings;
static long long settingsDirtySince = 0;
static const long long settingsSaveDelay = 3000;

void state_getPersistedSettingsSnapshot(settings_t *settings)
{
    *settings = state.settings;
    settings->usbLogExport = false;
    settings->sqlLevel = default_settings.sqlLevel;
}
static bool m17LogPrimed = false;
static uint8_t lastM17OpMode = OPMODE_NONE;
static uint8_t lastM17OpStatus = OFF;
static bool lastM17LsfOk = false;
static char lastM17Src[10] = {0};
static char lastM17Dst[10] = {0};
static char lastM17Link[10] = {0};
static char lastM17Refl[10] = {0};
static char lastM17Meta[53] = {0};
static char lastM17Route[10] = {0};
static uint8_t lastM17TxCan = 0;
static uint8_t lastM17RxCan = 0;
static bool lastM17CanRxEn = false;
static bool settingsDirtyLogged = false;
static uint8_t lastGpsFixQuality = 0;
static uint8_t lastGpsFixType = 0;
static uint8_t lastGpsTracked = 0;
static uint8_t lastGpsInView = 0;

static void _state_logGpsStatus(const gps_t *gps)
{
    bool hadFix = (lastGpsFixQuality != FIX_QUALITY_NO_FIX);
    bool hasFix = (gps->fix_quality != FIX_QUALITY_NO_FIX);

    if((hadFix == false) && hasFix)
        devConsole_log(DEVLOG_INFO, "GPS", "Fix q=%u type=%u sats=%u/%u",
                       gps->fix_quality, gps->fix_type,
                       gps->satellites_tracked, gps->satellites_in_view);

    if(hadFix && (hasFix == false))
        devConsole_log(DEVLOG_WARN, "GPS", "Fix lost");

    if(hasFix && (lastGpsFixType != gps->fix_type))
        devConsole_log(DEVLOG_INFO, "GPS", "Fix type %u", gps->fix_type);

    if(hasFix && ((lastGpsTracked != gps->satellites_tracked) ||
                  (lastGpsInView != gps->satellites_in_view)))
        devConsole_log(DEVLOG_DEBUG, "GPS", "Sats %u/%u",
                       gps->satellites_tracked, gps->satellites_in_view);

    lastGpsFixQuality = gps->fix_quality;
    lastGpsFixType = gps->fix_type;
    lastGpsTracked = gps->satellites_tracked;
    lastGpsInView = gps->satellites_in_view;
}

static void _state_logM17Status(void)
{
    rtxStatus_t rtx = rtx_getCurrentStatus();

    if(rtx.opMode != OPMODE_M17)
    {
        if(lastM17OpMode == OPMODE_M17)
            devConsole_log(DEVLOG_INFO, "M17", "Mode exit");

        lastM17OpMode = rtx.opMode;
        lastM17OpStatus = rtx.opStatus;
        lastM17LsfOk = false;
        lastM17Src[0] = '\0';
        lastM17Dst[0] = '\0';
        lastM17Link[0] = '\0';
        lastM17Refl[0] = '\0';
        lastM17Meta[0] = '\0';
        lastM17Route[0] = '\0';
        lastM17TxCan = 0;
        lastM17RxCan = 0;
        lastM17CanRxEn = false;
        m17LogPrimed = false;
        return;
    }

    if(m17LogPrimed == false)
    {
        devConsole_log(DEVLOG_INFO, "M17", "Mode enter");
        lastM17OpMode = rtx.opMode;
        lastM17OpStatus = rtx.opStatus;
        lastM17LsfOk = rtx.lsfOk;
        strncpy(lastM17Src, rtx.M17_src, sizeof(lastM17Src));
        strncpy(lastM17Dst, rtx.M17_dst, sizeof(lastM17Dst));
        strncpy(lastM17Link, rtx.M17_link, sizeof(lastM17Link));
        strncpy(lastM17Refl, rtx.M17_refl, sizeof(lastM17Refl));
        strncpy(lastM17Meta, rtx.M17_meta_text, sizeof(lastM17Meta));
        strncpy(lastM17Route, rtx.destination_address, sizeof(lastM17Route));
        lastM17Src[sizeof(lastM17Src) - 1] = '\0';
        lastM17Dst[sizeof(lastM17Dst) - 1] = '\0';
        lastM17Link[sizeof(lastM17Link) - 1] = '\0';
        lastM17Refl[sizeof(lastM17Refl) - 1] = '\0';
        lastM17Meta[sizeof(lastM17Meta) - 1] = '\0';
        lastM17Route[sizeof(lastM17Route) - 1] = '\0';
        lastM17TxCan = rtx.txCan;
        lastM17RxCan = rtx.rxCan;
        lastM17CanRxEn = rtx.canRxEn;
        m17LogPrimed = true;
        return;
    }

    if((lastM17OpStatus != RX) && (rtx.opStatus == RX))
        devConsole_log(DEVLOG_INFO, "M17", "RX active");

    if((lastM17OpStatus != TX) && (rtx.opStatus == TX))
        devConsole_log(DEVLOG_INFO, "M17", "TX active");

    if((lastM17OpStatus != OFF) && (rtx.opStatus == OFF))
        devConsole_log(DEVLOG_INFO, "M17", "Idle");

    if((lastM17LsfOk == false) && rtx.lsfOk)
        devConsole_log(DEVLOG_INFO, "M17", "LSF %s>%s", rtx.M17_src, rtx.M17_dst);

    if(lastM17LsfOk && (rtx.lsfOk == false))
        devConsole_log(DEVLOG_WARN, "M17", "LSF lost");

    if(strncmp(lastM17Route, rtx.destination_address, sizeof(lastM17Route)) != 0)
        devConsole_log(DEVLOG_INFO, "M17", "Route %s", rtx.destination_address);

    if((lastM17TxCan != rtx.txCan) || (lastM17RxCan != rtx.rxCan) ||
       (lastM17CanRxEn != rtx.canRxEn))
        devConsole_log(DEVLOG_INFO, "M17", "CAN tx=%u rx=%u chk=%u",
                       rtx.txCan, rtx.rxCan, rtx.canRxEn ? 1 : 0);

    if(rtx.lsfOk)
    {
        if((rtx.M17_link[0] != '\0') && (strncmp(lastM17Link, rtx.M17_link, sizeof(lastM17Link)) != 0))
            devConsole_log(DEVLOG_INFO, "M17", "Link %s", rtx.M17_link);

        if((rtx.M17_refl[0] != '\0') && (strncmp(lastM17Refl, rtx.M17_refl, sizeof(lastM17Refl)) != 0))
            devConsole_log(DEVLOG_INFO, "M17", "Refl %s", rtx.M17_refl);

        if((rtx.M17_meta_text[0] != '\0') && (strncmp(lastM17Meta, rtx.M17_meta_text, sizeof(lastM17Meta)) != 0))
            devConsole_log(DEVLOG_DEBUG, "M17", "Meta %s", rtx.M17_meta_text);
    }

    lastM17OpMode = rtx.opMode;
    lastM17OpStatus = rtx.opStatus;
    lastM17LsfOk = rtx.lsfOk;
    strncpy(lastM17Src, rtx.M17_src, sizeof(lastM17Src));
    strncpy(lastM17Dst, rtx.M17_dst, sizeof(lastM17Dst));
    strncpy(lastM17Link, rtx.M17_link, sizeof(lastM17Link));
    strncpy(lastM17Refl, rtx.M17_refl, sizeof(lastM17Refl));
    strncpy(lastM17Meta, rtx.M17_meta_text, sizeof(lastM17Meta));
    strncpy(lastM17Route, rtx.destination_address, sizeof(lastM17Route));
    lastM17Src[sizeof(lastM17Src) - 1] = '\0';
    lastM17Dst[sizeof(lastM17Dst) - 1] = '\0';
    lastM17Link[sizeof(lastM17Link) - 1] = '\0';
    lastM17Refl[sizeof(lastM17Refl) - 1] = '\0';
    lastM17Meta[sizeof(lastM17Meta) - 1] = '\0';
    lastM17Route[sizeof(lastM17Route) - 1] = '\0';
    lastM17TxCan = rtx.txCan;
    lastM17RxCan = rtx.rxCan;
    lastM17CanRxEn = rtx.canRxEn;
}

static void _state_normalizePowerSelection(void)
{
    state.channel.power = powerNormalizeStoredValue(state.channel.power,
                                                    state.settings.powerProfile);
}

// Commonly used frequency steps, expressed in Hz
const uint32_t freq_steps[] = { 1000, 5000, 6250, 10000, 12500, 15000,
                                20000, 25000, 50000, 100000 };
const size_t n_freq_steps   = sizeof(freq_steps) / sizeof(freq_steps[0]);


void state_init()
{
    pthread_mutex_init(&state_mutex, NULL);
    devConsole_init();

    /*
     * Try loading settings from nonvolatile memory and default to sane values
     * in case of failure.
     */
    if(nvm_readSettings(&state.settings) < 0)
    {
        state.settings = default_settings;
        strncpy(state.settings.callsign, "OPNRTX", 10);
        strncpy(state.settings.M17_meta_text, "OPENRTX", 53);
    }

    if(state.settings.powerProfile >= POWER_PROFILE_MAX)
        state.settings.powerProfile = POWER_PROFILE_5W;

    if(!bandplanIsValid(state.settings.bandplan))
        state.settings.bandplan = BANDPLAN_CANADA;

    /*
     * Try loading VFO configuration from nonvolatile memory and default to sane
     * values in case of failure.
     */
    if(nvm_readVfoChannelData(&state.channel) < 0)
    {
        state.channel = cps_getDefaultChannel();
    }

    /*
     * Initialise remaining fields
     */
    #ifdef CONFIG_RTC
    state.time = platform_getCurrentTime();
    #endif
    state.v_bat  = platform_getVbat();
    state.charge = battery_getCharge(state.v_bat);
    batteryStatsInit(state.charge);
    state.rssi   = -127.0f;
    state.volume = platform_getVolumeLevel();

    state.channel_index = 0;    // Set default channel index (it is 0-based)
    state.bank_enabled  = false;
    state.bank_is_virtual = false;
    state.rtxStatus     = RTX_OFF;
    state.emergency     = false;
    state.txDisable     = false;
    state.step_index    = 4; // Default frequency step 12.5kHz

    // Force brightness field to be in range 0 - 100
    if(state.settings.brightness > 100)
    {
        state.settings.brightness = 100;
    }

    _state_normalizePowerSelection();

    state.settings.usbLogExport = false;
    state.settings.sqlLevel = default_settings.sqlLevel;
    devConsole_setUsbExportEnabled(false);

    state_getPersistedSettingsSnapshot(&lastPersistedSettings);
}

void state_terminate()
{
    settings_t settingsCopy;

    state_getPersistedSettingsSnapshot(&settingsCopy);

    // Never store a brightness of 0 to avoid booting with a black screen
    if(settingsCopy.brightness == 0)
    {
        settingsCopy.brightness = 5;
    }
    nvm_writeSettingsAndVfo(&settingsCopy, &state.channel);
    pthread_mutex_destroy(&state_mutex);
}

int state_saveSettings()
{
    settings_t settingsCopy;

    state_getPersistedSettingsSnapshot(&settingsCopy);

    if(nvm_writeSettings(&settingsCopy) < 0)
    {
        devConsole_log(DEVLOG_ERROR, "STATE", "Settings save failed");
        return -1;
    }

    lastPersistedSettings = settingsCopy;
    settingsDirtySince = 0;
    devConsole_log(DEVLOG_INFO, "STATE", "Settings saved");
    return 0;
}

void state_task()
{
    bool saveSettings = false;
    settings_t currentPersistedSettings;
    gps_t gpsSnapshot;

    // Update radio state once every 100ms
    if((getTick() - lastUpdate) < 100)
        return;

    lastUpdate = getTick();

    pthread_mutex_lock(&state_mutex);

    /*
     * Low-pass filtering with a time constant of 10s when updated at 1Hz
     * Original computation: state.v_bat = 0.02*vbat + 0.98*state.v_bat
     * Peak error is 18mV when input voltage is 49mV.
     *
     * NOTE: GD77 and DM-1801 already have an hardware low-pass filter on the
     * vbat pin. Adding also the digital one seems to cause more troubles than
     * benefits.
     */
    uint16_t vbat = platform_getVbat();
    #if defined(PLATFORM_GD77) || defined(PLATFORM_DM1801)
    state.v_bat   = vbat;
    #else
    state.v_bat  -= (state.v_bat * 2) / 100;
    state.v_bat  += (vbat * 2) / 100;
    #endif

    /*
     * Update volume level, as a 50% average between previous value and a new
     * read of the knob position. This gives a good reactivity while preventing
     * the volume level to jitter when the knob is not being moved.
     */
    uint16_t vol = platform_getVolumeLevel() + state.volume;
    state.volume = vol / 2;

    state.charge = battery_getCharge(state.v_bat);
    batteryStatsUpdate(state.charge);
    state.rssi = rtx_getRssi();

    #ifdef CONFIG_RTC
    state.time = platform_getCurrentTime();
    #endif

    gpsSnapshot = state.gps_data;

    state_getPersistedSettingsSnapshot(&currentPersistedSettings);

    if(memcmp(&currentPersistedSettings, &lastPersistedSettings,
              sizeof(settings_t)) != 0)
    {
        if(settingsDirtySince == 0)
        {
            settingsDirtySince = lastUpdate;
            if(settingsDirtyLogged == false)
            {
                devConsole_log(DEVLOG_INFO, "STATE", "Settings changed; save pending");
                settingsDirtyLogged = true;
            }
        }
        else if((lastUpdate - settingsDirtySince) >= settingsSaveDelay)
        {
            saveSettings = true;
        }
    }
    else
    {
        settingsDirtySince = 0;
        settingsDirtyLogged = false;
    }

    pthread_mutex_unlock(&state_mutex);

    _state_logM17Status();
    _state_logGpsStatus(&gpsSnapshot);

    if(saveSettings)
    {
        if(state_saveSettings() == 0)
            {}
    }

    ui_pushEvent(EVENT_STATUS, 0);
}

void state_resetSettingsAndVfo()
{
    state.settings = default_settings;
    state.channel  = cps_getDefaultChannel();
    state.settings.usbLogExport = false;
    devConsole_setUsbExportEnabled(false);
    devConsole_log(DEVLOG_WARN, "STATE", "Settings reset to defaults");
}
