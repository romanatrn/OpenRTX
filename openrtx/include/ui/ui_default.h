/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_DEFAULT_H
#define UI_DEFAULT_H

#include <stdbool.h>
#include "core/state.h"
#include "core/satellite.h"
#include "core/graphics.h"
#include "interfaces/keyboard.h"
#include <stdint.h>
#include "core/event.h"
#include "hwconfig.h"
#include "core/ui.h"

// Maximum menu entry length
#define MAX_ENTRY_LEN 21
// Frequency digits
#define FREQ_DIGITS 7
// Time & Date digits
#define TIMEDATE_DIGITS 10
// Max number of UI events
#define MAX_NUM_EVENTS 16

enum uiScreen
{
    MAIN_VFO = 0,
    MAIN_VFO_INPUT,
    MAIN_MEM,
    MODE_VFO,
    MODE_MEM,
    MENU_TOP,
    MENU_BANK,
    MENU_BANK_ACTION,
    MENU_BANK_RENAME,
    MENU_CHANNEL,
    MENU_CHANNEL_ACTION,
    MENU_CHANNEL_EDIT,
    MENU_CHANNEL_LOCATION_INPUT,
    MENU_CHANNEL_FREQ_INPUT,
    MENU_CHANNEL_RENAME,
    MENU_CHANNEL_DELETE,
    MENU_CHANNEL_OVERWRITE,
    MENU_CONTACTS,
    MENU_CONTACT_EDIT,
    MENU_CONTACT_RENAME,
    MENU_GAMES,
    MENU_GPS,
    MENU_SATELLITES,
    MENU_SATELLITE_INFO,
    MENU_SETTINGS,
    MENU_BACKUP_RESTORE,
    MENU_BACKUP,
    MENU_RESTORE,
    MENU_INFO,
    MENU_BATTERY_INFO,
    MENU_DEV_CONSOLE,
    MENU_ABOUT,
    GAME_RUN,
    SETTINGS_TIMEDATE,
    SETTINGS_TIMEDATE_SET,
    SETTINGS_DISPLAY,
    SETTINGS_GPS,
    SETTINGS_RADIO,
    SETTINGS_M17,
    SETTINGS_FM,
    SETTINGS_ACCESSIBILITY,
    SETTINGS_RESET2DEFAULTS,
    SETTINGS_FACTORY_RESET,
    LOW_BAT
};

enum SetRxTx
{
    SET_RX = 0,
    SET_TX
};

// This enum is needed to have item numbers that match
// menu elements even if some elements may be missing (GPS)
enum menuItems
{
    M_BANK = 0,
    M_CHANNEL,
    M_CONTACTS,
    M_GAMES,
#ifdef CONFIG_GPS
    M_GPS,
    M_SATELLITES,
#endif
    M_SETTINGS,
    M_INFO,
    M_ABOUT
};

enum settingsItems
{
    S_DISPLAY = 0,
#ifdef CONFIG_RTC
    S_TIMEDATE,
#endif
#ifdef CONFIG_GPS
    S_GPS,
#endif
    S_RADIO,
#ifdef CONFIG_M17
    S_M17,
#endif
    S_FM,
    S_SCAN,
    S_ACCESSIBILITY,
    S_RESET2DEFAULTS,
    S_FACTORY_RESET,
};

enum backupRestoreItems
{
    BR_BACKUP = 0,
    BR_RESTORE
};

enum channelEditItems
{
    CE_RENAME = 0,
    CE_RX_FREQ,
    CE_TX_FREQ,
    CE_MODE,
    CE_BANDWIDTH,
    CE_POWER,
    CE_ZONE,
    CE_SCANLIST,
    CE_LOCATION,
    CE_SAVE,
    CE_DELETE,
    CE_CANCEL
};

enum channelActionItems
{
    CA_OPEN = 0,
    CA_EDIT,
    CA_COPY_TO_VFO,
    CA_SAVE_VFO_HERE,
    CA_DELETE
};

enum bankActionItems
{
    BA_OPEN = 0,
    BA_EDIT,
    BA_DELETE
};

enum contactEditItems
{
    CT_RENAME = 0,
    CT_SAVE,
    CT_DELETE,
    CT_CANCEL
};

enum displayItems
{
#ifdef CONFIG_SCREEN_BRIGHTNESS
    D_BRIGHTNESS = 0,
#endif
#ifdef CONFIG_SCREEN_CONTRAST
    D_CONTRAST,
#endif
    D_TIMER,
    D_BATTERY,
    D_THEME
};

enum uiThemes
{
    THEME_CLASSIC = 0,
    THEME_OCEAN,
    THEME_FOREST,
    THEME_SUNSET,
    THEME_AMBER,
    THEME_PHOSPHOR,
    THEME_CREAM,
    THEME_PLASMA,
    THEME_PALENIGHT,
    THEME_NORD,
    THEME_EVERFOREST
};

#ifdef CONFIG_GPS
enum settingsGPSItems
{
    G_ENABLED = 0,
#ifdef CONFIG_RTC
    G_SET_TIME,
    G_TIMEZONE
#endif
};
#endif

enum settingsAccessibilityItems
{
    A_MACRO_LATCH = 0,
    A_LEVEL,
    A_PHONETIC,
};

enum settingsRadioItems
{
    R_BAND_PLAN,
    R_SHIFT,
    R_DIRECTION,
    R_STEP,
    R_PPM,
    R_POWER_RANGE,
    R_USB_LOG_EXPORT
};

enum settingsM17Items
{
    M17_CALLSIGN = 0,
    M17_METATEXT,
    M17_CAN,
    M17_CAN_RX,
    M17_ENCRYPTION,
    M17_KEY_SLOT,
    M17_KEY_1,
    M17_KEY_2,
    M17_KEY_3,
    M17_KEY_4
};

enum settingsFMItems
{
    FM_RX_TONE,
    FM_TX_TONE,
    FM_TONE_MODE
};

/**
 * Struct containing a set of positions and sizes that get
 * calculated for the selected display size.
 * Using these parameters make the UI automatically adapt
 * To displays of different sizes
 */
typedef struct layout_t
{
    uint16_t hline_h;
    uint16_t top_h;
    uint16_t line1_h;
    uint16_t line2_h;
    uint16_t line3_h;
    uint16_t line3_large_h;
    uint16_t line4_h;
    uint16_t menu_h;
    uint16_t bottom_h;
    uint16_t bottom_pad;
    uint16_t status_v_pad;
    uint16_t horizontal_pad;
    uint16_t text_v_offset;
    point_t top_pos;
    point_t line1_pos;
    point_t line2_pos;
    point_t line3_pos;
    point_t line3_large_pos;
    point_t line4_pos;
    point_t bottom_pos;
    fontSize_t top_font;
    symbolSize_t top_symbol_size;
    fontSize_t line1_font;
    symbolSize_t line1_symbol_size;
    fontSize_t line2_font;
    symbolSize_t line2_symbol_size;
    fontSize_t line3_font;
    symbolSize_t line3_symbol_size;
    fontSize_t line3_large_font;
    fontSize_t line4_font;
    symbolSize_t line4_symbol_size;
    fontSize_t bottom_font;
    fontSize_t input_font;
    fontSize_t menu_font;
    fontSize_t message_font;
} layout_t;

/**
 * This structs contains state variables internal to the
 * UI that need to be kept between executions of the UI
 * This state does not need to be saved on device poweroff
 */
typedef struct ui_state_t
{
    // Index of the currently selected menu entry
    uint8_t menu_selected;
    // If true we can change a menu entry value with UP/DOWN
    bool edit_mode;
    bool input_locked;
    // Variables used for VFO input
    uint8_t input_number;
    uint8_t input_position;
    uint8_t input_set;
    long long last_keypress;
    freq_t new_rx_frequency;
    freq_t new_tx_frequency;
    char new_rx_freq_buf[14];
    char new_tx_freq_buf[14];
    size_t m17_meta_text_scroll_position;
    long long m17_meta_text_last_scroll_tick;
    char new_message[53];
    char new_m17_key[M17_KEY_HEX_LEN + 1];
    bool edit_message;
#ifdef CONFIG_RTC
    // Variables used for Time & Date input
    datetime_t new_timedate;
    char new_date_buf[9];
    char new_time_buf[9];
#endif
    char new_callsign[10];
    char new_channel_name[CPS_STR_SIZE];
    freq_t new_shift;
    uint16_t new_ppm;
    int8_t new_ppm_sign;
    channel_t memory_channel_backup;
    channel_t memory_channel_draft;
    int16_t memory_edit_index;
    int16_t channel_edit_zone;
    uint8_t channel_edit_scanlist;
    int16_t bank_edit_index;
    int16_t contact_edit_index;
    bool memory_edit_active;
    bool memory_edit_new;
    bool memory_edit_from_vfo;
    int32_t channel_edit_latitude_e4;
    int32_t channel_edit_longitude_e4;
    uint8_t channel_edit_location_field;
    uint8_t channel_edit_location_digits;
    long long scan_next_tick;
    long long scan_resume_tick;
    bool scan_was_open;
    uint8_t gps_map_zoom;
    bool gps_map_enabled;
    bool gps_map_manual_pan;
    int32_t gps_map_center_lat;
    int32_t gps_map_center_lon;
    uint8_t battery_page_scroll;
    uint8_t satellite_selected;
    bool satellite_auto_doppler;
    int32_t satellite_manual_bias_hz;
    long long satellite_last_update_tick;
    long long satellite_next_event_tick;
    satellite_prediction_t satellite_prediction;
    // Which state to return to when we exit menu
    uint8_t last_main_state;
#if defined(CONFIG_UI_NO_KEYBOARD)
    uint8_t macro_menu_selected;
#endif // UI_NO_KEYBOARD
}
ui_state_t;

extern layout_t layout;
extern ui_state_t ui_state;
extern state_t last_state;
extern bool    macro_latched;
extern const char *menu_items[];
extern const char *settings_items[];
extern const char *display_items[];
extern const char *settings_gps_items[];
extern const char *settings_radio_items[];
extern const char *settings_m17_items[];
extern const char *settings_fm_items[];
extern const char * settings_accessibility_items[];
extern const char *ui_theme_names[];
extern const char *backup_restore_items[];
extern const char *bank_action_items[];
extern const char *channel_edit_items[];
extern const char *channel_action_items[];
extern const char *contact_edit_items[];
extern const char *info_items[];
extern const char *authors[];
extern const uint8_t menu_num;
extern const uint8_t settings_num;
extern const uint8_t display_num;
extern const uint8_t settings_gps_num;
extern const uint8_t settings_radio_num;
extern const uint8_t settings_m17_num;
extern const uint8_t settings_fm_num;
extern const uint8_t settings_accessibility_num;
extern const uint8_t backup_restore_num;
extern const uint8_t bank_action_num;
extern const uint8_t channel_edit_num;
extern const uint8_t channel_action_num;
extern const uint8_t contact_edit_num;
extern const uint8_t info_num;
extern const uint8_t author_num;
extern color_t color_black;
extern color_t color_grey;
extern color_t color_white;
extern color_t yellow_fab413;

void _ui_clearScreen();

#endif /* UI_DEFAULT_H */
