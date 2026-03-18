/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_GAMES_INTERNAL_H
#define UI_GAMES_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "core/graphics.h"
#include "core/input.h"

typedef struct ui_game_layout_t
{
    point_t  board_origin;
    uint16_t board_width;
    uint16_t board_height;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  tile_size;
    uint8_t  header_height;
} ui_game_layout_t;

typedef struct ui_game_driver_t
{
    const char *title;
    const char *subtitle;
    uint16_t    tick_period_ms;
    void      (*start)(const ui_game_layout_t *layout);
    void      (*handleInput)(kbd_msg_t msg);
    void      (*tick)(const ui_game_layout_t *layout);
    void      (*draw)(const ui_game_layout_t *layout);
    uint16_t  (*getScore)(void);
    uint16_t  (*getBestScore)(void);
    bool      (*isGameOver)(void);
} ui_game_driver_t;

extern const ui_game_driver_t ui_game_snake;

#endif /* UI_GAMES_INTERNAL_H */
