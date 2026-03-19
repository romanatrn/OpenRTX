/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "hwconfig.h"
#include <stdbool.h>
#include <stdint.h>

#define M17_KEY_SLOTS 4
#define M17_KEY_HEX_LEN 32

typedef enum
{
    TIMER_OFF =  0,
    TIMER_5S  =  1,
    TIMER_10S =  2,
    TIMER_15S =  3,
    TIMER_20S =  4,
    TIMER_25S =  5,
    TIMER_30S =  6,
    TIMER_1M  =  7,
    TIMER_2M  =  8,
    TIMER_3M  =  9,
    TIMER_4M  = 10,
    TIMER_5M  = 11,
    TIMER_15M = 12,
    TIMER_30M = 13,
    TIMER_45M = 14,
    TIMER_1H  = 15
}
display_timer_t;

typedef struct
{
    uint8_t brightness;           // Display brightness
    uint8_t contrast;             // Display contrast
    uint8_t sqlLevel;             // Squelch level
    uint8_t voxLevel;             // Vox level
    int8_t  utc_timezone;         // Timezone, in units of half hours
    bool    gps_enabled;          // GPS active
    char    callsign[10];         // Plaintext callsign
    uint8_t display_timer   : 4,  // Standby timer
            m17_can         : 4;  // M17 CAN
    uint8_t vpLevel         : 3,  // Voice prompt level
            vpPhoneticSpell : 1,  // Phonetic spell enabled
            macroMenuLatch  : 1;  // Automatic latch of macro menu
    uint8_t theme;                // UI color theme
    bool    m17_can_rx;           // Check M17 CAN on RX
    char    m17_dest[10];         // M17 destination
    bool    showBatteryIcon;      // Battery display true: icon, false: percentage
    bool    gpsSetTime;           // Use GPS to ajust RTC time
    char    M17_meta_text[53];    // M17 Meta Text to send
    uint8_t m17_default_encryption;          // M17 default encryption mode
    uint8_t m17_default_enc_subtype;         // M17 default encryption subtype
    uint8_t m17_default_key_index;           // M17 default key slot (1-based, 0 disabled)
    char    m17_keys[M17_KEY_SLOTS][M17_KEY_HEX_LEN + 1]; // M17 key slots as hex strings
    int16_t ppm_offset;           // Frequency offset for tuning (in tenth of ppm)
    uint16_t snake_high_score;    // Best Snake score
    uint16_t tetris_high_score;   // Best Tetris score
    uint16_t bomber_high_score;   // Best Bomberman Lite score
    uint16_t mines_high_score;    // Best Minesweeper clears
}
__attribute__((packed)) settings_t;


static const settings_t default_settings =
{
    100,
#ifdef CONFIG_SCREEN_CONTRAST
    CONFIG_DEFAULT_CONTRAST,
#else
    255,
#endif
    4,
    0,
    0,
    false,
    "",
    TIMER_30S,
    0,
    0,
    0,
    1,
    0,
    false,
    "",
    false,
    false,
    "OpenRTX",
    0,
    0,
    0,
    { "", "", "", "" },
    0,
    0,
    0,
    0,
    0,
};

#endif /* SETTINGS_H */
