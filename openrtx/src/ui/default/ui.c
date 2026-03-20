/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * The graphical user interface (GUI) works by splitting the screen in
 * horizontal rows, with row height depending on vertical resolution.
 *
 * The general screen layout is composed by an upper status bar at the
 * top of the screen and a lower status bar at the bottom.
 * The central portion of the screen is filled by two big text/number rows
 * And a small row.
 *
 * Below is shown the row height for two common display densities.
 *
 *        160x128 display (MD380)            Recommended font size
 *      ┌─────────────────────────┐
 *      │  top_status_bar (16px)  │  8 pt (11 px) font with 2 px vertical padding
 *      │      top_pad (4px)      │  4 px padding
 *      │      Line 1 (20px)      │  8 pt (11 px) font with 4 px vertical padding
 *      │      Line 2 (20px)      │  8 pt (11 px) font with 4 px vertical padding
 *      │                         │
 *      │      Line 3 (40px)      │  16 pt (xx px) font with 6 px vertical padding
 *      │ RSSI+squelch bar (20px) │  20 px
 *      │      bottom_pad (4px)   │  4 px padding
 *      └─────────────────────────┘
 *
 *         128x64 display (GD-77)
 *      ┌─────────────────────────┐
 *      │  top_status_bar (11 px) │  6 pt (9 px) font with 1 px vertical padding
 *      │      top_pad (1px)      │  1 px padding
 *      │      Line 1 (10px)      │  6 pt (9 px) font without vertical padding
 *      │      Line 2 (10px)      │  6 pt (9 px) font with 2 px vertical padding
 *      │      Line 3 (18px)      │  12 pt (xx px) font with 0 px vertical padding
 *      │ RSSI+squelch bar (11px) │  11 px
 *      │      bottom_pad (1px)   │  1 px padding
 *      └─────────────────────────┘
 *
 *         128x48 display (RD-5R)
 *      ┌─────────────────────────┐
 *      │  top_status_bar (11 px) │  6 pt (9 px) font with 1 px vertical padding
 *      ├─────────────────────────┤  1 px line
 *      │      Line 2 (10px)      │  8 pt (11 px) font with 4 px vertical padding
 *      │      Line 3 (18px)      │  8 pt (11 px) font with 4 px vertical padding
 *      └─────────────────────────┘
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "ui/ui_default.h"
#include "rtx/rtx.h"
#include "interfaces/platform.h"
#include "interfaces/display.h"
#include "interfaces/cps_io.h"
#include "interfaces/nvmem.h"
#include "interfaces/delays.h"
#include <string.h>
#include "core/battery.h"
#include "core/input.h"
#include "core/repeater.h"
#include "core/power.h"
#include "core/scan.h"
#include "core/dev_console.h"
#include "core/utils.h"
#include "hwconfig.h"
#include "ui/ui_games.h"
#include "ui/gps_map.h"
#include "core/voicePromptUtils.h"
#include "core/beeps.h"

/* UI main screen functions, their implementation is in "ui_main.c" */
extern void _ui_drawMainBackground();
extern void _ui_drawMainTop(ui_state_t* ui_state);
extern void _ui_drawVFOMiddle();
extern void _ui_drawMEMMiddle();
extern void _ui_drawVFOBottom();
extern void _ui_drawMEMBottom();
extern void _ui_drawMainVFO(ui_state_t* ui_state);
extern void _ui_drawMainVFOInput(ui_state_t* ui_state);
extern void _ui_drawMainMEM(ui_state_t* ui_state);
/* UI menu functions, their implementation is in "ui_menu.c" */
extern void _ui_drawMenuTop(ui_state_t* ui_state);
extern void _ui_drawMenuBank(ui_state_t* ui_state);
extern void _ui_drawMenuBankAction(ui_state_t* ui_state);
extern void _ui_drawMenuBankRename(ui_state_t* ui_state);
extern void _ui_drawMenuChannel(ui_state_t* ui_state);
extern void _ui_drawMenuChannelEdit(ui_state_t* ui_state);
extern void _ui_drawMenuChannelAction(ui_state_t* ui_state);
extern void _ui_drawMenuChannelLocationInput(ui_state_t* ui_state);
extern void _ui_drawMenuChannelFreqInput(ui_state_t* ui_state);
extern void _ui_drawMenuChannelRename(ui_state_t* ui_state);
extern void _ui_drawMenuChannelDelete(ui_state_t* ui_state);
extern void _ui_drawMenuChannelOverwrite(ui_state_t* ui_state);
extern void _ui_drawMenuContacts(ui_state_t* ui_state);
extern void _ui_drawMenuContactEdit(ui_state_t* ui_state);
extern void _ui_drawMenuContactRename(ui_state_t* ui_state);
extern void ui_games_drawLibrary(ui_state_t* ui_state);
#ifdef CONFIG_GPS
extern void _ui_drawMenuGPS();
extern void _ui_drawSettingsGPS(ui_state_t* ui_state);
#endif

static void _ui_textInputPreset(char *buf, uint8_t max_len, const char *initial);
extern void _ui_drawSettingsAccessibility(ui_state_t* ui_state);
extern void _ui_drawMenuSettings(ui_state_t* ui_state);
extern void _ui_drawMenuBackupRestore(ui_state_t* ui_state);
extern void _ui_drawMenuBackup(ui_state_t* ui_state);
extern void _ui_drawMenuRestore(ui_state_t* ui_state);
extern void _ui_drawMenuInfo(ui_state_t* ui_state);
extern void _ui_drawMenuDevConsole(ui_state_t* ui_state);
extern void _ui_drawMenuAbout(ui_state_t* ui_state);
#ifdef CONFIG_RTC
extern void _ui_drawSettingsTimeDate();
extern void _ui_drawSettingsTimeDateSet(ui_state_t* ui_state);
#endif
extern void _ui_drawSettingsDisplay(ui_state_t* ui_state);
extern void _ui_drawSettingsM17(ui_state_t* ui_state);
extern void _ui_drawSettingsFM(ui_state_t* ui_state);
extern void _ui_drawSettingsVoicePrompts(ui_state_t* ui_state);
extern void _ui_drawSettingsReset2Defaults(ui_state_t* ui_state);
extern void _ui_drawSettingsFactoryReset(ui_state_t* ui_state);
extern void _ui_drawSettingsRadio(ui_state_t* ui_state);
extern bool _ui_drawMacroMenu(ui_state_t* ui_state);
extern void _ui_reset_menu_anouncement_tracking();
// TODO: get these from ui strings / currentLanguage
const char *menu_items[] =
{
    "Banks",
    "Channels",
    "Contacts",
    "Games",
#ifdef CONFIG_GPS
    "GPS",
#endif
    "Settings",
    "Info",
    "About"
};

const char *settings_items[] =
{
    "Display",
#ifdef CONFIG_RTC
    "Time & Date",
#endif
#ifdef CONFIG_GPS
    "GPS",
#endif
    "Radio",
#ifdef CONFIG_M17
    "M17",
#endif
    "FM",
    "Accessibility",
    "Default Settings",
    "Factory Reset"
};

const char *display_items[] =
{
#ifdef CONFIG_SCREEN_BRIGHTNESS
    "Brightness",
#endif
#ifdef CONFIG_SCREEN_CONTRAST
    "Contrast",
#endif
    "Timer",
    "Battery Icon",
    "Theme"
};

#ifdef CONFIG_GPS
const char *settings_gps_items[] =
{
    "GPS Enabled",
#ifdef CONFIG_RTC
    "GPS Set Time",
    "UTC Timezone"
#endif
};
#endif

const char *settings_radio_items[] =
{
    "Rpt. shift",
    "Direction",
    "Step",
    "Correction",
    "Power Range",
    "USB Log Export",
};

const char * settings_m17_items[] =
{
    "Callsign",
    "Meta Txt",
    "CAN",
    "CAN RX Check",
    "Encryption",
    "Key Slot",
    "Key 1",
    "Key 2",
    "Key 3",
    "Key 4"
};

const char* settings_fm_items[] =
{
    "RX Tone",
    "TX Tone",
    "Tone Mode"
};

const char * settings_accessibility_items[] =
{
    "Macro Latch",
    "Voice",
    "Phonetic"
};

const char *backup_restore_items[] =
{
    "Backup",
    "Restore"
};

const char *bank_action_items[] =
{
    "Open",
    "Edit",
    "Delete"
};

const char *channel_edit_items[] =
{
    "Rename",
    "RX Freq",
    "TX Freq",
    "Mode",
    "Bandwidth",
    "Power",
    "Zone",
    "Scan List",
    "Repeater GPS",
    "Save",
    "Delete",
    "Cancel"
};

const char *channel_action_items[] =
{
    "Open",
    "Edit",
    "Copy to VFO",
    "Scan",
    "Save VFO Here",
    "Delete"
};

const char *contact_edit_items[] =
{
    "Rename",
    "Save",
    "Delete",
    "Cancel"
};

const char *info_items[] =
{
    "",
    "Bat. Voltage",
    "Bat. Charge",
    "RSSI",
    "Used heap",
    "Band",
    "VHF",
    "UHF",
    "Hw Version",
#ifdef PLATFORM_TTWRPLUS
    "Radio",
    "Radio FW",
#endif
    "Developer Console",
};

const char *authors[] =
{
    "Niccolo' IU2KIN",
    "Silvano IU2KWO",
    "Federico IU2NUO",
    "Fred IU2NRO",
    "Joseph VK7JS",
    "Morgan ON4MOD",
    "Marco DM4RCO"
};

static const char *symbols_ITU_T_E161[] =
{
    " 0",
    ",.?1",
    "abc2ABC",
    "def3DEF",
    "ghi4GHI",
    "jkl5JKL",
    "mno6MNO",
    "pqrs7PQRS",
    "tuv8TUV",
    "wxyz9WXYZ",
    "-/*",
    "#"
};

static const char *symbols_ITU_T_E161_callsign[] =
{
    "0 ",
    "1",
    "ABC2",
    "DEF3",
    "GHI4",
    "JKL5",
    "MNO6",
    "PQRS7",
    "TUV8",
    "WXYZ9",
    "-/",
    ""
};

// Calculate number of menu entries
const uint8_t menu_num = sizeof(menu_items)/sizeof(menu_items[0]);
const uint8_t settings_num = sizeof(settings_items)/sizeof(settings_items[0]);
const uint8_t display_num = sizeof(display_items)/sizeof(display_items[0]);
#ifdef CONFIG_GPS
const uint8_t settings_gps_num = sizeof(settings_gps_items)/sizeof(settings_gps_items[0]);
#endif
const uint8_t settings_radio_num = sizeof(settings_radio_items)/sizeof(settings_radio_items[0]);
#ifdef CONFIG_M17
const uint8_t settings_m17_num = sizeof(settings_m17_items)/sizeof(settings_m17_items[0]);
#endif
const uint8_t settings_fm_num = sizeof(settings_fm_items) / sizeof(settings_fm_items[0]);
const uint8_t settings_accessibility_num = sizeof(settings_accessibility_items)/sizeof(settings_accessibility_items[0]);
const uint8_t backup_restore_num = sizeof(backup_restore_items)/sizeof(backup_restore_items[0]);
const uint8_t bank_action_num = sizeof(bank_action_items)/sizeof(bank_action_items[0]);
const uint8_t channel_edit_num = sizeof(channel_edit_items)/sizeof(channel_edit_items[0]);
const uint8_t channel_action_num = sizeof(channel_action_items)/sizeof(channel_action_items[0]);
const uint8_t contact_edit_num = sizeof(contact_edit_items)/sizeof(contact_edit_items[0]);
const uint8_t info_num = sizeof(info_items)/sizeof(info_items[0]);
const uint8_t author_num = sizeof(authors)/sizeof(authors[0]);

typedef struct
{
    color_t background;
    color_t text;
    color_t border;
    color_t accent;
} ui_theme_colors_t;

static const ui_theme_colors_t ui_theme_colors[] =
{
    {{0, 0, 0, 255},    {255, 255, 255, 255}, {60, 60, 60, 255},   {250, 180, 19, 255}},
    {{16, 24, 34, 255}, {235, 247, 255, 255}, {76, 112, 138, 255}, {92, 194, 230, 255}},
    {{16, 24, 18, 255}, {235, 255, 235, 255}, {74, 110, 80, 255},  {142, 216, 150, 255}},
    {{42, 18, 18, 255}, {255, 244, 232, 255}, {148, 84, 56, 255},  {255, 124, 64, 255}},
    {{52, 28, 0, 255},  {255, 214, 150, 255}, {164, 96, 24, 255},  {255, 166, 36, 255}},
    {{6, 26, 6, 255},   {170, 255, 170, 255}, {54, 118, 54, 255},  {118, 255, 118, 255}},
    {{50, 38, 24, 255}, {255, 244, 220, 255}, {150, 118, 72, 255}, {255, 198, 112, 255}},
    {{30, 8, 38, 255},  {244, 226, 255, 255}, {100, 58, 128, 255}, {255, 84, 164, 255}},
    {{41, 45, 62, 255}, {214, 222, 255, 255}, {92, 99, 144, 255},  {130, 170, 255, 255}},
    {{46, 52, 64, 255}, {229, 233, 240, 255}, {94, 129, 172, 255}, {136, 192, 208, 255}},
    {{24, 26, 27, 255}, {240, 238, 232, 255}, {105, 117, 101, 255}, {167, 192, 120, 255}},
};

const char *ui_theme_names[] =
{
    "Classic",
    "Ocean",
    "Forest",
    "Sunset",
    "Amber CRT",
    "Green CRT",
    "Cream",
    "Plasma",
    "Palenight",
    "Nord",
    "Everforest"
};

color_t color_black = {0, 0, 0, 255};
color_t color_grey = {60, 60, 60, 255};
color_t color_white = {255, 255, 255, 255};
color_t yellow_fab413 = {250, 180, 19, 255};

layout_t layout;
state_t last_state;
bool macro_latched;
ui_state_t ui_state;
static bool macro_menu = false;
static bool layout_ready = false;
static bool redraw_needed = true;
static settings_t last_saved_settings;
static long long last_settings_announce_tick = 0;

static bool standby = false;
static long long last_event_tick = 0;

#define UI_GPS_MAP_NS_CENTER_LAT  45100000
#define UI_GPS_MAP_NS_CENTER_LON -62850000

static void _ui_clearTemporaryFmActions(bool *sync_rtx);
static void _ui_stopVfoScan(bool *sync_rtx, bool announce);

static void _ui_resetGPSMapCenter(void)
{
    ui_state.gps_map_manual_pan = false;
    ui_state.gps_map_center_lat = UI_GPS_MAP_NS_CENTER_LAT;
    ui_state.gps_map_center_lon = UI_GPS_MAP_NS_CENTER_LON;
}

static int32_t _ui_gpsMapPanLatStep(uint8_t zoom)
{
    switch(zoom)
    {
        case GPS_MAP_ZOOM_PROVINCE: return 300000;
        case GPS_MAP_ZOOM_REGION:   return 120000;
        case GPS_MAP_ZOOM_TOWN:     return 40000;
        case GPS_MAP_ZOOM_LOCAL:    return 15000;
        default:                    return 4000;
    }
}

static int32_t _ui_gpsMapPanLonStep(uint8_t zoom)
{
    switch(zoom)
    {
        case GPS_MAP_ZOOM_PROVINCE: return 420000;
        case GPS_MAP_ZOOM_REGION:   return 180000;
        case GPS_MAP_ZOOM_TOWN:     return 60000;
        case GPS_MAP_ZOOM_LOCAL:    return 22000;
        default:                    return 6000;
    }
}

// UI event queue
static uint8_t evQueue_rdPos;
static uint8_t evQueue_wrPos;
static event_t evQueue[MAX_NUM_EVENTS];


static void _ui_applyTheme(uint8_t theme)
{
    const ui_theme_colors_t *palette = &ui_theme_colors[theme % (sizeof(ui_theme_colors) / sizeof(ui_theme_colors[0]))];

    color_black = palette->background;
    color_white = palette->text;
    color_grey = palette->border;
    yellow_fab413 = palette->accent;
}

static void _ui_changeTheme(int8_t variation)
{
    uint8_t count = sizeof(ui_theme_names) / sizeof(ui_theme_names[0]);
    state.settings.theme = (state.settings.theme + count + variation) % count;
    _ui_applyTheme(state.settings.theme);
}

void _ui_clearScreen()
{
    gfx_fillScreen(color_black);

    if(CONFIG_SCREEN_HEIGHT > 63)
    {
        gfx_drawHLine(0, 1, color_grey);
        gfx_drawHLine(CONFIG_SCREEN_HEIGHT - 1, 1, color_grey);
    }
}


static void _ui_calculateLayout(layout_t *layout)
{
    // Horizontal line height
    static const uint16_t hline_h = 1;
    // Compensate for fonts printing below the start position
    static const uint16_t text_v_offset = 1;

    // Calculate UI layout depending on vertical resolution
    // Tytera MD380, MD-UV380
    #if CONFIG_SCREEN_HEIGHT > 127

    // Height and padding shown in diagram at beginning of file
    static const uint16_t top_h = 16;
    static const uint16_t top_pad = 4;
    static const uint16_t line1_h = 20;
    static const uint16_t line2_h = 20;
    static const uint16_t line3_h = 20;
    static const uint16_t line3_large_h = 40;
    static const uint16_t line4_h = 20;
    static const uint16_t menu_h = 16;
    static const uint16_t bottom_h = 23;
    static const uint16_t bottom_pad = top_pad;
    static const uint16_t status_v_pad = 2;
    static const uint16_t small_line_v_pad = 2;
    static const uint16_t big_line_v_pad = 6;
    static const uint16_t horizontal_pad = 4;

    // Top bar font: 8 pt
    static const fontSize_t   top_font = FONT_SIZE_8PT;
    static const symbolSize_t top_symbol_size = SYMBOLS_SIZE_8PT;
    // Text line font: 8 pt
    static const fontSize_t line1_font = FONT_SIZE_8PT;
    static const symbolSize_t line1_symbol_size = SYMBOLS_SIZE_8PT;
    static const fontSize_t line2_font = FONT_SIZE_8PT;
    static const symbolSize_t line2_symbol_size = SYMBOLS_SIZE_8PT;
    static const fontSize_t line3_font = FONT_SIZE_8PT;
    static const symbolSize_t line3_symbol_size = SYMBOLS_SIZE_8PT;
    static const fontSize_t line4_font = FONT_SIZE_8PT;
    static const symbolSize_t line4_symbol_size = SYMBOLS_SIZE_8PT;
    // Message font
    const fontSize_t message_font = FONT_SIZE_6PT;
    // Frequency line font: 16 pt
    static const fontSize_t line3_large_font = FONT_SIZE_16PT;
    // Bottom bar font: 8 pt
    static const fontSize_t bottom_font = FONT_SIZE_8PT;
    // TimeDate/Frequency input font
    static const fontSize_t input_font = FONT_SIZE_12PT;
    // Menu font
    static const fontSize_t menu_font = FONT_SIZE_8PT;

    // Radioddity GD-77
    #elif CONFIG_SCREEN_HEIGHT > 63

    // Height and padding shown in diagram at beginning of file
    static const uint16_t top_h = 11;
    static const uint16_t top_pad = 1;
    static const uint16_t line1_h = 10;
    static const uint16_t line2_h = 10;
    static const uint16_t line3_h = 10;
    static const uint16_t line3_large_h = 16;
    static const uint16_t line4_h = 10;
    static const uint16_t menu_h = 10;
    static const uint16_t bottom_h = 15;
    static const uint16_t bottom_pad = 0;
    static const uint16_t status_v_pad = 1;
    static const uint16_t small_line_v_pad = 1;
    static const uint16_t big_line_v_pad = 0;
    static const uint16_t horizontal_pad = 4;

    // Top bar font: 6 pt
    static const fontSize_t   top_font = FONT_SIZE_6PT;
    static const symbolSize_t top_symbol_size = SYMBOLS_SIZE_6PT;
    // Middle line fonts: 5, 8, 8 pt
    static const fontSize_t line1_font = FONT_SIZE_6PT;
    static const symbolSize_t line1_symbol_size = SYMBOLS_SIZE_6PT;
    static const fontSize_t line2_font = FONT_SIZE_6PT;
    static const symbolSize_t line2_symbol_size = SYMBOLS_SIZE_6PT;
    static const fontSize_t line3_font = FONT_SIZE_6PT;
    static const symbolSize_t line3_symbol_size = SYMBOLS_SIZE_6PT;
    static const fontSize_t line3_large_font = FONT_SIZE_10PT;
    static const fontSize_t line4_font = FONT_SIZE_6PT;
    static const symbolSize_t line4_symbol_size = SYMBOLS_SIZE_6PT;
    // Message font
    const fontSize_t message_font = FONT_SIZE_6PT;
    // Bottom bar font: 6 pt
    static const fontSize_t bottom_font = FONT_SIZE_6PT;
    // TimeDate/Frequency input font
    static const fontSize_t input_font = FONT_SIZE_8PT;
    // Menu font
    static const fontSize_t menu_font = FONT_SIZE_6PT;

    // Radioddity RD-5R
    #elif CONFIG_SCREEN_HEIGHT > 47

    // Height and padding shown in diagram at beginning of file
    static const uint16_t top_h = 11;
    static const uint16_t top_pad = 1;
    static const uint16_t line1_h = 0;
    static const uint16_t line2_h = 10;
    static const uint16_t line3_h = 10;
    static const uint16_t line3_large_h = 18;
    static const uint16_t line4_h = 10;
    static const uint16_t menu_h = 10;
    static const uint16_t bottom_h = 0;
    static const uint16_t bottom_pad = 0;
    static const uint16_t status_v_pad = 1;
    static const uint16_t small_line_v_pad = 1;
    static const uint16_t big_line_v_pad = 0;
    static const uint16_t horizontal_pad = 4;

    // Top bar font: 6 pt
    static const fontSize_t   top_font = FONT_SIZE_6PT;
    static const symbolSize_t top_symbol_size = SYMBOLS_SIZE_6PT;
    // Middle line fonts: 16, 16
    static const fontSize_t line2_font = FONT_SIZE_6PT;
    static const fontSize_t line3_font = FONT_SIZE_6PT;
    static const fontSize_t line4_font = FONT_SIZE_6PT;
    static const fontSize_t line3_large_font = FONT_SIZE_12PT;
    // TimeDate/Frequency input font
    static const fontSize_t input_font = FONT_SIZE_8PT;
    // Menu font
    static const fontSize_t menu_font = FONT_SIZE_6PT;
    // Message font
    const fontSize_t message_font = FONT_SIZE_6PT;
    // Not present on this resolution
    static const fontSize_t line1_font = 0;
    static const fontSize_t bottom_font = 0;

    #else
    #error Unsupported vertical resolution!
    #endif

    // Calculate printing positions
    static const uint16_t top_pos   = top_h - status_v_pad - text_v_offset;
    static const uint16_t line1_pos = top_h + top_pad + line1_h - small_line_v_pad - text_v_offset;
    static const uint16_t line2_pos = top_h + top_pad + line1_h + line2_h - small_line_v_pad - text_v_offset;
    static const uint16_t line3_pos = top_h + top_pad + line1_h + line2_h + line3_h - small_line_v_pad - text_v_offset;
    static const uint16_t line4_pos = top_h + top_pad + line1_h + line2_h + line3_h + line4_h - small_line_v_pad - text_v_offset;
    static const uint16_t line3_large_pos = top_h + top_pad + line1_h + line2_h + line3_large_h - big_line_v_pad - text_v_offset;
    static const uint16_t bottom_pos = CONFIG_SCREEN_HEIGHT - bottom_pad - status_v_pad - text_v_offset;

    layout_t new_layout =
    {
        hline_h,
        top_h,
        line1_h,
        line2_h,
        line3_h,
        line3_large_h,
        line4_h,
        menu_h,
        bottom_h,
        bottom_pad,
        status_v_pad,
        horizontal_pad,
        text_v_offset,
        {horizontal_pad, top_pos},
        {horizontal_pad, line1_pos},
        {horizontal_pad, line2_pos},
        {horizontal_pad, line3_pos},
        {horizontal_pad, line3_large_pos},
        {horizontal_pad, line4_pos},
        {horizontal_pad, bottom_pos},
        top_font,
        top_symbol_size,
        line1_font,
        line1_symbol_size,
        line2_font,
        line2_symbol_size,
        line3_font,
        line3_symbol_size,
        line3_large_font,
        line4_font,
        line4_symbol_size,
        bottom_font,
        input_font,
        menu_font,
        message_font
    };

    memcpy(layout, &new_layout, sizeof(layout_t));
}

static void _ui_drawLowBatteryScreen()
{
    gfx_clearScreen();
    uint16_t bat_width = CONFIG_SCREEN_WIDTH / 2;
    uint16_t bat_height = CONFIG_SCREEN_HEIGHT / 3;
    point_t bat_pos = {CONFIG_SCREEN_WIDTH / 4, CONFIG_SCREEN_HEIGHT / 8};
    gfx_drawBattery(bat_pos, bat_width, bat_height, 10);
    point_t text_pos_1 = {0, CONFIG_SCREEN_HEIGHT * 2 / 3};
    point_t text_pos_2 = {0, CONFIG_SCREEN_HEIGHT * 2 / 3 + 16};

    gfx_print(text_pos_1,
              FONT_SIZE_6PT,
              TEXT_ALIGN_CENTER,
              color_white,
              currentLanguage->forEmergencyUse);
    gfx_print(text_pos_2,
              FONT_SIZE_6PT,
              TEXT_ALIGN_CENTER,
              color_white,
              currentLanguage->pressAnyButton);
}

static freq_t _ui_freq_add_digit(freq_t freq, uint8_t pos, uint8_t number)
{
    freq_t coefficient = 100;
    for(uint8_t i=0; i < FREQ_DIGITS - pos; i++)
    {
        coefficient *= 10;
    }
    return freq += number * coefficient;
}

#ifdef CONFIG_RTC
static void _ui_timedate_add_digit(datetime_t *timedate, uint8_t pos,
                                   uint8_t number)
{
    vp_flush();
    vp_queueInteger(number);
    if (pos == 2 || pos == 4)
        vp_queuePrompt(PROMPT_SLASH);
    // just indicates separation of date and time.
    if (pos==6) // start of time.
        vp_queueString("hh:mm", vpAnnounceCommonSymbols|vpAnnounceLessCommonSymbols);
    if (pos == 8)
        vp_queuePrompt(PROMPT_COLON);
    vp_play();

    switch(pos)
    {
        // Set date
        case 1:
            timedate->date += number * 10;
            break;
        case 2:
            timedate->date += number;
            break;
        // Set month
        case 3:
            timedate->month += number * 10;
            break;
        case 4:
            timedate->month += number;
            break;
        // Set year
        case 5:
            timedate->year += number * 10;
            break;
        case 6:
            timedate->year += number;
            break;
        // Set hour
        case 7:
            timedate->hour += number * 10;
            break;
        case 8:
            timedate->hour += number;
            break;
        // Set minute
        case 9:
            timedate->minute += number * 10;
            break;
        case 10:
            timedate->minute += number;
            break;
    }
}
#endif

static bool _ui_freq_check_limits(freq_t freq)
{
    bool valid = false;
    const hwInfo_t* hwinfo = platform_getHwInfo();
    if(hwinfo->vhf_band)
    {
        // hwInfo_t frequencies are in MHz
        if(freq >= (hwinfo->vhf_minFreq * 1000000) &&
           freq <= (hwinfo->vhf_maxFreq * 1000000))
        valid = true;
    }
    if(hwinfo->uhf_band)
    {
        // hwInfo_t frequencies are in MHz
        if(freq >= (hwinfo->uhf_minFreq * 1000000) &&
           freq <= (hwinfo->uhf_maxFreq * 1000000))
        valid = true;
    }
    return valid;
}

static bool _ui_channel_valid(channel_t* channel)
{
return _ui_freq_check_limits(channel->rx_frequency) &&
       _ui_freq_check_limits(channel->tx_frequency);
}

static int16_t _ui_findZoneForChannel(const uint16_t channel_index);
static channel_t *_ui_getMemoryEditChannel(void);

static bool _ui_isVirtualBankActive(void)
{
    return state.bank_enabled && state.bank_is_virtual;
}

static int _ui_getBankChannelCount(uint16_t bank_index, uint16_t *count)
{
    if(count == NULL)
        return -1;

    if(_ui_isVirtualBankActive() && (bank_index == REPEATER_NEAREST_BANK))
    {
        *count = repeater_getNearestCount(&state.gps_data);
        return 0;
    }

    bankHdr_t bank = {0};
    if(cps_readBankHeader(&bank, bank_index) == -1)
        return -1;

    *count = bank.ch_count;
    return 0;
}

static int32_t _ui_getBankChannelIndex(uint16_t bank_index, uint16_t channel_index)
{
    if(_ui_isVirtualBankActive() && (bank_index == REPEATER_NEAREST_BANK))
        return repeater_getNearestChannelIndex(&state.gps_data, channel_index);

    return cps_readBankData(bank_index, channel_index);
}

static uint8_t _ui_getLocationFieldDigits(uint8_t field)
{
    return (field == 0U) ? 6U : 7U;
}

static uint8_t _ui_countLocationDigits(int32_t value_e4)
{
    uint32_t abs_value = (value_e4 < 0) ? (uint32_t) (-value_e4) : (uint32_t) value_e4;
    uint8_t digits = 0U;

    while(abs_value > 0U)
    {
        abs_value /= 10U;
        digits++;
    }

    return digits;
}

static int32_t _ui_getLocationFieldValue(uint8_t field)
{
    return (field == 0U) ? ui_state.channel_edit_latitude_e4
                         : ui_state.channel_edit_longitude_e4;
}

static void _ui_setLocationFieldValue(uint8_t field, int32_t value_e4)
{
    if(field == 0U)
        ui_state.channel_edit_latitude_e4 = value_e4;
    else
        ui_state.channel_edit_longitude_e4 = value_e4;
}

static bool _ui_getChannelLocationE4(const channel_t *channel, int32_t *lat_e4,
                                     int32_t *lon_e4)
{
    int32_t lat_e6;
    int32_t lon_e6;

    if(!repeater_getChannelLocation(channel, &lat_e6, &lon_e6))
    {
        if(lat_e4 != NULL)
            *lat_e4 = 0;
        if(lon_e4 != NULL)
            *lon_e4 = 0;
        return false;
    }

    if(lat_e4 != NULL)
        *lat_e4 = lat_e6 / 100;
    if(lon_e4 != NULL)
        *lon_e4 = lon_e6 / 100;

    return true;
}

static bool _ui_setChannelLocationE4(channel_t *channel, int32_t lat_e4, int32_t lon_e4)
{
    int32_t lat_int;
    int32_t lon_int;
    int32_t lat_dec;
    int32_t lon_dec;

    if(channel == NULL)
        return false;
    if((lat_e4 < -900000) || (lat_e4 > 900000))
        return false;
    if((lon_e4 < -1800000) || (lon_e4 > 1800000))
        return false;
    if((lat_e4 == 0) && (lon_e4 == 0))
        return false;

    lat_int = lat_e4 / 10000;
    lon_int = lon_e4 / 10000;
    lat_dec = lat_e4 % 10000;
    lon_dec = lon_e4 % 10000;

    if(lat_dec < 0)
        lat_dec = -lat_dec;
    if(lon_dec < 0)
        lon_dec = -lon_dec;

    channel->ch_location.ch_lat_int = (int8_t) lat_int;
    channel->ch_location.ch_lat_dec = (uint16_t) lat_dec;
    channel->ch_location.ch_lon_int = (int16_t) lon_int;
    channel->ch_location.ch_lon_dec = (uint16_t) lon_dec;
    channel->ch_location.ch_altitude = 0;
    return true;
}

static void _ui_beginChannelLocationInput(void)
{
    _ui_getChannelLocationE4(&ui_state.memory_channel_draft,
                             &ui_state.channel_edit_latitude_e4,
                             &ui_state.channel_edit_longitude_e4);
    ui_state.channel_edit_location_field = 0U;
    ui_state.channel_edit_location_digits = _ui_countLocationDigits(ui_state.channel_edit_latitude_e4);
    state.ui_screen = MENU_CHANNEL_LOCATION_INPUT;
}

static void _ui_locationInputAppendDigit(uint8_t digit)
{
    const uint8_t max_digits = _ui_getLocationFieldDigits(ui_state.channel_edit_location_field);
    int32_t value = _ui_getLocationFieldValue(ui_state.channel_edit_location_field);
    const bool negative = (value < 0);
    uint32_t abs_value = negative ? (uint32_t) (-value) : (uint32_t) value;

    if(ui_state.channel_edit_location_digits >= max_digits)
        return;

    abs_value = (abs_value * 10U) + digit;
    ui_state.channel_edit_location_digits++;
    _ui_setLocationFieldValue(ui_state.channel_edit_location_field,
                              negative ? -(int32_t) abs_value : (int32_t) abs_value);
}

static void _ui_locationInputBackspace(void)
{
    int32_t value = _ui_getLocationFieldValue(ui_state.channel_edit_location_field);
    const bool negative = (value < 0);
    uint32_t abs_value = negative ? (uint32_t) (-value) : (uint32_t) value;

    abs_value /= 10U;
    if(ui_state.channel_edit_location_digits > 0U)
        ui_state.channel_edit_location_digits--;

    _ui_setLocationFieldValue(ui_state.channel_edit_location_field,
                              negative ? -(int32_t) abs_value : (int32_t) abs_value);
}

static void _ui_locationInputToggleSign(void)
{
    _ui_setLocationFieldValue(ui_state.channel_edit_location_field,
                              -_ui_getLocationFieldValue(ui_state.channel_edit_location_field));
}

static bool _ui_confirmChannelLocationInput(void)
{
    if(ui_state.channel_edit_location_field == 0U)
    {
        ui_state.channel_edit_location_field = 1U;
        ui_state.channel_edit_location_digits = _ui_countLocationDigits(ui_state.channel_edit_longitude_e4);
        return false;
    }

    if(!_ui_setChannelLocationE4(&ui_state.memory_channel_draft,
                                 ui_state.channel_edit_latitude_e4,
                                 ui_state.channel_edit_longitude_e4))
    {
        vp_announceError(vpqInit);
        return false;
    }

    state.ui_screen = MENU_CHANNEL_EDIT;
    return true;
}

static void _ui_cycleChannelMode(int8_t direction)
{
    (void) direction;
    channel_t *channel = _ui_getMemoryEditChannel();

#ifdef CONFIG_M17
    if(channel->mode == OPMODE_FM)
        channel->mode = OPMODE_M17;
    else
        channel->mode = OPMODE_FM;
#else
    channel->mode = OPMODE_FM;
#endif
}

static void _ui_cycleChannelBandwidth(int8_t direction);
static void _ui_cycleChannelZone(int8_t direction);
static void _ui_cycleChannelScanList(int8_t direction);
static void _ui_beginChannelLocationInput(void);
static void _ui_prepareBankRename(int16_t bank_index);
static void _ui_prepareContactEdit(int16_t contact_index);
static void _ui_announceStoreError();

static int16_t _ui_resolveChannelStorageIndex(int16_t channel_index)
{
    if(channel_index < 0)
        return -1;

    if(state.bank_enabled)
    {
        uint16_t bank_count = 0;

        if(_ui_getBankChannelCount(state.bank, &bank_count) == -1)
            return -1;
        if(channel_index >= bank_count)
            return -1;

        return _ui_getBankChannelIndex(state.bank, channel_index);
    }

    return channel_index;
}

static channel_t *_ui_getMemoryEditChannel()
{
    if(ui_state.memory_edit_active)
        return &ui_state.memory_channel_draft;

    return &state.channel;
}

static void _ui_resetMemoryEditSession()
{
    ui_state.memory_edit_active = false;
    ui_state.memory_edit_new = false;
    ui_state.memory_edit_from_vfo = false;
    ui_state.memory_edit_index = -1;
    ui_state.channel_edit_zone = -1;
    ui_state.channel_edit_scanlist = 0;
}

static void _ui_beginMemoryEditSession(int16_t channel_index, const channel_t *source,
                                       bool isNew, bool fromVfo)
{
    ui_state.memory_channel_backup = state.channel;
    ui_state.memory_channel_draft = *source;
    ui_state.memory_edit_active = true;
    ui_state.memory_edit_new = isNew;
    ui_state.memory_edit_from_vfo = fromVfo;
    ui_state.memory_edit_index = channel_index;

    if(channel_index >= 0)
        ui_state.channel_edit_zone = _ui_findZoneForChannel((uint16_t) channel_index);
    else if(state.bank_enabled && !state.bank_is_virtual)
        ui_state.channel_edit_zone = state.bank;
    else
        ui_state.channel_edit_zone = -1;

    ui_state.channel_edit_scanlist = ui_state.memory_channel_draft.scanList_index;
    ui_state.menu_selected = 0;
    ui_state.edit_mode = false;
}

static void _ui_cancelMemoryEditSession(bool *sync_rtx)
{
    state.channel = ui_state.memory_channel_backup;
    *sync_rtx = true;

    if(ui_state.memory_edit_new && ui_state.memory_edit_from_vfo)
        state.ui_screen = MAIN_VFO;
    else
        state.ui_screen = MAIN_MEM;

    _ui_resetMemoryEditSession();
}

static void _ui_resetCodeplug(bool *sync_rtx)
{
    channel_t preserved_vfo = state.vfo_channel;

    if(ui_state.last_main_state == MAIN_VFO)
        preserved_vfo = state.channel;

    if(!_ui_channel_valid(&preserved_vfo))
        preserved_vfo = cps_getDefaultChannel();

    if(cps_create(NULL) < 0)
    {
        _ui_announceStoreError();
        return;
    }

    state.bank_enabled = false;
    state.bank_is_virtual = false;
    state.bank = 0;
    state.channel_index = 0;
    state.vfo_channel = preserved_vfo;
    state.channel = preserved_vfo;
    *sync_rtx = true;
}

static void _ui_cycleChannelPower(int8_t direction)
{
    channel_t *channel = _ui_getMemoryEditChannel();
#if defined(PLATFORM_MDUV3x0)
    channel->power = powerNormalizeStoredValue(channel->power,
                                               state.settings.powerProfile);
    channel->power = powerGetNextStep(channel->power, direction,
                                      state.settings.powerProfile);
#else
    if(direction > 0)
        channel->power = (channel->power == 1000) ? 5000 : 1000;
    else
        channel->power = (channel->power == 5000) ? 1000 : 5000;
#endif
}

static bool _ui_drawDarkOverlay()
{
    color_t alpha_grey = {0, 0, 0, 255};
    point_t origin = {0, 0};
    gfx_drawRect(origin, CONFIG_SCREEN_WIDTH, CONFIG_SCREEN_HEIGHT, alpha_grey, true);
    return true;
}

static int _ui_fsm_loadChannel(int16_t channel_index, bool *sync_rtx)
{
    channel_t channel;
    int32_t selected_channel = channel_index;
    // If a bank is active, get index from current bank
    if(state.bank_enabled)
    {
        uint16_t bank_count = 0;

        if(_ui_getBankChannelCount(state.bank, &bank_count) == -1)
            return -1;
        if((channel_index < 0) || (channel_index >= bank_count))
            return -1;
        channel_index = _ui_getBankChannelIndex(state.bank, channel_index);
    }

    int result = cps_readChannel(&channel, channel_index + 1);
    // Read successful and channel is valid
    if((result != -1) && _ui_channel_valid(&channel))
    {
        _ui_clearTemporaryFmActions(sync_rtx);
        // Set new channel index
        state.channel_index = selected_channel;
        // Copy channel read to state
        state.channel = channel;
        *sync_rtx = true;
    }

    return result;
}

static uint16_t _ui_getChannelCount()
{
    channel_t channel;
    uint16_t index = 0;

    while((index < UINT16_MAX) && (cps_readChannel(&channel, index + 1) != -1))
        index++;

    return index;
}

static uint16_t _ui_getBankCount()
{
    bankHdr_t bank;
    uint16_t index = 0;

    while((index < UINT16_MAX) && (cps_readBankHeader(&bank, index) != -1))
        index++;

    return index;
}

static uint16_t _ui_getContactCount()
{
    contact_t contact;
    uint16_t index = 0;

    while((index < UINT16_MAX) && (cps_readContact(&contact, index + 1) != -1))
        index++;

    return index;
}

static int16_t _ui_findZoneForChannel(const uint16_t channel_index)
{
    const uint16_t bank_count = _ui_getBankCount();

    for(uint16_t bank = 0; bank < bank_count; bank++)
    {
        bankHdr_t header = {0};

        if(cps_readBankHeader(&header, bank) == -1)
            continue;

        for(uint16_t pos = 0; pos < header.ch_count; pos++)
        {
            if(cps_readBankData(bank, pos) == channel_index)
                return bank;
        }
    }

    return -1;
}

static void _ui_prepareBankRename(int16_t bank_index)
{
    bankHdr_t bank = {0};

    ui_state.bank_edit_index = bank_index;
    if(bank_index >= 0 && cps_readBankHeader(&bank, (uint16_t) bank_index) != -1)
        _ui_textInputPreset(ui_state.new_channel_name, 15, bank.name);
    else
        _ui_textInputPreset(ui_state.new_channel_name, 15, "");
}

static void _ui_prepareContactEdit(int16_t contact_index)
{
    contact_t contact = {0};

    ui_state.contact_edit_index = contact_index;
    if(contact_index >= 0 && cps_readContact(&contact, (uint16_t) contact_index + 1) != -1)
        strncpy(ui_state.new_channel_name, contact.name, sizeof(ui_state.new_channel_name));
    else
        _ui_textInputPreset(ui_state.new_channel_name, 15, "");

    ui_state.menu_selected = 0;
    ui_state.edit_mode = false;
}

static void _ui_cycleChannelBandwidth(int8_t direction)
{
    (void) direction;
    channel_t *channel = _ui_getMemoryEditChannel();
    channel->bandwidth = (channel->bandwidth == BW_12_5) ? BW_25 : BW_12_5;
}

static void _ui_cycleChannelZone(int8_t direction)
{
    const int16_t bank_count = (int16_t) _ui_getBankCount();

    if(bank_count <= 0)
    {
        ui_state.channel_edit_zone = -1;
        return;
    }

    if(direction > 0)
    {
        if(ui_state.channel_edit_zone >= (bank_count - 1))
            ui_state.channel_edit_zone = -1;
        else
            ui_state.channel_edit_zone++;
    }
    else
    {
        if(ui_state.channel_edit_zone < 0)
            ui_state.channel_edit_zone = bank_count - 1;
        else
            ui_state.channel_edit_zone--;
    }
}

static void _ui_cycleChannelScanList(int8_t direction)
{
    if(direction > 0)
        ui_state.channel_edit_scanlist = (ui_state.channel_edit_scanlist + 1) % 251;
    else if(ui_state.channel_edit_scanlist == 0)
        ui_state.channel_edit_scanlist = 250;
    else
        ui_state.channel_edit_scanlist--;

    ui_state.memory_channel_draft.scanList_index = ui_state.channel_edit_scanlist;
}

static int _ui_applyChannelZoneSelection(const uint16_t channel_index)
{
    const uint16_t bank_count = _ui_getBankCount();

    for(uint16_t bank = 0; bank < bank_count; bank++)
    {
        bankHdr_t header = {0};

        if(cps_readBankHeader(&header, bank) == -1)
            continue;

        for(uint16_t pos = 0; pos < header.ch_count; pos++)
        {
            if(cps_readBankData(bank, pos) == channel_index)
            {
                if(cps_deleteBankData(bank, pos) == -1)
                    return -1;

                header.ch_count--;
                pos--;
            }
        }
    }

    if(ui_state.channel_edit_zone >= 0)
    {
        bankHdr_t header = {0};
        if(cps_readBankHeader(&header, ui_state.channel_edit_zone) == -1)
            return -1;

        if(cps_insertBankData(channel_index, ui_state.channel_edit_zone, header.ch_count) == -1)
            return -1;
    }

    return 0;
}

static void _ui_announceChannelStored(const uint16_t channel_index)
{
    if(state.settings.vpLevel < vpLow)
        return;

    vp_flush();
    vp_queuePrompt(PROMPT_CHANNEL);
    vp_queueInteger(channel_index + 1);
    vp_play();
}

static void _ui_announceStoreError()
{
    if(state.settings.vpLevel < vpLow)
        return;

    vp_flush();
    vp_queueStringTableEntry(&currentLanguage->error);
    vp_play();
}

static void _ui_fsm_storeVfoToNewChannel(bool *sync_rtx)
{
    channel_t new_channel = state.channel;
    uint16_t channel_index = _ui_getChannelCount();

    if(new_channel.name[0] == '\0')
        snprintf(new_channel.name, sizeof(new_channel.name), "CH-%04u", channel_index + 1);

    if(cps_insertChannel(new_channel, channel_index) == -1)
    {
        _ui_announceStoreError();
        return;
    }

    repeater_invalidateNearestCache();

    state.vfo_channel = state.channel;
    state.bank_enabled = false;
    state.bank_is_virtual = false;

    if(_ui_fsm_loadChannel(channel_index, sync_rtx) == -1)
    {
        _ui_announceStoreError();
        return;
    }

    state.ui_screen = MAIN_MEM;
    _ui_announceChannelStored(state.channel_index);
}

static void _ui_fsm_beginNewChannel(bool *sync_rtx)
{
    channel_t new_channel = state.channel;
    uint16_t channel_index = _ui_getChannelCount();
    bool fromVfo = (ui_state.last_main_state == MAIN_VFO);

    (void) sync_rtx;

    if((new_channel.name[0] == '\0') || (ui_state.last_main_state != MAIN_MEM))
        snprintf(new_channel.name, sizeof(new_channel.name), "CH-%04u", channel_index + 1);

    _ui_beginMemoryEditSession(-1, &new_channel, true, fromVfo);
    state.ui_screen = MENU_CHANNEL_EDIT;
}

static void _ui_fsm_beginChannelFrequencyInput(enum SetRxTx field)
{
    channel_t *channel = _ui_getMemoryEditChannel();

    ui_state.input_set = field;
    ui_state.input_position = 0;
    ui_state.new_rx_frequency = channel->rx_frequency;
    ui_state.new_tx_frequency = channel->tx_frequency;
    memset(ui_state.new_rx_freq_buf, 0, sizeof(ui_state.new_rx_freq_buf));
    memset(ui_state.new_tx_freq_buf, 0, sizeof(ui_state.new_tx_freq_buf));
    state.ui_screen = MENU_CHANNEL_FREQ_INPUT;
}

static void _ui_fsm_confirmChannelFrequencyInput(bool *sync_rtx)
{
    freq_t new_freq = (ui_state.input_set == SET_RX) ? ui_state.new_rx_frequency
                                                     : ui_state.new_tx_frequency;
    channel_t *channel = _ui_getMemoryEditChannel();

    (void) sync_rtx;

    if(_ui_freq_check_limits(new_freq))
    {
        if(ui_state.input_set == SET_RX)
            channel->rx_frequency = new_freq;
        else
            channel->tx_frequency = new_freq;

        vp_announceFrequencies(new_freq, new_freq, vpqInit);
    }
    else
    {
        vp_announceError(vpqInit);
    }

    state.ui_screen = MENU_CHANNEL_EDIT;
}

static void _ui_fsm_storeCurrentMemoryChannel(bool *sync_rtx)
{
    channel_t channel = ui_state.memory_channel_draft;
    channel.scanList_index = ui_state.channel_edit_scanlist;

    if(ui_state.memory_edit_new)
    {
        uint16_t channel_index = _ui_getChannelCount();

        if(cps_insertChannel(channel, channel_index) == -1)
        {
            _ui_announceStoreError();
            return;
        }

        repeater_invalidateNearestCache();

        if(_ui_fsm_loadChannel(channel_index, sync_rtx) == -1)
        {
            _ui_announceStoreError();
            return;
        }

        if(_ui_applyChannelZoneSelection(channel_index) == -1)
        {
            _ui_announceStoreError();
            return;
        }

        _ui_fsm_loadChannel(channel_index, sync_rtx);
        _ui_announceChannelStored(state.channel_index);
        _ui_resetMemoryEditSession();
        return;
    }

    const int16_t storage_index = _ui_resolveChannelStorageIndex(ui_state.memory_edit_index);

    if((storage_index < 0) || (cps_writeChannel(channel, storage_index) == -1))
    {
        _ui_announceStoreError();
        return;
    }

    repeater_invalidateNearestCache();

    if(_ui_applyChannelZoneSelection(storage_index) == -1)
    {
        _ui_announceStoreError();
        return;
    }

    if(_ui_fsm_loadChannel(ui_state.memory_edit_index, sync_rtx) == -1)
    {
        _ui_announceStoreError();
        return;
    }

    _ui_announceChannelStored(state.channel_index);
    _ui_resetMemoryEditSession();
}

static void _ui_fsm_confirmVFOInput(bool *sync_rtx)
{
    const bool single_field_input = (state.ui_screen == MENU_CHANNEL_FREQ_INPUT);
    const uint8_t return_screen = (state.ui_screen == MENU_CHANNEL_FREQ_INPUT)
                               ? MENU_CHANNEL_EDIT
                               : MAIN_VFO;

    vp_flush();
    // Switch to TX input
    if(ui_state.input_set == SET_RX)
    {
        if(single_field_input)
        {
            if(_ui_freq_check_limits(ui_state.new_rx_frequency))
            {
                state.channel.rx_frequency = ui_state.new_rx_frequency;
                *sync_rtx = true;
                vp_queueFrequency(state.channel.rx_frequency);
            }
            else
            {
                vp_announceError(vpqInit);
            }

            state.ui_screen = return_screen;
            vp_play();
            return;
        }

        ui_state.input_set = SET_TX;
        // Reset input position
        ui_state.input_position = 0;
        // announce the rx frequency just confirmed with Enter.
        vp_queueFrequency(ui_state.new_rx_frequency);
        // defer playing till the end.
        // indicate that the user has moved to the tx freq field.
        vp_announceInputReceiveOrTransmit(true, vpqDefault);
    }
    else if(ui_state.input_set == SET_TX)
    {
        // Save new frequency setting
        // If TX frequency was not set, TX = RX
        if(ui_state.new_tx_frequency == 0)
        {
            ui_state.new_tx_frequency = ui_state.new_rx_frequency;
        }
        // Apply new frequencies if they are valid
        if(_ui_freq_check_limits(ui_state.new_rx_frequency) &&
           _ui_freq_check_limits(ui_state.new_tx_frequency))
        {
            state.channel.rx_frequency = ui_state.new_rx_frequency;
            state.channel.tx_frequency = ui_state.new_tx_frequency;
            *sync_rtx = true;
            // force init to clear any prompts in progress.
            // defer play because play is called at the end of the function
            //due to above freq queuing.
            vp_announceFrequencies(state.channel.rx_frequency,
                                   state.channel.tx_frequency, vpqInit);
        }
        else
        {
            vp_announceError(vpqInit);
        }

        state.ui_screen = return_screen;
    }

    vp_play();
}

static void _ui_fsm_insertVFONumber(kbd_msg_t msg, bool *sync_rtx)
{
    const bool single_field_input = (state.ui_screen == MENU_CHANNEL_FREQ_INPUT);
    const uint8_t return_screen = (state.ui_screen == MENU_CHANNEL_FREQ_INPUT)
                               ? MENU_CHANNEL_EDIT
                               : MAIN_VFO;

    // Advance input position
    ui_state.input_position += 1;
    // clear any prompts in progress.
    vp_flush();
    // Save pressed number to calculate frequency and show in GUI
    ui_state.input_number = input_getPressedNumber(msg);
    // queue the digit just pressed.
    vp_queueInteger(ui_state.input_number);
    // queue  point if user has entered three digits.
    if (ui_state.input_position == 3)
        vp_queuePrompt(PROMPT_POINT);

    if(ui_state.input_set == SET_RX)
    {
        if(ui_state.input_position == 1)
            ui_state.new_rx_frequency = 0;
        // Calculate portion of the new RX frequency
        ui_state.new_rx_frequency = _ui_freq_add_digit(ui_state.new_rx_frequency,
                                                       ui_state.input_position,
                                                       ui_state.input_number);
        if(ui_state.input_position >= FREQ_DIGITS)
        {
            if(single_field_input)
            {
                if(_ui_freq_check_limits(ui_state.new_rx_frequency))
                {
                    state.channel.rx_frequency = ui_state.new_rx_frequency;
                    *sync_rtx = true;
                    vp_queueFrequency(state.channel.rx_frequency);
                }
                else
                {
                    vp_announceError(vpqInit);
                }

                state.ui_screen = return_screen;
            }
            else
            {
                // queue the rx freq just completed.
                vp_queueFrequency(ui_state.new_rx_frequency);
                // now queue tx as user has changed fields.
                vp_queuePrompt(PROMPT_TRANSMIT);
                // Switch to TX input
                ui_state.input_set = SET_TX;
                // Reset input position
                ui_state.input_position = 0;
                // Reset TX frequency
                ui_state.new_tx_frequency = 0;
            }
        }
    }
    else if(ui_state.input_set == SET_TX)
    {
        if(ui_state.input_position == 1)
            ui_state.new_tx_frequency = 0;
        // Calculate portion of the new TX frequency
        ui_state.new_tx_frequency = _ui_freq_add_digit(ui_state.new_tx_frequency,
                                                       ui_state.input_position,
                                                       ui_state.input_number);
        if(ui_state.input_position >= FREQ_DIGITS)
        {
            if(single_field_input)
            {
                if(_ui_freq_check_limits(ui_state.new_tx_frequency))
                {
                    state.channel.tx_frequency = ui_state.new_tx_frequency;
                    *sync_rtx = true;
                    vp_queueFrequency(state.channel.tx_frequency);
                }
                else
                {
                    vp_announceError(vpqInit);
                }
            }
            else if(_ui_freq_check_limits(ui_state.new_rx_frequency) &&
                    _ui_freq_check_limits(ui_state.new_tx_frequency))
            {
                state.channel.rx_frequency = ui_state.new_rx_frequency;
                state.channel.tx_frequency = ui_state.new_tx_frequency;
                *sync_rtx = true;
                // play is called at end.
                vp_announceFrequencies(state.channel.rx_frequency,
                                       state.channel.tx_frequency, vpqInit);
            }

            state.ui_screen = return_screen;
        }
    }

    vp_play();
}

#ifdef CONFIG_SCREEN_BRIGHTNESS
static void _ui_changeBrightness(int variation)
{
    state.settings.brightness += variation;

    // Max value for brightness is 100, min value is set to 5 to avoid complete
    //  display shutdown.
    if(state.settings.brightness > 100) state.settings.brightness = 100;
    if(state.settings.brightness < 5)   state.settings.brightness = 5;

    display_setBacklightLevel(state.settings.brightness);
}
#endif

#ifdef CONFIG_SCREEN_CONTRAST
static void _ui_changeContrast(int variation)
{
    if(variation >= 0)
        state.settings.contrast =
        (255 - state.settings.contrast < variation) ? 255 : state.settings.contrast + variation;
    else
        state.settings.contrast =
        (state.settings.contrast < -variation) ? 0 : state.settings.contrast + variation;

    display_setContrast(state.settings.contrast);
}
#endif

static void _ui_changeTimer(int variation)
{
    if ((state.settings.display_timer == TIMER_OFF && variation < 0) ||
        (state.settings.display_timer == TIMER_1H && variation > 0))
    {
        return;
    }

    state.settings.display_timer += variation;
}

static void _ui_changeMacroLatch(bool newVal)
{
    state.settings.macroMenuLatch = newVal ? 1 : 0;
    vp_announceSettingsOnOffToggle(&currentLanguage->macroLatching,
                                   vp_getVoiceLevelQueueFlags(),
                                   state.settings.macroMenuLatch);
}

#ifdef CONFIG_M17
static inline void _ui_changeM17Can(int variation)
{
    uint8_t can = state.settings.m17_can;
    state.settings.m17_can = (can + variation) % 16;
}

static void _ui_changeM17Encryption(int variation)
{
    static const uint8_t modes[] = {
        PLAIN,
        SCRAMBLER,
        SCRAMBLER,
        SCRAMBLER,
        AES
    };
    static const uint8_t subtypes[] = {
        0,
        0,
        1,
        2,
        0
    };

    int index = 0;
    if(state.settings.m17_default_encryption == AES)
        index = 4;
    else if(state.settings.m17_default_encryption == SCRAMBLER)
        index = 1 + state.settings.m17_default_enc_subtype;

    index += variation;
    if(index < 0)
        index = 0;
    if(index > 4)
        index = 4;

    state.settings.m17_default_encryption = modes[index];
    state.settings.m17_default_enc_subtype = subtypes[index];
}

static void _ui_changeM17KeySlot(int variation)
{
    int slot = state.settings.m17_default_key_index + variation;
    if(slot < 0)
        slot = 0;
    if(slot > M17_KEY_SLOTS)
        slot = M17_KEY_SLOTS;
    state.settings.m17_default_key_index = slot;
}
#endif

static void _ui_changeVoiceLevel(int variation)
{
    if ((state.settings.vpLevel == vpNone && variation < 0) ||
        (state.settings.vpLevel == vpHigh && variation > 0))
        {
            return;
        }

    state.settings.vpLevel += variation;

    // Force these flags to ensure the changes are spoken for levels 1 through 3.
    vpQueueFlags_t flags = vpqInit
                         | vpqAddSeparatingSilence
                         | vpqPlayImmediately;

    if (!vp_isPlaying())
    {
        flags |= vpqIncludeDescriptions;
    }

    vp_announceSettingsVoiceLevel(flags);
}

static void _ui_changePhoneticSpell(bool newVal)
{
    state.settings.vpPhoneticSpell = newVal ? 1 : 0;

    vp_announceSettingsOnOffToggle(&currentLanguage->phonetic,
                                   vp_getVoiceLevelQueueFlags(),
                                   state.settings.vpPhoneticSpell);
}

bool _ui_checkStandby(long long time_since_last_event)
{
    if (standby)
    {
        return false;
    }

    switch (state.settings.display_timer)
    {
        case TIMER_OFF:
            return false;
        case TIMER_5S:
        case TIMER_10S:
        case TIMER_15S:
        case TIMER_20S:
        case TIMER_25S:
        case TIMER_30S:
            return time_since_last_event >= (5000 * state.settings.display_timer);
        case TIMER_1M:
        case TIMER_2M:
        case TIMER_3M:
        case TIMER_4M:
        case TIMER_5M:
            return time_since_last_event >=
                (60000 * (state.settings.display_timer - (TIMER_1M - 1)));
        case TIMER_15M:
        case TIMER_30M:
        case TIMER_45M:
            return time_since_last_event >=
                (60000 * 15 * (state.settings.display_timer - (TIMER_15M - 1)));
        case TIMER_1H:
            return time_since_last_event >= 60 * 60 * 1000;
    }

    // unreachable code
    return false;
}

static void _ui_enterStandby()
{
    if(standby)
        return;

    standby = true;
    redraw_needed = false;
    display_setBacklightLevel(0);
}

static bool _ui_exitStandby(long long now)
{
    last_event_tick = now;

    if(!standby)
        return false;

    standby = false;
    redraw_needed = true;
    display_setBacklightLevel(state.settings.brightness);

    return true;
}

// TODO: find a better home for this function
int _ui_handleToneSelectScroll(bool direction_up)
{
    bool tone_tx_enable = state.channel.fm.txToneEn;
    bool tone_rx_enable = state.channel.fm.rxToneEn;
    uint8_t tone_flags = tone_tx_enable << 1 | tone_rx_enable;

    if(direction_up) 
        tone_flags++;
    else
        tone_flags--;

    tone_flags %= 4;
    tone_tx_enable = tone_flags >> 1;
    tone_rx_enable = tone_flags & 1;
    state.channel.fm.txToneEn = tone_tx_enable;
    state.channel.fm.rxToneEn = tone_rx_enable;

    return 1;
}

static void _ui_announceQuickActionState(const char *label, bool enabled)
{
    if(state.settings.vpLevel < vpLow)
        return;

    vp_flush();
    vp_queueString(label, vpAnnounceCommonSymbols);
    vp_queueStringTableEntry(enabled ? &currentLanguage->on : &currentLanguage->off);
    vp_play();
}

static void _ui_toggleMonitor(bool *sync_rtx)
{
    if(state.channel.mode != OPMODE_FM)
        return;

    state.fm_monitor = !state.fm_monitor;
    *sync_rtx = true;
    _ui_announceQuickActionState("Monitor", state.fm_monitor);
}

static void _ui_toggleReverse(bool *sync_rtx)
{
    if(state.channel.mode != OPMODE_FM)
        return;

    freq_t tmp = state.channel.rx_frequency;
    state.channel.rx_frequency = state.channel.tx_frequency;
    state.channel.tx_frequency = tmp;
    state.fm_reverse = !state.fm_reverse;
    *sync_rtx = true;

    if(state.settings.vpLevel >= vpLow)
    {
        vp_flush();
        vp_queueString("Reverse", vpAnnounceCommonSymbols);
        vp_queueStringTableEntry(state.fm_reverse ? &currentLanguage->on
                                                  : &currentLanguage->off);
        vp_queuePrompt(PROMPT_RECEIVE);
        vp_queueFrequency(state.channel.rx_frequency);
        vp_queuePrompt(PROMPT_TRANSMIT);
        vp_queueFrequency(state.channel.tx_frequency);
        vp_play();
    }
}

static void _ui_clearTemporaryFmActions(bool *sync_rtx)
{
    bool changed = false;

    if(state.fm_monitor)
    {
        state.fm_monitor = false;
        changed = true;
    }

    if(state.fm_reverse)
    {
        freq_t tmp = state.channel.rx_frequency;
        state.channel.rx_frequency = state.channel.tx_frequency;
        state.channel.tx_frequency = tmp;
        state.fm_reverse = false;
        changed = true;
    }

    if(changed)
        *sync_rtx = true;
}

static void _ui_announceScanState(const char *label, bool enabled)
{
    if(state.settings.vpLevel < vpLow)
        return;

    vp_flush();
    vp_queueString(label, vpAnnounceCommonSymbols);
    vp_queueStringTableEntry(enabled ? &currentLanguage->on : &currentLanguage->off);
    vp_play();
}

static void _ui_stopVfoScan(bool *sync_rtx, bool announce)
{
    if(state.tuner_mode != SCAN)
        return;

    state.tuner_mode = VFO;
    scan_notifyModeChange();
    if(announce)
        _ui_announceScanState("VFO Scan", false);
    *sync_rtx = true;
}

static void _ui_stopChannelScan(bool *sync_rtx, bool announce)
{
    if(state.tuner_mode != CHSCAN)
        return;

    state.tuner_mode = CH;
    scan_notifyModeChange();
    if(announce)
        _ui_announceScanState("Channel Scan", false);
    *sync_rtx = true;
}

static void _ui_toggleVfoScan(bool *sync_rtx)
{
    if(state.ui_screen != MAIN_VFO)
        return;

    if(state.tuner_mode == SCAN)
    {
        _ui_stopVfoScan(sync_rtx, true);
        return;
    }

    state.tuner_mode = SCAN;
    scan_notifyModeChange();
    _ui_announceScanState("VFO Scan", true);
    *sync_rtx = true;
}

static void _ui_toggleChannelScan(bool *sync_rtx)
{
    if(state.ui_screen != MAIN_MEM)
        return;

    if(state.tuner_mode == CHSCAN)
    {
        _ui_stopChannelScan(sync_rtx, true);
        return;
    }

    state.tuner_mode = CHSCAN;
    scan_notifyModeChange();
    _ui_announceScanState("Channel Scan", true);
    *sync_rtx = true;
}

static void _ui_copyMemoryToVfo(const uint16_t channel_index, bool *sync_rtx)
{
    state.bank_enabled = false;
    state.bank_is_virtual = false;

    if(_ui_fsm_loadChannel(channel_index, sync_rtx) == -1)
    {
        _ui_announceStoreError();
        return;
    }

    state.vfo_channel = state.channel;
    state.ui_screen = MAIN_VFO;
    state.tuner_mode = VFO;
}

static int _ui_overwriteMemoryWithVfo(const uint16_t channel_index)
{
    channel_t channel = {0};
    const int16_t storage_index = _ui_resolveChannelStorageIndex(channel_index);

    if(storage_index < 0)
        return -1;

    if(cps_readChannel(&channel, storage_index + 1) == -1)
        return -1;

    channel_t replacement = state.channel;
    if(replacement.name[0] == '\0')
        strncpy(replacement.name, channel.name, sizeof(replacement.name));

    if(cps_writeChannel(replacement, storage_index) == -1)
        return -1;

    repeater_invalidateNearestCache();
    return 0;
}

static void _ui_fsm_menuMacro(kbd_msg_t msg, bool *sync_rtx)
{
    // If there is no keyboard left and right select the menu entry to edit
#if defined(CONFIG_UI_NO_KEYBOARD)
    if (msg.keys & KNOB_LEFT)
    {
        ui_state.macro_menu_selected--;
        ui_state.macro_menu_selected += 9;
        ui_state.macro_menu_selected %= 9;
    }
    if (msg.keys & KNOB_RIGHT)
    {
        ui_state.macro_menu_selected++;
        ui_state.macro_menu_selected %= 9;
    }
    if ((msg.keys & KEY_ENTER) && !msg.long_press)
        ui_state.input_number = ui_state.macro_menu_selected + 1;
    else
        ui_state.input_number = 0;
#else // CONFIG_UI_NO_KEYBOARD
    ui_state.input_number = input_getPressedNumber(msg);
#endif // CONFIG_UI_NO_KEYBOARD
    // CTCSS Encode/Decode Selection
    vpQueueFlags_t queueFlags = vp_getVoiceLevelQueueFlags();

    switch(ui_state.input_number)
    {
        case 1:
            if(state.channel.mode == OPMODE_FM)
            {
                _ui_handleToneSelectScroll(true);
                *sync_rtx                 = true;
                vp_announceCTCSS(
                    state.channel.fm.rxToneEn, state.channel.fm.rxTone,
                    state.channel.fm.txToneEn, state.channel.fm.txTone,
                    queueFlags | vpqIncludeDescriptions);
            }
            break;
        case 2:
            if (state.channel.mode == OPMODE_FM)
            {
                if (state.channel.fm.txTone == 0)
                {
                    state.channel.fm.txTone = CTCSS_FREQ_NUM-1;
                }
                else
                {
                    state.channel.fm.txTone--;
                }

                state.channel.fm.txTone %= CTCSS_FREQ_NUM;
                state.channel.fm.rxTone = state.channel.fm.txTone;
                *sync_rtx = true;
                vp_announceCTCSS(state.channel.fm.rxToneEn,
                                 state.channel.fm.rxTone,
                                 state.channel.fm.txToneEn,
                                 state.channel.fm.txTone,
                                 queueFlags);
            }
            break;

        case 3:
            if(state.channel.mode == OPMODE_FM)
            {
                state.channel.fm.txTone++;
                state.channel.fm.txTone %= CTCSS_FREQ_NUM;
                state.channel.fm.rxTone = state.channel.fm.txTone;
                *sync_rtx = true;
                vp_announceCTCSS(state.channel.fm.rxToneEn,
                                 state.channel.fm.rxTone,
                                 state.channel.fm.txToneEn,
                                 state.channel.fm.txTone,
                                 queueFlags |vpqIncludeDescriptions);
            }
            break;
        case 4:
            if(state.channel.mode == OPMODE_FM)
            {
                if(msg.long_press)
                {
                    _ui_toggleMonitor(sync_rtx);
                }
                else
                {
                    state.channel.bandwidth++;
                    state.channel.bandwidth %= 2;
                    *sync_rtx = true;
                    vp_announceBandwidth(state.channel.bandwidth, queueFlags);
                }
            }
            break;
        case 5:
            if(msg.long_press && (state.ui_screen == MAIN_VFO))
            {
                _ui_toggleVfoScan(sync_rtx);
            }
            else if(msg.long_press && (state.ui_screen == MAIN_MEM))
            {
                _ui_toggleChannelScan(sync_rtx);
            }
            else if(msg.long_press && (state.channel.mode == OPMODE_FM))
            {
                _ui_toggleReverse(sync_rtx);
            }
            else
            {
                _ui_cycleChannelMode(+1);
                *sync_rtx = true;
                vp_announceRadioMode(state.channel.mode, queueFlags);
            }
            break;
        case 6:
            if(msg.long_press && (state.ui_screen == MAIN_VFO))
            {
                _ui_clearTemporaryFmActions(sync_rtx);
                _ui_fsm_storeVfoToNewChannel(sync_rtx);
                macro_menu = false;
                macro_latched = false;
            }
            else if(msg.long_press && (state.ui_screen == MAIN_MEM))
            {
                _ui_clearTemporaryFmActions(sync_rtx);
                _ui_beginMemoryEditSession(state.channel_index, &state.channel, false, false);
                state.ui_screen = MENU_CHANNEL_EDIT;
                macro_menu = false;
                macro_latched = false;
            }
            else
            {
                _ui_cycleChannelPower(+1);
                *sync_rtx = true;
                vp_announcePower(state.channel.power, queueFlags);
            }
            break;
#ifdef CONFIG_SCREEN_BRIGHTNESS
        case 7:
            _ui_changeBrightness(-5);
            vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                   state.settings.brightness);
            break;
        case 8:
            _ui_changeBrightness(+5);
            vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                   state.settings.brightness);
            break;
#endif
        case 9:
            if (!ui_state.input_locked)
                ui_state.input_locked = true;
            else
                ui_state.input_locked = false;
            break;
    }

#if defined(PLATFORM_TTWRPLUS)
    if(msg.keys & KEY_VOLDOWN)
#else
    if(msg.keys & KEY_LEFT || msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
#endif // PLATFORM_TTWRPLUS
    {
#ifdef CONFIG_KNOB_ABSOLUTE // If the radio has an absolute position knob
        state.settings.sqlLevel = platform_getChSelector() - 1;
#endif // CONFIG_KNOB_ABSOLUTE
        if(state.settings.sqlLevel > 0)
        {
            state.settings.sqlLevel -= 1;
            *sync_rtx = true;
            vp_announceSquelch(state.settings.sqlLevel, queueFlags);
        }
    }

#if defined(PLATFORM_TTWRPLUS)
    else if(msg.keys & KEY_VOLUP)
#else
    else if(msg.keys & KEY_RIGHT || msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
#endif // PLATFORM_TTWRPLUS
    {
#ifdef CONFIG_KNOB_ABSOLUTE
        state.settings.sqlLevel = platform_getChSelector() - 1;
#endif
        if(state.settings.sqlLevel < 15)
        {
            state.settings.sqlLevel += 1;
            *sync_rtx = true;
            vp_announceSquelch(state.settings.sqlLevel, queueFlags);
        }
    }
}

static void _ui_menuUp(uint8_t menu_entries)
{
    if(ui_state.menu_selected > 0)
        ui_state.menu_selected -= 1;
    else
        ui_state.menu_selected = menu_entries - 1;
    vp_playMenuBeepIfNeeded(ui_state.menu_selected==0);
}

static void _ui_menuDown(uint8_t menu_entries)
{
    if(ui_state.menu_selected < menu_entries - 1)
        ui_state.menu_selected += 1;
    else
        ui_state.menu_selected = 0;
    vp_playMenuBeepIfNeeded(ui_state.menu_selected==0);
}

static void _ui_menuBack(uint8_t prev_state)
{
    if(ui_state.edit_mode)
    {
        ui_state.edit_mode = false;
    }
    else
    {
        // Return to previous menu
        state.ui_screen = prev_state;
        // Reset menu selection
        ui_state.menu_selected = 0;
        vp_playMenuBeepIfNeeded(true);
    }
}

static void _ui_textInputReset(char *buf)
{
    ui_state.input_number = 0;
    ui_state.input_position = 0;
    ui_state.input_set = 0;
    ui_state.last_keypress = 0;
    memset(buf, 0, 9);
    buf[0] = '_';
}

static void _ui_textInputPreset(char *buf, uint8_t max_len, const char *initial)
{
    ui_state.input_number = 0;
    ui_state.input_position = 0;
    ui_state.input_set = 0;
    ui_state.last_keypress = 0;

    memset(buf, 0, max_len + 1);
    strncpy(buf, initial, max_len);

    uint8_t len = strnlen(buf, max_len);
    if(len < max_len)
    {
        buf[len] = '_';
        ui_state.input_position = len;
    }
    else if(max_len > 0)
    {
        ui_state.input_position = max_len - 1;
    }
}

static void _ui_textInputKeypad(char *buf, uint8_t max_len, kbd_msg_t msg,
                         bool callsign)
{
    long long now = getTick();
    // Get currently pressed number key
    uint8_t num_key = input_getPressedChar(msg);

    bool key_timeout = ((now - ui_state.last_keypress) >= input_longPressTimeout);
    bool same_key = ui_state.input_number == num_key;
    // Get number of symbols related to currently pressed key
    uint8_t num_symbols = 0;
    if(callsign)
    {
        num_symbols = strlen(symbols_ITU_T_E161_callsign[num_key]);
        if(num_symbols == 0)
            return;
    }
    else
        num_symbols = strlen(symbols_ITU_T_E161[num_key]);

    // Return if max length is reached or finished editing last character
    if((ui_state.input_position >= max_len) || ((ui_state.input_position == (max_len-1)) && (key_timeout || !same_key)))
        return;

    // Skip keypad logic for first keypress
    if(ui_state.last_keypress != 0)
    {
        // Same key pressed and timeout not expired: cycle over chars of current key
        if(same_key && !key_timeout)
        {
            ui_state.input_set = (ui_state.input_set + 1) % num_symbols;
        }
        // Different key pressed: save current char and change key
        else
        {
            ui_state.input_position += 1;
            ui_state.input_set = 0;
        }
    }
    // Show current character on buffer
    if(callsign)
        buf[ui_state.input_position] = symbols_ITU_T_E161_callsign[num_key][ui_state.input_set];
    else
    {
        buf[ui_state.input_position] = symbols_ITU_T_E161[num_key][ui_state.input_set];
    }
    // Announce the character
    vp_announceInputChar(buf[ui_state.input_position]);
    // Update reference values
    ui_state.input_number = num_key;
    ui_state.last_keypress = now;
}

static void _ui_textInputConfirm(char *buf)
{
    buf[ui_state.input_position + 1] = '\0';
}

static void _ui_textInputDel(char *buf)
{
    // announce the char about to be backspaced.
    // Note this assumes editing callsign.
    // If we edit a different buffer which allows the underline char, we may
    // not want to exclude it, but when editing callsign, we do not want to say
    // underline since it means the field is empty.
    if(buf[ui_state.input_position]
    && buf[ui_state.input_position]!='_')
        vp_announceInputChar(buf[ui_state.input_position]);

    buf[ui_state.input_position] = '\0';
    // Move back input cursor
    if(ui_state.input_position > 0)
    {
        ui_state.input_position--;
    // If we deleted the initial character, reset starting condition
    }
    else
        ui_state.last_keypress = 0;
    ui_state.input_set = 0;
}

static void _ui_numberInputKeypad(uint32_t *num, kbd_msg_t msg)
{
    long long now = getTick();

#ifdef CONFIG_UI_NO_KEYBOARD
    // If knob is turned, increment or Decrement
    if (msg.keys & KNOB_LEFT)
    {
        *num = *num + 1;
        if (*num % 10 == 0)
            *num = *num - 10;
    }

    if (msg.keys & KNOB_RIGHT)
    {
        if (*num == 0)
            *num = 9;
        else
        {
            *num = *num - 1;
            if (*num % 10 == 9)
                *num = *num + 10;
        }
    }

    // If enter is pressed, advance to the next digit
    if (msg.keys & KEY_ENTER)
        *num *= 10;

    // Announce the character
    vp_announceInputChar('0' + *num % 10);

    // Update reference values
    ui_state.input_number = *num % 10;
#else
    // Maximum frequency len is uint32_t max value number of decimal digits
    if(ui_state.input_position >= 10)
        return;

    // Get currently pressed number key
    uint8_t num_key = input_getPressedNumber(msg);
    *num *= 10;
    *num += num_key;

    // Announce the character
    vp_announceInputChar('0' + num_key);

    // Update reference values
    ui_state.input_number = num_key;
#endif

    ui_state.last_keypress = now;
}

static void _ui_numberInputDel(uint32_t *num)
{
    // announce the digit about to be backspaced.
    vp_announceInputChar('0' + *num % 10);

    *num /= 10;

    // Move back input cursor
    if(ui_state.input_position > 0)
        ui_state.input_position--;
    else
        ui_state.last_keypress = 0;

    ui_state.input_set = 0;
}

static bool _ui_isSettingsScreen(const uint8_t screen)
{
    switch(screen)
    {
        case MENU_SETTINGS:
        case SETTINGS_TIMEDATE:
        case SETTINGS_TIMEDATE_SET:
        case SETTINGS_DISPLAY:
        case SETTINGS_GPS:
        case SETTINGS_RADIO:
        case SETTINGS_M17:
        case SETTINGS_FM:
        case SETTINGS_ACCESSIBILITY:
        case SETTINGS_RESET2DEFAULTS:
            return true;
        default:
            return false;
    }
}

static void _ui_saveSettingsOnExit(bool leaving_settings)
{
    settings_t persisted_settings;

    if(leaving_settings == false)
        return;

    state_getPersistedSettingsSnapshot(&persisted_settings);

    if(memcmp(&persisted_settings, &last_saved_settings, sizeof(settings_t)) != 0)
        if(state_saveSettings() == 0)
            last_saved_settings = persisted_settings;
}

static bool _ui_shouldAnnounceSettingChange(long long now)
{
    if((now - last_settings_announce_tick) < 250)
        return false;

    last_settings_announce_tick = now;
    return true;
}

void ui_init()
{
    last_event_tick = getTick();
    redraw_needed = true;
    _ui_calculateLayout(&layout);
    layout_ready = true;
    // Initialize struct ui_state to all zeroes
    // This syntax is called compound literal
    // https://stackoverflow.com/questions/6891720/initialize-reset-struct-to-zero-null
    ui_state = (const struct ui_state_t){ 0 };
    ui_state.gps_map_zoom = GPS_MAP_ZOOM_PROVINCE;
    _ui_resetGPSMapCenter();
    _ui_resetMemoryEditSession();
    state_getPersistedSettingsSnapshot(&last_saved_settings);
    last_settings_announce_tick = 0;
    ui_games_init();
    gps_map_resetTrack();
}

void ui_drawSplashScreen()
{
    gfx_clearScreen();

    #if CONFIG_SCREEN_HEIGHT > 64
    static const point_t    logo_orig = {0, (CONFIG_SCREEN_HEIGHT / 2) - 6};
    static const point_t    call_orig = {0, CONFIG_SCREEN_HEIGHT - 8};
    static const fontSize_t logo_font = FONT_SIZE_12PT;
    static const fontSize_t call_font = FONT_SIZE_8PT;
    #else
    static const point_t    logo_orig = {0, 19};
    static const point_t    call_orig = {0, CONFIG_SCREEN_HEIGHT - 8};
    static const fontSize_t logo_font = FONT_SIZE_8PT;
    static const fontSize_t call_font = FONT_SIZE_6PT;
    #endif

    gfx_print(logo_orig, logo_font, TEXT_ALIGN_CENTER, yellow_fab413, "O P N\nR T X");
    gfx_print(call_orig, call_font, TEXT_ALIGN_CENTER, color_white, state.settings.callsign);

    vp_announceSplashScreen();
}

void ui_saveState()
{
    last_state = state;
}

#ifdef CONFIG_GPS
static uint16_t priorGPSSpeed = 0;
static int16_t  priorGPSAltitude = 0;
static int16_t  priorGPSDirection = 500; // impossible value init.
static uint8_t  priorGPSFixQuality= 0;
static uint8_t  priorGPSFixType = 0;
static uint8_t  priorSatellitesInView = 0;
static uint32_t vpGPSLastUpdate = 0;

static vpGPSInfoFlags_t GetGPSDirectionOrSpeedChanged()
{
    if (!state.settings.gps_enabled)
        return vpGPSNone;

    uint32_t now = getTick();
    if (now - vpGPSLastUpdate < 8000)
        return vpGPSNone;

    vpGPSInfoFlags_t whatChanged=  vpGPSNone;

    if (state.gps_data.fix_quality != priorGPSFixQuality)
    {
        whatChanged |= vpGPSFixQuality;
        priorGPSFixQuality= state.gps_data.fix_quality;
    }

    if (state.gps_data.fix_type != priorGPSFixType)
    {
        whatChanged |= vpGPSFixType;
        priorGPSFixType = state.gps_data.fix_type;
    }

    if (state.gps_data.speed != priorGPSSpeed)
    {
        whatChanged |= vpGPSSpeed;
        priorGPSSpeed = state.gps_data.speed;
    }

    if (state.gps_data.altitude != priorGPSAltitude)
    {
        whatChanged |= vpGPSAltitude;
        priorGPSAltitude = state.gps_data.altitude;
    }

    if (state.gps_data.tmg_true != priorGPSDirection)
    {
        whatChanged |= vpGPSDirection;
        priorGPSDirection = state.gps_data.tmg_true;
    }

    if (state.gps_data.satellites_in_view != priorSatellitesInView)
    {
        whatChanged |= vpGPSSatCount;
        priorSatellitesInView = state.gps_data.satellites_in_view;
    }

    if (whatChanged)
        vpGPSLastUpdate=now;

    return whatChanged;
}
#endif // CONFIG_GPS

void ui_updateFSM(bool *sync_rtx)
{
    ui_games_syncPersistence();

    // Check for events
    if(evQueue_wrPos == evQueue_rdPos)
        return;

    // Pop an event from the queue
    uint8_t newTail = (evQueue_rdPos + 1) % MAX_NUM_EVENTS;
    event_t event   = evQueue[evQueue_rdPos];
    evQueue_rdPos   = newTail;

    // There is some event to process, we need an UI redraw.
    // UI redraw request is cancelled if we're in standby mode.
    redraw_needed = true;
    if(standby) redraw_needed = false;

    // Check if battery has enough charge to operate.
    // Check is skipped if there is an ongoing transmission, since the voltage
    // drop caused by the RF PA power absorption causes spurious triggers of
    // the low battery alert.
    bool txOngoing = platform_getPttStatus();
#if !defined(PLATFORM_TTWRPLUS)
    if ((!state.emergency) && (!txOngoing) && (state.charge <= 0))
    {
        state.ui_screen = LOW_BAT;
        if(event.type == EVENT_KBD && event.payload)
        {
            state.ui_screen = MAIN_VFO;
            state.emergency = true;
        }
        return;
    }
#endif // PLATFORM_TTWRPLUS

    // Unlatch and exit from macro menu on PTT press
    if(macro_latched && txOngoing)
    {
        macro_latched = false;
        macro_menu = false;
    }

    long long now = getTick();
    // Process pressed keys
    if(event.type == EVENT_KBD)
    {
        kbd_msg_t msg;
        msg.value = event.payload;
        bool f1Handled = false;
        vpQueueFlags_t queueFlags = vp_getVoiceLevelQueueFlags();
        // If we get out of standby, we ignore the kdb event
        // unless is the MONI key for the MACRO functions
        if (_ui_exitStandby(now) && !(msg.keys & KEY_MONI))
            return;

        if(((state.tuner_mode == SCAN) || (state.tuner_mode == CHSCAN)) &&
           ((msg.keys & KEY_MONI) == 0))
        {
            if(state.tuner_mode == SCAN)
                _ui_stopVfoScan(sync_rtx, false);
            else
                _ui_stopChannelScan(sync_rtx, false);
        }

        // If MONI is pressed, activate MACRO functions
        bool moniPressed = msg.keys & KEY_MONI;
        if(moniPressed || macro_latched)
        {
            macro_menu = true;

            if(state.settings.macroMenuLatch == 1)
            {
                // long press moni on its own latches function.
                if (moniPressed && msg.long_press && !macro_latched)
                {
                    macro_latched = true;
                    vp_beep(BEEP_FUNCTION_LATCH_ON, LONG_BEEP);
                }
                else if (moniPressed && macro_latched)
                {
                    macro_latched = false;
                    vp_beep(BEEP_FUNCTION_LATCH_OFF, LONG_BEEP);
                }
            }

            _ui_fsm_menuMacro(msg, sync_rtx);
            return;
        }
        else
        {
            macro_menu = false;
        }
#if defined(PLATFORM_TTWRPLUS)
        // T-TWR Plus has no KEY_MONI, using KEY_VOLDOWN long press instead
        if ((msg.keys & KEY_VOLDOWN) && msg.long_press)
        {
            macro_menu = true;
            macro_latched = true;
        }
#endif // PLA%FORM_TTWRPLUS

        if(state.tone_enabled && !(msg.keys & KEY_HASH))
        {
            state.tone_enabled = false;
            *sync_rtx = true;
        }

        int priorUIScreen = state.ui_screen;
        switch(state.ui_screen)
        {
            // VFO screen
            case MAIN_VFO:
            {
                // Enable Tx in MAIN_VFO mode
                if (state.txDisable)
                {
                    state.txDisable = false;
                    *sync_rtx = true;
                }

                // Break out of the FSM if the keypad is locked but allow the
                // use of the hash key in FM mode for the 1750Hz tone.
                bool skipLock =  (state.channel.mode == OPMODE_FM)
                              && (msg.keys == KEY_HASH);

                if ((ui_state.input_locked == true) && (skipLock == false))
                    break;

                if(ui_state.edit_mode)
                {
                    #ifdef CONFIG_M17
                    if(state.channel.mode == OPMODE_M17)
                    {
                        if(msg.keys & KEY_ENTER)
                        {
                            _ui_textInputConfirm(ui_state.new_callsign);
                            // Save selected dst ID and disable input mode
                            strncpy(state.settings.m17_dest, ui_state.new_callsign, 10);
                            ui_state.edit_mode = false;
                            *sync_rtx = true;
                            vp_announceM17Info(NULL,  ui_state.edit_mode,
                                               queueFlags);
                        }
                        else if(msg.keys & KEY_HASH)
                        {
                            // Save selected dst ID and disable input mode
                            strncpy(state.settings.m17_dest, "", 1);
                            ui_state.edit_mode = false;
                            *sync_rtx = true;
                            vp_announceM17Info(NULL,  ui_state.edit_mode,
                                               queueFlags);
                        }
                        else if(msg.keys & KEY_ESC)
                            // Discard selected dst ID and disable input mode
                            ui_state.edit_mode = false;
                        else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            _ui_textInputDel(ui_state.new_callsign);
                        else if(input_isCharPressed(msg))
                            _ui_textInputKeypad(ui_state.new_callsign, 9, msg, true);
                        break;
                    }
                    #endif
                }
                else
                {
                    if(msg.keys & KEY_ENTER)
                    {
                        if(msg.long_press)
                        {
                            _ui_clearTemporaryFmActions(sync_rtx);
                            _ui_fsm_storeVfoToNewChannel(sync_rtx);
                        }
                        else
                        {
                            // Save current main state
                            ui_state.last_main_state = state.ui_screen;
                            // Open Menu
                            state.ui_screen = MENU_TOP;
                            // The selected item will be announced when the item is first selected.
                        }
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        _ui_clearTemporaryFmActions(sync_rtx);
                        // Save VFO channel
                        state.vfo_channel = state.channel;
                        int result = _ui_fsm_loadChannel(state.channel_index, sync_rtx);
                        // Read successful and channel is valid
                        if(result != -1)
                        {
                            // Switch to MEM screen
                            state.ui_screen = MAIN_MEM;
                            // anounce the active channel name.
                            vp_announceChannelName(&state.channel,
                                                   state.channel_index,
                                                   queueFlags);
                        }
                    }
                    else if(msg.keys & KEY_HASH)
                    {
                        #ifdef CONFIG_M17
                        // Only enter edit mode when using M17
                        if(state.channel.mode == OPMODE_M17)
                        {
                            // Enable dst ID input
                            ui_state.edit_mode = true;
                            // Reset text input variables
                            _ui_textInputReset(ui_state.new_callsign);
                            vp_announceM17Info(NULL,  ui_state.edit_mode,
                                               queueFlags);
                        }
                        else
                        #endif
                        {
                            if(!state.tone_enabled)
                            {
                                state.tone_enabled = true;
                                *sync_rtx = true;
                            }
                        }
                    }
                    else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                    {
                        // Increment TX and RX frequency of 12.5KHz
                        if(_ui_freq_check_limits(state.channel.rx_frequency + freq_steps[state.step_index]) &&
                           _ui_freq_check_limits(state.channel.tx_frequency + freq_steps[state.step_index]))
                        {
                            state.channel.rx_frequency += freq_steps[state.step_index];
                            state.channel.tx_frequency += freq_steps[state.step_index];
                            *sync_rtx = true;
                            vp_announceFrequencies(state.channel.rx_frequency,
                                                   state.channel.tx_frequency,
                                                   queueFlags);
                        }
                    }
                    else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                    {
                        // Decrement TX and RX frequency of 12.5KHz
                        if(_ui_freq_check_limits(state.channel.rx_frequency - freq_steps[state.step_index]) &&
                           _ui_freq_check_limits(state.channel.tx_frequency - freq_steps[state.step_index]))
                        {
                            state.channel.rx_frequency -= freq_steps[state.step_index];
                            state.channel.tx_frequency -= freq_steps[state.step_index];
                            *sync_rtx = true;
                            vp_announceFrequencies(state.channel.rx_frequency,
                                                   state.channel.tx_frequency,
                                                   queueFlags);
                        }
                    }
                    else if(msg.keys & KEY_F1)
                    {
                        if (state.settings.vpLevel > vpBeep)
                        {// quick press repeat vp, long press summary.
                            if (msg.long_press)
                                vp_announceChannelSummary(&state.channel, 0,
                                                          state.bank, vpAllInfo);
                            else
                                vp_replayLastPrompt();
                            f1Handled = true;
                        }
                    }
                    else if(input_isNumberPressed(msg))
                    {
                        // Open Frequency input screen
                        state.ui_screen = MAIN_VFO_INPUT;
                        // Reset input position and selection
                        ui_state.input_position = 1;
                        ui_state.input_set = SET_RX;
                        // do not play  because we will also announce the number just entered.
                        vp_announceInputReceiveOrTransmit(false, vpqInit);
                        vp_queueInteger(input_getPressedNumber(msg));
                        vp_play();

                        ui_state.new_rx_frequency = 0;
                        ui_state.new_tx_frequency = 0;
                        // Save pressed number to calculare frequency and show in GUI
                        ui_state.input_number = input_getPressedNumber(msg);
                        // Calculate portion of the new frequency
                        ui_state.new_rx_frequency = _ui_freq_add_digit(ui_state.new_rx_frequency,
                                                                       ui_state.input_position,
                                                                       ui_state.input_number);
                    }
                }
            }
                break;
            // VFO frequency input screen
            case MAIN_VFO_INPUT:
                if(msg.keys & KEY_ENTER)
                {
                    _ui_fsm_confirmVFOInput(sync_rtx);
                }
                else if(msg.keys & KEY_ESC)
                {
                    // Cancel frequency input, return to VFO mode
                    state.ui_screen = MAIN_VFO;
                }
                else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN)
                {
                    if(ui_state.input_set == SET_RX)
                    {
                        ui_state.input_set = SET_TX;
                        vp_announceInputReceiveOrTransmit(true, queueFlags);
                    }
                    else if(ui_state.input_set == SET_TX)
                    {
                        ui_state.input_set = SET_RX;
                        vp_announceInputReceiveOrTransmit(false, queueFlags);
                    }
                    // Reset input position
                    ui_state.input_position = 0;
                }
                else if(input_isNumberPressed(msg))
                {
                    _ui_fsm_insertVFONumber(msg, sync_rtx);
                }
                break;
            // MEM screen
            case MAIN_MEM:
                // Enable Tx in MAIN_MEM mode
                if (state.txDisable)
                {
                    state.txDisable = false;
                    *sync_rtx = true;
                }
                if (ui_state.input_locked)
                    break;
                // M17 Destination callsign input
                if(ui_state.edit_mode)
                {
                    {
                        if(msg.keys & KEY_ENTER)
                        {
                            _ui_textInputConfirm(ui_state.new_callsign);
                            // Save selected dst ID and disable input mode
                            strncpy(state.settings.m17_dest, ui_state.new_callsign, 10);
                            ui_state.edit_mode = false;
                            *sync_rtx = true;
                        }
                        else if(msg.keys & KEY_HASH)
                        {
                            // Save selected dst ID and disable input mode
                            strncpy(state.settings.m17_dest, "", 1);
                            ui_state.edit_mode = false;
                            *sync_rtx = true;
                        }
                        else if(msg.keys & KEY_ESC)
                            // Discard selected dst ID and disable input mode
                            ui_state.edit_mode = false;
                        else if(msg.keys & KEY_F1)
                        {
                            if (state.settings.vpLevel > vpBeep)
                            {
                                // Quick press repeat vp, long press summary.
                                if (msg.long_press)
                                {
                                    vp_announceChannelSummary(
                                            &state.channel,
                                            state.channel_index,
                                            state.bank,
                                            vpAllInfo);
                                }
                                else
                                {
                                    vp_replayLastPrompt();
                                }

                                f1Handled = true;
                            }
                        }
                        else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            _ui_textInputDel(ui_state.new_callsign);
                        else if(input_isCharPressed(msg))
                            _ui_textInputKeypad(ui_state.new_callsign, 9, msg, true);
                        break;
                    }
                }
                else
                {
                    if(msg.keys & KEY_ENTER)
                    {
                        if(msg.long_press)
                        {
                            _ui_beginMemoryEditSession(state.channel_index, &state.channel, false, false);
                            state.ui_screen = MENU_CHANNEL_EDIT;
                        }
                        else
                        {
                            // Save current main state
                            ui_state.last_main_state = state.ui_screen;
                            // Open Menu
                            state.ui_screen = MENU_TOP;
                        }
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        state.fm_monitor = false;
                        state.fm_reverse = false;
                        // Restore VFO channel
                        state.channel = state.vfo_channel;
                        // Update RTX configuration
                        *sync_rtx = true;
                        // Switch to VFO screen
                        state.ui_screen = MAIN_VFO;
                    }
                    else if(msg.keys & KEY_HASH)
                    {
                        // Only enter edit mode when using M17
                        if(state.channel.mode == OPMODE_M17)
                        {
                            // Enable dst ID input
                            ui_state.edit_mode = true;
                            // Reset text input variables
                            _ui_textInputReset(ui_state.new_callsign);
                        }
                        else
                        {
                            if(!state.tone_enabled)
                            {
                                state.tone_enabled = true;
                                *sync_rtx = true;
                            }
                        }
                    }
                    else if(msg.keys & KEY_F1)
                    {
                        if (state.settings.vpLevel > vpBeep)
                        {// quick press repeat vp, long press summary.
                            if (msg.long_press)
                            {
                                vp_announceChannelSummary(&state.channel,
                                                          state.channel_index+1,
                                                          state.bank, vpAllInfo);
                            }
                            else
                            {
                                vp_replayLastPrompt();
                            }

                            f1Handled = true;
                        }
                    }
                    else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                    {
                        _ui_fsm_loadChannel(state.channel_index + 1, sync_rtx);
                        vp_announceChannelName(&state.channel,
                                               state.channel_index+1,
                                               queueFlags);
                    }
                    else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                    {
                        _ui_fsm_loadChannel(state.channel_index - 1, sync_rtx);
                        vp_announceChannelName(&state.channel,
                                               state.channel_index+1,
                                               queueFlags);
                    }
                }
                break;
            case MENU_CHANNEL_EDIT:
                if(ui_state.edit_mode)
                {
                    switch(ui_state.menu_selected)
                    {
                        case CE_RX_FREQ:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                if(_ui_freq_check_limits(state.channel.rx_frequency + freq_steps[state.step_index]))
                                {
                                    state.channel.rx_frequency += freq_steps[state.step_index];
                                    *sync_rtx = true;
                                }
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                if(_ui_freq_check_limits(state.channel.rx_frequency - freq_steps[state.step_index]))
                                {
                                    state.channel.rx_frequency -= freq_steps[state.step_index];
                                    *sync_rtx = true;
                                }
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_TX_FREQ:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                if(_ui_freq_check_limits(state.channel.tx_frequency + freq_steps[state.step_index]))
                                {
                                    state.channel.tx_frequency += freq_steps[state.step_index];
                                    *sync_rtx = true;
                                }
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                if(_ui_freq_check_limits(state.channel.tx_frequency - freq_steps[state.step_index]))
                                {
                                    state.channel.tx_frequency -= freq_steps[state.step_index];
                                    *sync_rtx = true;
                                }
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_MODE:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_cycleChannelMode(+1);
                                vp_announceRadioMode(ui_state.memory_channel_draft.mode, queueFlags);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_cycleChannelMode(-1);
                                vp_announceRadioMode(ui_state.memory_channel_draft.mode, queueFlags);
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_BANDWIDTH:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_cycleChannelBandwidth(+1);
                                vp_announceBandwidth(ui_state.memory_channel_draft.bandwidth, queueFlags);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_cycleChannelBandwidth(-1);
                                vp_announceBandwidth(ui_state.memory_channel_draft.bandwidth, queueFlags);
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_POWER:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_cycleChannelPower(+1);
                                vp_announcePower(ui_state.memory_channel_draft.power, queueFlags);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_cycleChannelPower(-1);
                                vp_announcePower(ui_state.memory_channel_draft.power, queueFlags);
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_ZONE:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_cycleChannelZone(+1);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_cycleChannelZone(-1);
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        case CE_SCANLIST:
                            if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_cycleChannelScanList(+1);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_cycleChannelScanList(-1);
                            }
                            else if(msg.keys & KEY_ENTER || msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            break;
                        default:
                            ui_state.edit_mode = false;
                            break;
                    }
                }
                else if(state.ui_screen == MENU_CHANNEL_EDIT &&
                        (ui_state.menu_selected == CE_RX_FREQ || ui_state.menu_selected == CE_TX_FREQ) &&
                        input_isNumberPressed(msg))
                {
                    _ui_fsm_beginChannelFrequencyInput((ui_state.menu_selected == CE_RX_FREQ) ? SET_RX : SET_TX);
                    _ui_fsm_insertVFONumber(msg, sync_rtx);
                }
                else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(channel_edit_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(channel_edit_num);
                else if(msg.keys & KEY_ENTER)
                {
                    switch(ui_state.menu_selected)
                    {
                        case CE_RENAME:
                            _ui_textInputPreset(ui_state.new_channel_name, 15,
                                                ui_state.memory_channel_draft.name);
                            state.ui_screen = MENU_CHANNEL_RENAME;
                            break;
                        case CE_RX_FREQ:
                            _ui_fsm_beginChannelFrequencyInput(SET_RX);
                            break;
                        case CE_TX_FREQ:
                            _ui_fsm_beginChannelFrequencyInput(SET_TX);
                            break;
                        case CE_MODE:
                        case CE_BANDWIDTH:
                        case CE_POWER:
                        case CE_ZONE:
                        case CE_SCANLIST:
                            ui_state.edit_mode = true;
                            break;
                        case CE_LOCATION:
                            _ui_beginChannelLocationInput();
                            break;
                        case CE_SAVE:
                            _ui_fsm_storeCurrentMemoryChannel(sync_rtx);
                            state.ui_screen = MAIN_MEM;
                            break;
                        case CE_DELETE:
                            ui_state.edit_mode = false;
                            state.ui_screen = MENU_CHANNEL_DELETE;
                            break;
                        case CE_CANCEL:
                            _ui_cancelMemoryEditSession(sync_rtx);
                            break;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    _ui_cancelMemoryEditSession(sync_rtx);
                }
                else if((msg.keys & KEY_HASH) && (ui_state.menu_selected == CE_LOCATION))
                {
                    repeater_clearChannelLocation(&ui_state.memory_channel_draft);
                }
                break;
            case MENU_CHANNEL_LOCATION_INPUT:
                if(input_isNumberPressed(msg))
                {
                    _ui_locationInputAppendDigit(input_getPressedNumber(msg));
                }
                else if(msg.keys & KEY_LEFT || msg.keys & KNOB_LEFT)
                {
                    _ui_locationInputBackspace();
                }
                else if(msg.keys & KEY_RIGHT || msg.keys & KNOB_RIGHT)
                {
                    _ui_locationInputToggleSign();
                }
                else if(msg.keys & KEY_HASH)
                {
                    _ui_setLocationFieldValue(ui_state.channel_edit_location_field, 0);
                    ui_state.channel_edit_location_digits = 0U;
                }
                else if(msg.keys & KEY_ESC)
                {
                    state.ui_screen = MENU_CHANNEL_EDIT;
                }
                else if(msg.keys & KEY_ENTER)
                {
                    _ui_confirmChannelLocationInput();
                }
                break;
            case MENU_CHANNEL_FREQ_INPUT:
                if(msg.keys & KEY_ENTER)
                {
                    _ui_fsm_confirmChannelFrequencyInput(sync_rtx);
                }
                else if(msg.keys & KEY_ESC)
                {
                    state.ui_screen = MENU_CHANNEL_EDIT;
                }
                else if(input_isNumberPressed(msg))
                {
                    _ui_fsm_insertVFONumber(msg, sync_rtx);
                }
                break;
            case MENU_CHANNEL_RENAME:
                if(msg.keys & KEY_ENTER)
                {
                    if(ui_state.new_channel_name[ui_state.input_position] == '_')
                        ui_state.new_channel_name[ui_state.input_position] = '\0';
                    else
                        _ui_textInputConfirm(ui_state.new_channel_name);

                    if(ui_state.new_channel_name[0] != '\0')
                    {
                        strncpy(ui_state.memory_channel_draft.name,
                                ui_state.new_channel_name,
                                CPS_STR_SIZE);
                        ui_state.memory_channel_draft.name[CPS_STR_SIZE - 1] = '\0';
                    }

                    state.ui_screen = MENU_CHANNEL_EDIT;
                }
                else if(msg.keys & KEY_ESC)
                {
                    state.ui_screen = MENU_CHANNEL_EDIT;
                }
                else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                        msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                {
                    _ui_textInputDel(ui_state.new_channel_name);
                }
                else if(input_isCharPressed(msg))
                {
                    _ui_textInputKeypad(ui_state.new_channel_name, 15, msg, false);
                }
                break;
            case MENU_CHANNEL_DELETE:
                if(msg.keys & KEY_ENTER)
                {
                    if(ui_state.edit_mode)
                    {
                        if(ui_state.memory_edit_new)
                        {
                            ui_state.edit_mode = false;
                            _ui_cancelMemoryEditSession(sync_rtx);
                            break;
                        }

                        const int16_t storage_index = _ui_resolveChannelStorageIndex(ui_state.memory_edit_index);
                        if((storage_index < 0) ||
                           (cps_deleteChannel(ui_state.memory_channel_draft, storage_index) == -1))
                        {
                            _ui_announceStoreError();
                        }
                        else
                        {
                            repeater_invalidateNearestCache();
                            ui_state.edit_mode = false;
                            const int16_t display_index = ui_state.memory_edit_index;
                            _ui_resetMemoryEditSession();
                            if(_ui_fsm_loadChannel(display_index, sync_rtx) == -1)
                            {
                                if((display_index > 0) &&
                                   (_ui_fsm_loadChannel(display_index - 1, sync_rtx) != -1))
                                {
                                    state.ui_screen = MAIN_MEM;
                                }
                                else
                                {
                                    state.channel = state.vfo_channel;
                                    *sync_rtx = true;
                                    state.ui_screen = MAIN_VFO;
                                }
                            }
                            else
                            {
                                state.ui_screen = MAIN_MEM;
                            }
                        }
                    }
                    else
                    {
                        ui_state.edit_mode = true;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    ui_state.edit_mode = false;
                    state.ui_screen = MENU_CHANNEL_EDIT;
                }
                break;
            case MENU_CHANNEL_OVERWRITE:
                if(msg.keys & KEY_ENTER)
                {
                    if(ui_state.edit_mode)
                    {
                        if(_ui_overwriteMemoryWithVfo(ui_state.memory_edit_index) == -1)
                        {
                            _ui_announceStoreError();
                        }
                        else
                        {
                            _ui_announceChannelStored(ui_state.memory_edit_index);
                            ui_state.edit_mode = false;
                            state.ui_screen = MAIN_VFO;
                        }
                    }
                    else
                    {
                        ui_state.edit_mode = true;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    ui_state.edit_mode = false;
                    state.ui_screen = MENU_CHANNEL_ACTION;
                }
                break;
            // Top menu screen
            case MENU_TOP:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(menu_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(menu_num);
                else if(msg.keys & KEY_ENTER)
                {
                    switch(ui_state.menu_selected)
                    {
                        case M_BANK:
                            state.ui_screen = MENU_BANK;
                            break;
                        case M_CHANNEL:
                            state.ui_screen = MENU_CHANNEL;
                            break;
                        case M_CONTACTS:
                            state.ui_screen = MENU_CONTACTS;
                            break;
                        case M_GAMES:
                            state.ui_screen = MENU_GAMES;
                            ui_games_enterLibrary(&ui_state);
                            break;
#ifdef CONFIG_GPS
                        case M_GPS:
                            state.ui_screen = MENU_GPS;
                            break;
#endif
                        case M_SETTINGS:
                            state.ui_screen = MENU_SETTINGS;
                            break;
                        case M_INFO:
                            state.ui_screen = MENU_INFO;
                            break;
                        case M_ABOUT:
                            state.ui_screen = MENU_ABOUT;
                            break;
                    }
                    // Reset menu selection
                    ui_state.menu_selected = 0;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(ui_state.last_main_state);
                break;
            // Zone menu screen
            case MENU_BANK:
            case MENU_BANK_ACTION:
            // Channel menu screen
            case MENU_CHANNEL:
            case MENU_CHANNEL_ACTION:
            // Contacts menu screen
            case MENU_CONTACTS:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    // Using 1 as parameter disables menu wrap around
                {
                    if(state.ui_screen == MENU_CHANNEL_ACTION || state.ui_screen == MENU_BANK_ACTION)
                    {
                        if(state.ui_screen == MENU_CHANNEL_ACTION)
                        {
                            const uint8_t channel_actions = (ui_state.last_main_state == MAIN_VFO)
                                                          ? channel_action_num
                                                          : (channel_action_num - 1);
                            _ui_menuUp(channel_actions);
                        }
                        else
                        {
                            _ui_menuUp(bank_action_num);
                        }
                    }
                    else
                        _ui_menuUp(1);
                }
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                {
                    if(state.ui_screen == MENU_BANK)
                    {
                        bankHdr_t bank;
                        if(ui_state.menu_selected == 0)
                            ui_state.menu_selected += 1;
                        else if(ui_state.menu_selected == 1)
                        {
                            ui_state.menu_selected += 1;
                        }
                        else if(ui_state.menu_selected == 2)
                        {
                            if(cps_readBankHeader(&bank, 0) != -1)
                                ui_state.menu_selected += 1;
                        }
                        else if(cps_readBankHeader(&bank, ui_state.menu_selected - 2) != -1)
                            ui_state.menu_selected += 1;
                    }
                    else if(state.ui_screen == MENU_CHANNEL)
                    {
                        channel_t channel;
                        if((ui_state.menu_selected == 0) &&
                           (cps_readChannel(&channel, 1) != -1))
                            ui_state.menu_selected += 1;
                        else if(cps_readChannel(&channel, ui_state.menu_selected + 1) != -1)
                            ui_state.menu_selected += 1;
                    }
                    else if(state.ui_screen == MENU_CHANNEL_ACTION)
                    {
                        const uint8_t channel_actions = (ui_state.last_main_state == MAIN_VFO)
                                                      ? channel_action_num
                                                      : (channel_action_num - 1);
                        _ui_menuDown(channel_actions);
                    }
                    else if(state.ui_screen == MENU_BANK_ACTION)
                    {
                        _ui_menuDown(bank_action_num);
                    }
                    else if(state.ui_screen == MENU_CONTACTS)
                    {
                        contact_t contact;
                        if((ui_state.menu_selected == 0) &&
                           (cps_readContact(&contact, 1) != -1))
                            ui_state.menu_selected += 1;
                        else if(cps_readContact(&contact, ui_state.menu_selected + 1) != -1)
                            ui_state.menu_selected += 1;
                    }
                }
                else if(msg.keys & KEY_ENTER)
                {
                    if(state.ui_screen == MENU_BANK)
                    {
                        if(ui_state.menu_selected == 0)
                        {
                            state.bank_enabled = false;
                            state.bank_is_virtual = false;
                            if(ui_state.last_main_state == MAIN_VFO)
                                state.vfo_channel = state.channel;
                            _ui_fsm_loadChannel(0, sync_rtx);
                            state.ui_screen = MAIN_MEM;
                        }
                        else if(ui_state.menu_selected == 1)
                        {
                            state.bank_enabled = true;
                            state.bank_is_virtual = true;
                            state.bank = REPEATER_NEAREST_BANK;
                            if(_ui_fsm_loadChannel(0, sync_rtx) != -1)
                            {
                                if(ui_state.last_main_state == MAIN_VFO)
                                    state.vfo_channel = state.channel;
                                state.ui_screen = MAIN_MEM;
                            }
                            else
                            {
                                state.bank_enabled = false;
                                state.bank_is_virtual = false;
                                state.bank = 0;
                                vp_announceError(vpqInit);
                            }
                        }
                        else if(ui_state.menu_selected == 2)
                        {
                            _ui_prepareBankRename(-1);
                            state.ui_screen = MENU_BANK_RENAME;
                        }
                        else
                        {
                            ui_state.bank_edit_index = ui_state.menu_selected - 3;
                            ui_state.menu_selected = 0;
                            state.ui_screen = MENU_BANK_ACTION;
                        }
                    }
                    else if(state.ui_screen == MENU_BANK_ACTION)
                    {
                        bankHdr_t newbank;
                        int result = 0;

                        switch(ui_state.menu_selected)
                        {
                            case CA_OPEN:
                                state.bank_enabled = true;
                                state.bank_is_virtual = false;
                                result = cps_readBankHeader(&newbank, ui_state.bank_edit_index);
                                if(result != -1)
                                {
                                    state.bank = ui_state.bank_edit_index;
                                    if(ui_state.last_main_state == MAIN_VFO)
                                        state.vfo_channel = state.channel;
                                    _ui_fsm_loadChannel(0, sync_rtx);
                                    state.ui_screen = MAIN_MEM;
                                }
                                break;
                            case CA_EDIT:
                                _ui_prepareBankRename(ui_state.bank_edit_index);
                                state.ui_screen = MENU_BANK_RENAME;
                                break;
                            case CA_DELETE:
                                cps_deleteBankHeader(ui_state.bank_edit_index);
                                state.ui_screen = MENU_BANK;
                                ui_state.menu_selected = 0;
                                break;
                        }
                    }
                    if(state.ui_screen == MENU_CHANNEL)
                    {
                        if(ui_state.menu_selected == 0)
                        {
                            _ui_fsm_beginNewChannel(sync_rtx);
                        }
                        else
                        {
                            ui_state.memory_edit_index = ui_state.menu_selected - 1;
                            ui_state.menu_selected = 0;
                            state.ui_screen = MENU_CHANNEL_ACTION;
                        }
                    }
                    else if(state.ui_screen == MENU_CHANNEL_ACTION)
                    {
                        uint8_t channel_action = ui_state.menu_selected;
                        if((ui_state.last_main_state != MAIN_VFO) &&
                           (channel_action >= CA_SAVE_VFO_HERE))
                            channel_action++;

                        switch(channel_action)
                        {
                            case CA_OPEN:
                                if(ui_state.last_main_state == MAIN_VFO)
                                    state.vfo_channel = state.channel;
                                _ui_fsm_loadChannel(ui_state.memory_edit_index, sync_rtx);
                                state.ui_screen = MAIN_MEM;
                                break;
                            case CA_EDIT:
                                if(_ui_fsm_loadChannel(ui_state.memory_edit_index, sync_rtx) != -1)
                                {
                                    _ui_beginMemoryEditSession(ui_state.memory_edit_index,
                                                               &state.channel,
                                                               false,
                                                               false);
                                    state.ui_screen = MENU_CHANNEL_EDIT;
                                }
                                break;
                            case CA_COPY_TO_VFO:
                                _ui_copyMemoryToVfo(ui_state.memory_edit_index, sync_rtx);
                                break;
                            case CA_SCAN:
                                if((state.tuner_mode == CHSCAN) &&
                                   (state.channel_index == ui_state.memory_edit_index))
                                {
                                    _ui_stopChannelScan(sync_rtx, true);
                                    state.ui_screen = MAIN_MEM;
                                }
                                else if(_ui_fsm_loadChannel(ui_state.memory_edit_index, sync_rtx) != -1)
                                {
                                    state.ui_screen = MAIN_MEM;
                                    state.tuner_mode = CHSCAN;
                                    scan_notifyModeChange();
                                    _ui_announceScanState("Channel Scan", true);
                                }
                                break;
                            case CA_SAVE_VFO_HERE:
                                if(ui_state.last_main_state == MAIN_VFO)
                                {
                                    channel_t target_channel = {0};
                                    if(cps_readChannel(&target_channel, ui_state.memory_edit_index + 1) != -1)
                                        strncpy(ui_state.new_channel_name, target_channel.name, sizeof(ui_state.new_channel_name));
                                    else
                                        ui_state.new_channel_name[0] = '\0';

                                    ui_state.edit_mode = false;
                                    state.ui_screen = MENU_CHANNEL_OVERWRITE;
                                }
                                break;
                            case CA_DELETE:
                                if(_ui_fsm_loadChannel(ui_state.memory_edit_index, sync_rtx) != -1)
                                {
                                    ui_state.edit_mode = false;
                                    state.ui_screen = MENU_CHANNEL_DELETE;
                                }
                                break;
                        }
                    }
                    else if(state.ui_screen == MENU_CONTACTS)
                    {
                        if(ui_state.menu_selected == 0)
                            _ui_prepareContactEdit(-1);
                        else
                            _ui_prepareContactEdit(ui_state.menu_selected - 1);

                        state.ui_screen = MENU_CONTACT_EDIT;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    if(state.ui_screen == MENU_CHANNEL_ACTION)
                    {
                        ui_state.menu_selected = ui_state.memory_edit_index + 1;
                        state.ui_screen = MENU_CHANNEL;
                    }
                    else if(state.ui_screen == MENU_BANK_ACTION)
                    {
                        ui_state.menu_selected = ui_state.bank_edit_index + 2;
                        state.ui_screen = MENU_BANK;
                    }
                    else
                        _ui_menuBack(MENU_TOP);
                }
                break;
            case MENU_BANK_RENAME:
            case MENU_CONTACT_RENAME:
                if(msg.keys & KEY_ENTER)
                {
                    if(ui_state.new_channel_name[ui_state.input_position] == '_')
                        ui_state.new_channel_name[ui_state.input_position] = '\0';
                    else
                        _ui_textInputConfirm(ui_state.new_channel_name);

                    if(state.ui_screen == MENU_BANK_RENAME)
                    {
                        bankHdr_t bank = {0};
                        strncpy(bank.name, ui_state.new_channel_name, sizeof(bank.name));
                        bank.ch_count = 0;

                        if(ui_state.bank_edit_index >= 0)
                            cps_writeBankHeader(bank, ui_state.bank_edit_index);
                        else
                            cps_insertBankHeader(bank, _ui_getBankCount());

                        state.ui_screen = MENU_BANK;
                    }
                    else
                    {
                        state.ui_screen = MENU_CONTACT_EDIT;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    state.ui_screen = (state.ui_screen == MENU_BANK_RENAME) ? MENU_BANK : MENU_CONTACT_EDIT;
                }
                else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                        msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                {
                    _ui_textInputDel(ui_state.new_channel_name);
                }
                else if(input_isCharPressed(msg))
                {
                    _ui_textInputKeypad(ui_state.new_channel_name, 15, msg, false);
                }
                break;
            case MENU_CONTACT_EDIT:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(contact_edit_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(contact_edit_num);
                else if(msg.keys & KEY_ENTER)
                {
                    switch(ui_state.menu_selected)
                    {
                        case CT_RENAME:
                            _ui_textInputPreset(ui_state.new_channel_name, 15, ui_state.new_channel_name);
                            state.ui_screen = MENU_CONTACT_RENAME;
                            break;
                        case CT_SAVE:
                        {
                            contact_t contact = {0};

                            if(ui_state.contact_edit_index >= 0)
                                cps_readContact(&contact, ui_state.contact_edit_index + 1);

                            strncpy(contact.name, ui_state.new_channel_name, sizeof(contact.name));
                            contact.mode = OPMODE_M17;

                            if(ui_state.contact_edit_index >= 0)
                                cps_writeContact(contact, ui_state.contact_edit_index + 1);
                            else
                                cps_insertContact(contact, _ui_getContactCount() + 1);

                            state.ui_screen = MENU_CONTACTS;
                            break;
                        }
                        case CT_DELETE:
                            if(ui_state.contact_edit_index >= 0)
                                cps_deleteContact(ui_state.contact_edit_index + 1);
                            state.ui_screen = MENU_CONTACTS;
                            ui_state.menu_selected = 0;
                            break;
                        case CT_CANCEL:
                            state.ui_screen = MENU_CONTACTS;
                            break;
                    }
                }
                else if(msg.keys & KEY_ESC)
                {
                    state.ui_screen = MENU_CONTACTS;
                }
                break;
            case MENU_GAMES:
                ui_games_handleLibraryEvent(&ui_state, msg);
                break;
#ifdef CONFIG_GPS
            // GPS menu screen
            case MENU_GPS:
                if ((msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
                {// quick press repeat vp, long press summary.
                    if (msg.long_press)
                        vp_announceGPSInfo(vpGPSAll);
                    else
                        vp_replayLastPrompt();
                    f1Handled = true;
                }
                else if((msg.keys & KEY_ENTER) && !msg.long_press)
                {
                    ui_state.gps_map_enabled = !ui_state.gps_map_enabled;
                    if(ui_state.gps_map_enabled)
                        _ui_resetGPSMapCenter();
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled && (msg.keys & KEY_5))
                {
                    _ui_resetGPSMapCenter();
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled && (msg.keys & KEY_2))
                {
                    ui_state.gps_map_manual_pan = true;
                    ui_state.gps_map_center_lat -= _ui_gpsMapPanLatStep(ui_state.gps_map_zoom);
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled && (msg.keys & KEY_8))
                {
                    ui_state.gps_map_manual_pan = true;
                    ui_state.gps_map_center_lat += _ui_gpsMapPanLatStep(ui_state.gps_map_zoom);
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled && (msg.keys & KEY_4))
                {
                    ui_state.gps_map_manual_pan = true;
                    ui_state.gps_map_center_lon -= _ui_gpsMapPanLonStep(ui_state.gps_map_zoom);
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled && (msg.keys & KEY_6))
                {
                    ui_state.gps_map_manual_pan = true;
                    ui_state.gps_map_center_lon += _ui_gpsMapPanLonStep(ui_state.gps_map_zoom);
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled &&
                        (msg.keys & KEY_LEFT || msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT))
                {
                    ui_state.gps_map_zoom = gps_map_clampZoom((int16_t)ui_state.gps_map_zoom - 1);
                    redraw_needed = true;
                }
                else if(ui_state.gps_map_enabled &&
                        (msg.keys & KEY_RIGHT || msg.keys & KEY_UP || msg.keys & KNOB_RIGHT))
                {
                    ui_state.gps_map_zoom = gps_map_clampZoom((int16_t)ui_state.gps_map_zoom + 1);
                    redraw_needed = true;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
#endif
            // Settings menu screen
            case MENU_SETTINGS:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_num);
                else if(msg.keys & KEY_ENTER)
                {

                    switch(ui_state.menu_selected)
                    {
                        case S_DISPLAY:
                            state.ui_screen = SETTINGS_DISPLAY;
                            break;
#ifdef CONFIG_RTC
                        case S_TIMEDATE:
                            state.ui_screen = SETTINGS_TIMEDATE;
                            break;
#endif
#ifdef CONFIG_GPS
                        case S_GPS:
                            state.ui_screen = SETTINGS_GPS;
                            break;
#endif
                        case S_RADIO:
                            state.ui_screen = SETTINGS_RADIO;
                            break;
#ifdef CONFIG_M17
                        case S_M17:
                            state.ui_screen = SETTINGS_M17;
                            break;
#endif
                        case S_FM:
                            state.ui_screen = SETTINGS_FM;
                            break;
                        case S_ACCESSIBILITY:
                            state.ui_screen = SETTINGS_ACCESSIBILITY;
                            break;
                        case S_RESET2DEFAULTS:
                            state.ui_screen = SETTINGS_RESET2DEFAULTS;
                            break;
                        case S_FACTORY_RESET:
                            state.ui_screen = SETTINGS_FACTORY_RESET;
                            break;
                        default:
                            state.ui_screen = MENU_SETTINGS;
                    }
                    // Reset menu selection
                    ui_state.menu_selected = 0;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
            // Flash backup and restore menu screen
            case MENU_BACKUP_RESTORE:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_num);
                else if(msg.keys & KEY_ENTER)
                {

                    switch(ui_state.menu_selected)
                    {
                        case BR_BACKUP:
                            state.ui_screen = MENU_BACKUP;
                            break;
                        case BR_RESTORE:
                            state.ui_screen = MENU_RESTORE;
                            break;
                        default:
                            state.ui_screen = MENU_BACKUP_RESTORE;
                    }
                    // Reset menu selection
                    ui_state.menu_selected = 0;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
            case MENU_BACKUP:
            case MENU_RESTORE:
                if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
            // Info menu screen
            case MENU_INFO:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(info_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(info_num);
                else if((msg.keys & KEY_ENTER) && (ui_state.menu_selected == (info_num - 1)))
                {
                    size_t lineCount = devConsole_getDisplayRowCount(MAX_ENTRY_LEN - 1);
                    uint8_t visibleLines = (CONFIG_SCREEN_HEIGHT - layout.top_h - 1) / layout.menu_h;

                    state.ui_screen = MENU_DEV_CONSOLE;
                    if(lineCount > visibleLines)
                        ui_state.menu_selected = lineCount - visibleLines;
                    else
                        ui_state.menu_selected = 0;
                    devConsole_log(DEVLOG_INFO, "UI", "Developer console opened");
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
            case MENU_DEV_CONSOLE:
            {
                size_t lineCount = devConsole_getDisplayRowCount(MAX_ENTRY_LEN - 1);
                uint8_t visibleLines = (CONFIG_SCREEN_HEIGHT - layout.top_h - 1) / layout.menu_h;
                size_t maxScroll = (lineCount > visibleLines) ? (lineCount - visibleLines) : 0;

                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                {
                    if(ui_state.menu_selected > 0)
                        ui_state.menu_selected -= 1;
                }
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                {
                    if(ui_state.menu_selected < maxScroll)
                        ui_state.menu_selected += 1;
                }
                else if(msg.keys & KEY_ENTER)
                {
                    devConsole_clear();
                    devConsole_log(DEVLOG_INFO, "UI", "Console cleared");
                    ui_state.menu_selected = 0;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_INFO);
                break;
            }
            // About screen, scroll without rollover
            case MENU_ABOUT:
                if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                {
                    if(ui_state.menu_selected > 0)
                        ui_state.menu_selected -= 1;
                }
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    ui_state.menu_selected += 1;
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_TOP);
                break;
#ifdef CONFIG_RTC
            // Time&Date settings screen
            case SETTINGS_TIMEDATE:
                if(msg.keys & KEY_ENTER)
                {
                    // Switch to set Time&Date mode
                    state.ui_screen = SETTINGS_TIMEDATE_SET;
                    // Reset input position and selection
                    ui_state.input_position = 0;
                    memset(&ui_state.new_timedate, 0, sizeof(datetime_t));
                    vp_announceBuffer(&currentLanguage->timeAndDate,
                                      true, false, "dd/mm/yy");
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;
            // Time&Date settings screen, edit mode
            case SETTINGS_TIMEDATE_SET:
                if(msg.keys & KEY_ENTER)
                {
                    // Save time only if all digits have been inserted
                    if(ui_state.input_position < TIMEDATE_DIGITS)
                        break;
                    // Return to Time&Date menu, saving values
                    // NOTE: The user inserted a local time, we must save an UTC time
                    datetime_t utc_time = localTimeToUtc(ui_state.new_timedate,
                                                         state.settings.utc_timezone);
                    platform_setTime(utc_time);
                    state.time = utc_time;
                    vp_announceSettingsTimeDate();
                    state.ui_screen = SETTINGS_TIMEDATE;
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(SETTINGS_TIMEDATE);
                else if(input_isNumberPressed(msg))
                {
                    // Discard excess digits
                    if(ui_state.input_position > TIMEDATE_DIGITS)
                        break;
                    ui_state.input_position += 1;
                    ui_state.input_number = input_getPressedNumber(msg);
                    _ui_timedate_add_digit(&ui_state.new_timedate, ui_state.input_position,
                                            ui_state.input_number);
                }
                break;
#endif
            case SETTINGS_DISPLAY:
                if(msg.keys & KEY_LEFT || (ui_state.edit_mode &&
                   (msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)))
                {
                    switch(ui_state.menu_selected)
                    {
#ifdef CONFIG_SCREEN_BRIGHTNESS
                        case D_BRIGHTNESS:
                            _ui_changeBrightness(-5);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                                       state.settings.brightness);
                            break;
#endif
#ifdef CONFIG_SCREEN_CONTRAST
                        case D_CONTRAST:
                            _ui_changeContrast(-4);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                                       state.settings.contrast);
                            break;
#endif
                        case D_TIMER:
                            _ui_changeTimer(-1);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceDisplayTimer();
                            break;
                        case D_BATTERY:
                            state.settings.showBatteryIcon = !state.settings.showBatteryIcon;
                            break;
                        case D_THEME:
                            _ui_changeTheme(-1);
                            break;
                        default:
                            state.ui_screen = SETTINGS_DISPLAY;
                    }
                }
                else if(msg.keys & KEY_RIGHT || (ui_state.edit_mode &&
                        (msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)))
                {
                    switch(ui_state.menu_selected)
                    {
#ifdef CONFIG_SCREEN_BRIGHTNESS
                        case D_BRIGHTNESS:
                            _ui_changeBrightness(+5);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                                       state.settings.brightness);
                            break;
#endif
#ifdef CONFIG_SCREEN_CONTRAST
                        case D_CONTRAST:
                            _ui_changeContrast(+4);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsInt(&currentLanguage->brightness, queueFlags,
                                                       state.settings.contrast);
                            break;
#endif
                        case D_TIMER:
                            _ui_changeTimer(+1);
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceDisplayTimer();
                            break;
                        case D_BATTERY:
                            state.settings.showBatteryIcon = !state.settings.showBatteryIcon;
                            break;
                        case D_THEME:
                            _ui_changeTheme(+1);
                            break;
                        default:
                            state.ui_screen = SETTINGS_DISPLAY;
                    }
                }
                else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(display_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(display_num);
                else if(msg.keys & KEY_ENTER)
                    ui_state.edit_mode = !ui_state.edit_mode;
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;
#ifdef CONFIG_GPS
            case SETTINGS_GPS:
                if(msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT ||
                   (ui_state.edit_mode &&
                   (msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT ||
                    msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)))
                {
                    switch(ui_state.menu_selected)
                    {
                        case G_ENABLED:
                            if(state.settings.gps_enabled)
                                state.settings.gps_enabled = 0;
                            else
                                state.settings.gps_enabled = 1;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsOnOffToggle(&currentLanguage->gpsEnabled,
                                                               queueFlags,
                                                               state.settings.gps_enabled);
                            break;
#ifdef CONFIG_RTC
                        case G_SET_TIME:
                            state.settings.gpsSetTime = !state.settings.gpsSetTime;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceSettingsOnOffToggle(&currentLanguage->gpsSetTime,
                                                               queueFlags,
                                                               state.settings.gpsSetTime);
                            break;
                        case G_TIMEZONE:
                            if(msg.keys & KEY_LEFT || msg.keys & KEY_DOWN ||
                               msg.keys & KNOB_LEFT)
                                state.settings.utc_timezone -= 1;
                            else if(msg.keys & KEY_RIGHT || msg.keys & KEY_UP ||
                                    msg.keys & KNOB_RIGHT)
                                state.settings.utc_timezone += 1;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceTimeZone(state.settings.utc_timezone, queueFlags);
                            break;
#endif
                        default:
                            state.ui_screen = SETTINGS_GPS;
                    }
                }
                else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_gps_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_gps_num);
                else if(msg.keys & KEY_ENTER)
                    ui_state.edit_mode = !ui_state.edit_mode;
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;
#endif
            // Radio Settings
            case SETTINGS_RADIO:
                // If the entry is selected with enter we are in edit_mode
                if (ui_state.edit_mode)
                {
                    switch(ui_state.menu_selected)
                    {
                        case R_SHIFT:
                            // Handle offset frequency input
#if defined(CONFIG_UI_NO_KEYBOARD)
                            if(msg.long_press && msg.keys & KEY_ENTER)
                            {
                                // Long press on CONFIG_UI_NO_KEYBOARD causes digits to advance by one
                                ui_state.new_shift /= 10;
#else
                            if(msg.keys & KEY_ENTER)
                            {
#endif
                                // Apply new offset
                                state.channel.tx_frequency = state.channel.rx_frequency + ui_state.new_shift;
                                vp_queueStringTableEntry(&currentLanguage->repeaterShift);
                                vp_queueFrequency(ui_state.new_shift);
                                ui_state.edit_mode = false;
                            }
                            else
                            if(msg.keys & KEY_ESC)
                            {
                                // Announce old frequency offset
                                vp_queueStringTableEntry(&currentLanguage->repeaterShift);
                                vp_queueFrequency((int32_t)state.channel.tx_frequency - (int32_t)state.channel.rx_frequency);
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                    msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            {
                                _ui_numberInputDel(&ui_state.new_shift);
                            }
#if defined(CONFIG_UI_NO_KEYBOARD)
                            else if(msg.keys & KNOB_LEFT || msg.keys & KNOB_RIGHT || msg.keys & KEY_ENTER)
#else
                            else if(input_isNumberPressed(msg))
#endif
                            {
                                _ui_numberInputKeypad(&ui_state.new_shift, msg);
                                ui_state.input_position += 1;
                            }
                            else if (msg.long_press && (msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
                            {
                                vp_queueFrequency(ui_state.new_shift);
                                f1Handled=true;
                            }
                            break;
                        case R_DIRECTION:
                            if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                               msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT ||
                               msg.keys & KNOB_LEFT || msg.keys & KNOB_RIGHT)
                            {
                                // Invert frequency offset direction
                                if (state.channel.tx_frequency >= state.channel.rx_frequency)
                                    state.channel.tx_frequency -= 2 * ((int32_t)state.channel.tx_frequency - (int32_t)state.channel.rx_frequency);
                                else // Switch to positive offset
                                    state.channel.tx_frequency -= 2 * ((int32_t)state.channel.tx_frequency - (int32_t)state.channel.rx_frequency);
                            }
                            break;
                        case R_STEP:
                            if (msg.keys & KEY_UP || msg.keys & KEY_RIGHT || msg.keys & KNOB_RIGHT)
                            {
                                // Cycle over the available frequency steps
                                state.step_index++;
                                state.step_index %= n_freq_steps;
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KEY_LEFT || msg.keys & KNOB_LEFT)
                            {
                                state.step_index += n_freq_steps;
                                state.step_index--;
                                state.step_index %= n_freq_steps;
                            }
                            break;
                        case R_PPM:
                            // Handle PPM offset input
#if defined(CONFIG_UI_NO_KEYBOARD)
                            if(msg.long_press && msg.keys & KEY_ENTER)
                            {
                                // Long press on CONFIG_UI_NO_KEYBOARD causes digits to advance by one
                                ui_state.new_ppm /= 10;
#else
                            if(msg.keys & KEY_ENTER)
                            {
#endif
                                // Apply new offset
                                state.settings.ppm_offset = ui_state.new_ppm*ui_state.new_ppm_sign;
                                vp_queueStringTableEntry(&currentLanguage->ppmFreqOffset);
                                vp_queuePPM(state.settings.ppm_offset);
                                ui_state.edit_mode = false;
                            }
                            else if(msg.keys & KEY_ESC)
                            {
                                // Announce old frequency offset
                                vp_queueStringTableEntry(&currentLanguage->ppmFreqOffset);
                                vp_queuePPM(state.settings.ppm_offset);
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                    msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            {
                                uint32_t tmp = (uint32_t)ui_state.new_ppm;
                                _ui_numberInputDel(&tmp);
                                ui_state.new_ppm = (uint16_t)tmp;
                            }
#if defined(CONFIG_UI_NO_KEYBOARD)
                            else if(msg.keys & KNOB_LEFT || msg.keys & KNOB_RIGHT || msg.keys & KEY_ENTER)
#else
                            else if(input_isNumberPressed(msg))
#endif
                            {
                                uint32_t tmp = (uint32_t)ui_state.new_ppm;
                                _ui_numberInputKeypad(&tmp, msg);
                                //The PPM correction holds in an INT16 even though here we use a UINT16.
                                if(tmp <= INT16_MAX) {
                                    ui_state.new_ppm = (uint16_t)tmp;
                                    ui_state.input_position += 1;
                                }
                            }
#if !defined(CONFIG_UI_NO_KEYBOARD)
                            else if(msg.keys & KEY_HASH)
                            {
                                ui_state.new_ppm_sign *= -1;
                                vp_flush();
                                vp_queuePPM(ui_state.new_ppm * ui_state.new_ppm_sign);
                                vp_play();
                            }
#endif
                            else if (msg.long_press && (msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
                            {
                                vp_queuePPM(ui_state.new_ppm * ui_state.new_ppm_sign);
                                f1Handled=true;
                            }
                            break;
                        case R_POWER_RANGE:
                            if(msg.keys & KEY_UP || msg.keys & KEY_RIGHT || msg.keys & KNOB_RIGHT)
                            {
                                state.settings.powerProfile = (state.settings.powerProfile + 1)
                                                            % POWER_PROFILE_MAX;
                                state.channel.power = powerNormalizeStoredValue(state.channel.power,
                                                                                state.settings.powerProfile);
                            }
                            else if(msg.keys & KEY_DOWN || msg.keys & KEY_LEFT || msg.keys & KNOB_LEFT)
                            {
                                state.settings.powerProfile += POWER_PROFILE_MAX;
                                state.settings.powerProfile -= 1;
                                state.settings.powerProfile %= POWER_PROFILE_MAX;
                                state.channel.power = powerNormalizeStoredValue(state.channel.power,
                                                                                state.settings.powerProfile);
                            }
                            break;
                        case R_USB_LOG_EXPORT:
                            if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                               msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT ||
                               msg.keys & KNOB_LEFT || msg.keys & KNOB_RIGHT ||
                               msg.keys & KEY_ENTER)
                            {
                                state.settings.usbLogExport = !state.settings.usbLogExport;
                                devConsole_setUsbExportEnabled(state.settings.usbLogExport);
                                devConsole_log(DEVLOG_INFO, "USB", "USB log export %s",
                                               state.settings.usbLogExport ? "enabled" : "disabled");
                            }
                            break;
                        default:
                            state.ui_screen = SETTINGS_RADIO;
                    }
                    // If ENTER or ESC are pressed, exit edit mode, R_SHIFT is managed separately
                    if((ui_state.menu_selected != R_SHIFT && msg.keys & KEY_ENTER) || msg.keys & KEY_ESC)
                        ui_state.edit_mode = false;
                }
                else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_radio_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_radio_num);
                else if(msg.keys & KEY_ENTER) {
                    if(ui_state.menu_selected == R_USB_LOG_EXPORT)
                    {
                        state.settings.usbLogExport = !state.settings.usbLogExport;
                        devConsole_setUsbExportEnabled(state.settings.usbLogExport);
                        devConsole_log(DEVLOG_INFO, "USB", "USB log export %s",
                                       state.settings.usbLogExport ? "enabled" : "disabled");
                    }
                    else
                    {
                        ui_state.edit_mode = true;
                        // If we are entering R_SHIFT clear temp offset
                        if (ui_state.menu_selected == R_SHIFT)
                        ui_state.new_shift = 0;
                        else if(ui_state.menu_selected == R_PPM) {
                            ui_state.new_ppm = abs(state.settings.ppm_offset);
                            ui_state.new_ppm_sign = (state.settings.ppm_offset < 0) ? -1 : 1;
                        }
                        // Reset input position
                        ui_state.input_position = 0;
                    }
                }
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;
#ifdef CONFIG_M17
            // M17 Settings
            case SETTINGS_M17:
                if(ui_state.edit_mode)
                {
                    switch (ui_state.menu_selected)
                    {
                        case M17_CALLSIGN:
                            // Handle text input for M17 callsign
                            if(msg.keys & KEY_ENTER)
                            {
                                _ui_textInputConfirm(ui_state.new_callsign);
                                // Save selected callsign and disable input mode
                                strncpy(state.settings.callsign, ui_state.new_callsign, 10);
                                ui_state.edit_mode = false;
                                vp_announceBuffer(&currentLanguage->callsign,
                                                  false, true, state.settings.callsign);
                            }
                            else if(msg.keys & KEY_ESC)
                            {
                                // Discard selected callsign and disable input mode
                                ui_state.edit_mode = false;
                                vp_announceBuffer(&currentLanguage->callsign,
                                                  false, true, state.settings.callsign);
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                     msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            {
                                _ui_textInputDel(ui_state.new_callsign);
                            }
                            else if(input_isCharPressed(msg))
                            {
                                _ui_textInputKeypad(ui_state.new_callsign, 9, msg, true);
                            }
                            else if (msg.long_press && (msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
                            {
                                vp_announceBuffer(&currentLanguage->callsign,
                                                  true, true, ui_state.new_callsign);
                                f1Handled=true;
                            }
                            break;
                        case M17_METATEXT:
                            // Handle text input for M17 message text
                            if(msg.keys & KEY_ENTER)
                            {
                                _ui_textInputConfirm(ui_state.new_message);
                                // Save selected message and disable input mode
                                strncpy(state.settings.M17_meta_text, ui_state.new_message, 52);
                                ui_state.edit_message = false;
                                ui_state.edit_mode = false;
                                vp_announceBuffer(&currentLanguage->metaText,
                                                  false, true, state.settings.M17_meta_text);
                            }
                            else if(msg.keys & KEY_ESC)
                            {
                                // Discard selected message and disable input mode
                                ui_state.edit_message = false;
                                ui_state.edit_mode = false;
                                vp_announceBuffer(&currentLanguage->metaText,
                                                  false, true, state.settings.M17_meta_text);
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                     msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            {
                                _ui_textInputDel(ui_state.new_message);
                            }
                            else if(input_isCharPressed(msg))
                            {
                                _ui_textInputKeypad(ui_state.new_message, 52, msg, false);
                            }
                            else if (msg.long_press && (msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
                            {
                                vp_announceBuffer(&currentLanguage->metaText,
                                                  true, true, ui_state.new_message);
                                f1Handled=true;
                            }
                            break;
                        case M17_CAN:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                                _ui_changeM17Can(-1);
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                                _ui_changeM17Can(+1);
                            else if(msg.keys & KEY_ENTER)
                                ui_state.edit_mode = !ui_state.edit_mode;
                            else if(msg.keys & KEY_ESC)
                                ui_state.edit_mode = false;
                            break;
                        case M17_CAN_RX:
                            if(msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT ||
                                (ui_state.edit_mode &&
                                 (msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT ||
                                  msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)))
                            {
                                state.settings.m17_can_rx =
                                    !state.settings.m17_can_rx;
                            }
                            else if(msg.keys & KEY_ENTER)
                                ui_state.edit_mode = !ui_state.edit_mode;
                            else if(msg.keys & KEY_ESC)
                                ui_state.edit_mode = false;
                            break;
                        case M17_ENCRYPTION:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                                _ui_changeM17Encryption(-1);
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                                _ui_changeM17Encryption(+1);
                            else if(msg.keys & KEY_ENTER)
                                ui_state.edit_mode = !ui_state.edit_mode;
                            else if(msg.keys & KEY_ESC)
                                ui_state.edit_mode = false;
                            break;
                        case M17_KEY_SLOT:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                                _ui_changeM17KeySlot(-1);
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                                _ui_changeM17KeySlot(+1);
                            else if(msg.keys & KEY_ENTER)
                                ui_state.edit_mode = !ui_state.edit_mode;
                            else if(msg.keys & KEY_ESC)
                                ui_state.edit_mode = false;
                            break;
                        case M17_KEY_1:
                        case M17_KEY_2:
                        case M17_KEY_3:
                        case M17_KEY_4:
                        {
                            uint8_t slot = ui_state.menu_selected - M17_KEY_1;
                            if(msg.keys & KEY_ENTER)
                            {
                                _ui_textInputConfirm(ui_state.new_m17_key);
                                strncpy(state.settings.m17_keys[slot], ui_state.new_m17_key,
                                        M17_KEY_HEX_LEN);
                                state.settings.m17_keys[slot][M17_KEY_HEX_LEN] = '\0';
                                ui_state.edit_mode = false;
                            }
                            else if(msg.keys & KEY_ESC)
                            {
                                ui_state.edit_mode = false;
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KEY_DOWN ||
                                    msg.keys & KEY_LEFT || msg.keys & KEY_RIGHT)
                            {
                                _ui_textInputDel(ui_state.new_m17_key);
                            }
                            else if(input_isCharPressed(msg))
                            {
                                _ui_textInputKeypad(ui_state.new_m17_key,
                                                    M17_KEY_HEX_LEN,
                                                    msg,
                                                    true);
                            }
                            break;
                        }
                    }
                }
                else
                {
                    if(msg.keys & KEY_ENTER)
                    {
                        // Enable edit mode
                        ui_state.edit_mode = true;

                        // If callsign input, reset text input variables
                        if(ui_state.menu_selected == M17_CALLSIGN)
                        {
                            _ui_textInputReset(ui_state.new_callsign);
                            vp_announceBuffer(&currentLanguage->callsign,
                                            true, true, ui_state.new_callsign);
                        }
                        // If message input, reset text input variables
                        if(ui_state.menu_selected == M17_METATEXT)
                        {
                            //   ui_state.edit_mode = false;
                            ui_state.edit_message = true;
                            _ui_textInputReset(ui_state.new_message);
                            vp_announceBuffer(&currentLanguage->metaText,
                                            true, true, ui_state.new_message);
                        }
                        if((ui_state.menu_selected >= M17_KEY_1)
                           && (ui_state.menu_selected <= M17_KEY_4))
                        {
                            uint8_t slot = ui_state.menu_selected - M17_KEY_1;
                            _ui_textInputPreset(ui_state.new_m17_key,
                                                M17_KEY_HEX_LEN,
                                                state.settings.m17_keys[slot]);
                        }
                    }
                    else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                        _ui_menuUp(settings_m17_num);
                    else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                        _ui_menuDown(settings_m17_num);
                    else if((msg.keys & KEY_RIGHT) && (ui_state.menu_selected == M17_CAN))
                            _ui_changeM17Can(+1);
                    else if((msg.keys & KEY_LEFT)  && (ui_state.menu_selected == M17_CAN))
                            _ui_changeM17Can(-1);
                    else if((msg.keys & KEY_RIGHT) && (ui_state.menu_selected == M17_ENCRYPTION))
                            _ui_changeM17Encryption(+1);
                    else if((msg.keys & KEY_LEFT)  && (ui_state.menu_selected == M17_ENCRYPTION))
                            _ui_changeM17Encryption(-1);
                    else if((msg.keys & KEY_RIGHT) && (ui_state.menu_selected == M17_KEY_SLOT))
                            _ui_changeM17KeySlot(+1);
                    else if((msg.keys & KEY_LEFT)  && (ui_state.menu_selected == M17_KEY_SLOT))
                            _ui_changeM17KeySlot(-1);
                    else if(msg.keys & KEY_ESC)
                    {
                        *sync_rtx = true;
                        _ui_menuBack(MENU_SETTINGS);
                    }
                }
                break;
#endif
            case SETTINGS_FM:
                if (ui_state.edit_mode)
                {
                    if (msg.keys & KEY_ESC)
                        ui_state.edit_mode = false;

                    switch(ui_state.menu_selected)
                    {
                        case FM_RX_TONE:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                if(state.channel.fm.rxTone == 0)
                                    state.channel.fm.rxTone = CTCSS_FREQ_NUM - 1;
                                else
                                    state.channel.fm.rxTone--;
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                state.channel.fm.rxTone++;
                            }
                            else if(msg.keys & KEY_ENTER)
                            {
                                ui_state.edit_mode = false;
                            }

                            state.channel.fm.rxTone %= CTCSS_FREQ_NUM;
                            *sync_rtx = true;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceCTCSS(state.channel.fm.rxToneEn,
                                                 state.channel.fm.rxTone,
                                                 state.channel.fm.txToneEn,
                                                 state.channel.fm.txTone,
                                                 queueFlags | vpqIncludeDescriptions);
                            break;

                        case FM_TX_TONE:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                if(state.channel.fm.txTone == 0)
                                    state.channel.fm.txTone = CTCSS_FREQ_NUM - 1;
                                else
                                    state.channel.fm.txTone--;
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                state.channel.fm.txTone++;
                            }
                            else if(msg.keys & KEY_ENTER)
                            {
                                ui_state.edit_mode = false;
                            }

                            state.channel.fm.txTone %= CTCSS_FREQ_NUM;
                            *sync_rtx = true;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceCTCSS(state.channel.fm.rxToneEn,
                                                 state.channel.fm.rxTone,
                                                 state.channel.fm.txToneEn,
                                                 state.channel.fm.txTone,
                                                 queueFlags | vpqIncludeDescriptions);
                            break;

                        case FM_TONE_MODE:
                            if(msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)
                            {
                                _ui_handleToneSelectScroll(true);
                            }
                            else if(msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)
                            {
                                _ui_handleToneSelectScroll(false);
                            }
                            else if(msg.keys & KEY_ENTER)
                            {
                                ui_state.edit_mode = false;
                            }

                            *sync_rtx = true;
                            if(_ui_shouldAnnounceSettingChange(now))
                                vp_announceCTCSS(state.channel.fm.rxToneEn,
                                                 state.channel.fm.rxTone,
                                                 state.channel.fm.txToneEn,
                                                 state.channel.fm.txTone,
                                                 queueFlags | vpqIncludeDescriptions);
                            break;
                    }
                }
                else if (msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_fm_num);
                else if (msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_fm_num);
                else if (msg.keys & KEY_ENTER)
                    ui_state.edit_mode = !ui_state.edit_mode;
                else if (msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;

            case SETTINGS_ACCESSIBILITY:
                if(msg.keys & KEY_LEFT || (ui_state.edit_mode &&
                   (msg.keys & KEY_DOWN || msg.keys & KNOB_LEFT)))
                {
                    switch(ui_state.menu_selected)
                    {
                        case A_MACRO_LATCH:
                            _ui_changeMacroLatch(false);
                            break;
                        case A_LEVEL:
                            if(_ui_shouldAnnounceSettingChange(now))
                                _ui_changeVoiceLevel(-1);
                            else
                                state.settings.vpLevel -= (state.settings.vpLevel > vpNone);
                            break;
                        case A_PHONETIC:
                            _ui_changePhoneticSpell(false);
                            break;
                        default:
                            state.ui_screen = SETTINGS_ACCESSIBILITY;
                    }
                }
                else if(msg.keys & KEY_RIGHT || (ui_state.edit_mode &&
                        (msg.keys & KEY_UP || msg.keys & KNOB_RIGHT)))
                {
                    switch(ui_state.menu_selected)
                    {
                        case A_MACRO_LATCH:
                            _ui_changeMacroLatch(true);
                            break;
                        case A_LEVEL:
                            if(_ui_shouldAnnounceSettingChange(now))
                                _ui_changeVoiceLevel(1);
                            else if(state.settings.vpLevel < vpHigh)
                                state.settings.vpLevel += 1;
                            break;
                        case A_PHONETIC:
                            _ui_changePhoneticSpell(true);
                            break;
                        default:
                            state.ui_screen = SETTINGS_ACCESSIBILITY;
                    }
                }
                else if(msg.keys & KEY_UP || msg.keys & KNOB_LEFT)
                    _ui_menuUp(settings_accessibility_num);
                else if(msg.keys & KEY_DOWN || msg.keys & KNOB_RIGHT)
                    _ui_menuDown(settings_accessibility_num);
                else if(msg.keys & KEY_ENTER)
                    ui_state.edit_mode = !ui_state.edit_mode;
                else if(msg.keys & KEY_ESC)
                    _ui_menuBack(MENU_SETTINGS);
                break;
            case SETTINGS_RESET2DEFAULTS:
                if(! ui_state.edit_mode){
                    //require a confirmation ENTER, then another
                    //edit_mode is slightly misused to allow for this
                    if(msg.keys & KEY_ENTER)
                    {
                        ui_state.edit_mode = true;
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        _ui_menuBack(MENU_SETTINGS);
                    }
                } else {
                    if(msg.keys & KEY_ENTER)
                    {
                        ui_state.edit_mode = false;
                        state_resetSettingsAndVfo();
                        devConsole_log(DEVLOG_WARN, "UI", "Defaults restored");
                        _ui_menuBack(MENU_SETTINGS);
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        ui_state.edit_mode = false;
                        _ui_menuBack(MENU_SETTINGS);
                    }
                }
                break;
            case SETTINGS_FACTORY_RESET:
                if(! ui_state.edit_mode)
                {
                    if(msg.keys & KEY_ENTER)
                    {
                        ui_state.edit_mode = true;
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        _ui_menuBack(MENU_SETTINGS);
                    }
                }
                else
                {
                    if(msg.keys & KEY_ENTER)
                    {
                        ui_state.edit_mode = false;
                        devConsole_log(DEVLOG_WARN, "UI", "Factory reset requested");
                        _ui_resetCodeplug(sync_rtx);
                        _ui_menuBack(MENU_SETTINGS);
                    }
                    else if(msg.keys & KEY_ESC)
                    {
                        ui_state.edit_mode = false;
                        _ui_menuBack(MENU_SETTINGS);
                    }
                }
                break;
            case GAME_RUN:
                ui_games_handleRunningKeyEvent(msg);
                break;
        }

        // Enable Tx only if in MAIN_VFO or MAIN_MEM states
        bool inMemOrVfo = (state.ui_screen == MAIN_VFO) || (state.ui_screen == MAIN_MEM);
        if(!inMemOrVfo)
            _ui_clearTemporaryFmActions(sync_rtx);

        if ((macro_menu == true) || ((inMemOrVfo == false) && (state.txDisable == false)))
        {
            state.txDisable = true;
            *sync_rtx = true;
        }
        if (!f1Handled && (msg.keys & KEY_F1) && (state.settings.vpLevel > vpBeep))
        {
            vp_replayLastPrompt();
        }
        else if ((priorUIScreen!=state.ui_screen) && state.settings.vpLevel > vpLow)
        {
            // When we switch to VFO or Channel screen, we need to announce it.
            // Likewise for information screens.
            // All other cases are handled as needed.
            vp_announceScreen(state.ui_screen);
        }
        // generic beep for any keydown if beep is enabled.
        // At vp levels higher than beep, keys will generate voice so no need
        // to beep or you'll get an unwanted click.
        if ((msg.keys &0xffff) && (state.settings.vpLevel == vpBeep))
            vp_beep(BEEP_KEY_GENERIC, SHORT_BEEP);

        _ui_saveSettingsOnExit(_ui_isSettingsScreen(priorUIScreen) &&
                               !_ui_isSettingsScreen(state.ui_screen));

        // If we exit and re-enter the same menu, we want to ensure it speaks.
        if (msg.keys & KEY_ESC)
            _ui_reset_menu_anouncement_tracking();
    }
    else if(event.type == EVENT_STATUS)
    {
        if((state.tuner_mode == SCAN) && ((state.ui_screen != MAIN_VFO) || txOngoing))
            _ui_stopVfoScan(sync_rtx, true);
        else if((state.tuner_mode == CHSCAN) && ((state.ui_screen != MAIN_MEM) || txOngoing))
            _ui_stopChannelScan(sync_rtx, true);

#ifdef CONFIG_GPS
        if ((state.ui_screen == MENU_GPS) &&
            (!vp_isPlaying()) &&
            (state.settings.vpLevel > vpLow) &&
            (!txOngoing && !rtx_rxSquelchOpen()))
        {// automatically read speed and direction changes only!
            vpGPSInfoFlags_t whatChanged = GetGPSDirectionOrSpeedChanged();
            if (whatChanged != vpGPSNone)
                vp_announceGPSInfo(whatChanged);

            redraw_needed = true;
        }
#endif //            CONFIG_GPS

        if (txOngoing || rtx_rxSquelchOpen() || (state.volume != last_state.volume))
        {
            _ui_exitStandby(now);
            return;
        }

        if (_ui_checkStandby(now - last_event_tick))
        {
            _ui_enterStandby();
        }
    }

}

bool ui_idleTick(void)
{
    if(state.ui_screen != GAME_RUN)
        return false;

    bool updated = ui_games_handleRunningStatusEvent();

    if(updated)
        redraw_needed = true;

    return updated;
}

bool ui_updateGUI()
{
    if(redraw_needed == false)
        return false;

    _ui_applyTheme(last_state.settings.theme);

    if(!layout_ready)
    {
        _ui_calculateLayout(&layout);
        layout_ready = true;
    }
    // Draw current GUI page
    switch(last_state.ui_screen)
    {
        // VFO main screen
        case MAIN_VFO:
            _ui_drawMainVFO(&ui_state);
            break;
        // VFO frequency input screen
        case MAIN_VFO_INPUT:
            _ui_drawMainVFOInput(&ui_state);
            break;
        // MEM main screen
        case MAIN_MEM:
            _ui_drawMainMEM(&ui_state);
            break;
        // Top menu screen
        case MENU_TOP:
            _ui_drawMenuTop(&ui_state);
            break;
        // Zone menu screen
        case MENU_BANK:
            _ui_drawMenuBank(&ui_state);
            break;
        case MENU_BANK_ACTION:
            _ui_drawMenuBankAction(&ui_state);
            break;
        case MENU_BANK_RENAME:
            _ui_drawMenuBankRename(&ui_state);
            break;
        // Channel menu screen
        case MENU_CHANNEL:
            _ui_drawMenuChannel(&ui_state);
            break;
        case MENU_CHANNEL_ACTION:
            _ui_drawMenuChannelAction(&ui_state);
            break;
        case MENU_CHANNEL_EDIT:
            _ui_drawMenuChannelEdit(&ui_state);
            break;
        case MENU_CHANNEL_LOCATION_INPUT:
            _ui_drawMenuChannelLocationInput(&ui_state);
            break;
        case MENU_CHANNEL_FREQ_INPUT:
            _ui_drawMenuChannelFreqInput(&ui_state);
            break;
        case MENU_CHANNEL_RENAME:
            _ui_drawMenuChannelRename(&ui_state);
            break;
        case MENU_CHANNEL_DELETE:
            _ui_drawMenuChannelDelete(&ui_state);
            break;
        case MENU_CHANNEL_OVERWRITE:
            _ui_drawMenuChannelOverwrite(&ui_state);
            break;
        // Contacts menu screen
        case MENU_CONTACTS:
            _ui_drawMenuContacts(&ui_state);
            break;
        case MENU_CONTACT_EDIT:
            _ui_drawMenuContactEdit(&ui_state);
            break;
        case MENU_CONTACT_RENAME:
            _ui_drawMenuContactRename(&ui_state);
            break;
        case MENU_GAMES:
            ui_games_drawLibrary(&ui_state);
            break;
#ifdef CONFIG_GPS
        // GPS menu screen
        case MENU_GPS:
            _ui_drawMenuGPS();
            break;
#endif
        // Settings menu screen
        case MENU_SETTINGS:
            _ui_drawMenuSettings(&ui_state);
            break;
        // Flash backup and restore screen
        case MENU_BACKUP_RESTORE:
            _ui_drawMenuBackupRestore(&ui_state);
            break;
        // Flash backup screen
        case MENU_BACKUP:
            _ui_drawMenuBackup(&ui_state);
            break;
        // Flash restore screen
        case MENU_RESTORE:
            _ui_drawMenuRestore(&ui_state);
            break;
        // Info menu screen
        case MENU_INFO:
            _ui_drawMenuInfo(&ui_state);
            break;
        case MENU_DEV_CONSOLE:
            _ui_drawMenuDevConsole(&ui_state);
            break;
        // About menu screen
        case MENU_ABOUT:
            _ui_drawMenuAbout(&ui_state);
            break;
        case GAME_RUN:
            ui_games_drawRunning();
            break;
#ifdef CONFIG_RTC
        // Time&Date settings screen
        case SETTINGS_TIMEDATE:
            _ui_drawSettingsTimeDate();
            break;
        // Time&Date settings screen, edit mode
        case SETTINGS_TIMEDATE_SET:
            _ui_drawSettingsTimeDateSet(&ui_state);
            break;
#endif
        // Display settings screen
        case SETTINGS_DISPLAY:
            _ui_drawSettingsDisplay(&ui_state);
            break;
#ifdef CONFIG_GPS
        // GPS settings screen
        case SETTINGS_GPS:
            _ui_drawSettingsGPS(&ui_state);
            break;
#endif
#ifdef CONFIG_M17
        // M17 settings screen
        case SETTINGS_M17:
            _ui_drawSettingsM17(&ui_state);
            break;
#endif
        // FM settings screen
        case SETTINGS_FM:
            _ui_drawSettingsFM(&ui_state);
            break;
        case SETTINGS_ACCESSIBILITY:
            _ui_drawSettingsAccessibility(&ui_state);
            break;
        // Screen to support resetting Settings and VFO to defaults
        case SETTINGS_RESET2DEFAULTS:
            _ui_drawSettingsReset2Defaults(&ui_state);
            break;
        case SETTINGS_FACTORY_RESET:
            _ui_drawSettingsFactoryReset(&ui_state);
            break;
        // Screen to set frequency offset and step
        case SETTINGS_RADIO:
            _ui_drawSettingsRadio(&ui_state);
            break;
        // Low battery screen
        case LOW_BAT:
            _ui_drawLowBatteryScreen();
            break;
    }

    // If MACRO menu is active draw it
    if(macro_menu)
    {
        _ui_drawDarkOverlay();
        _ui_drawMacroMenu(&ui_state);
    }

    redraw_needed = false;
    return true;
}

bool ui_pushEvent(const uint8_t type, const uint32_t data)
{
    if(type == EVENT_STATUS)
    {
        if(state.ui_screen == GAME_RUN)
            return true;

        if(evQueue_wrPos != evQueue_rdPos)
        {
            uint8_t prev = (evQueue_wrPos + MAX_NUM_EVENTS - 1) % MAX_NUM_EVENTS;
            if(evQueue[prev].type == EVENT_STATUS)
                return true;
        }
    }

    uint8_t newHead = (evQueue_wrPos + 1) % MAX_NUM_EVENTS;

    // Queue is full
    if(newHead == evQueue_rdPos) return false;

    // Preserve atomicity when writing the new element into the queue.
    event_t event;
    event.type    = type;
    event.payload = data;

    evQueue[evQueue_wrPos] = event;
    evQueue_wrPos = newHead;

    return true;
}

void ui_terminate()
{
}
