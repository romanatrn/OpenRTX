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

typedef enum ui_game_id_t
{
    UI_GAME_SNAKE = 0,
    UI_GAME_TETRIS,
    UI_GAME_BOMBER,
    UI_GAME_MINES,
} ui_game_id_t;

typedef struct ui_game_driver_t
{
    ui_game_id_t id;
    const char *title;
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
extern const ui_game_driver_t ui_game_tetris;
extern const ui_game_driver_t ui_game_bomber;
extern const ui_game_driver_t ui_game_mines;

uint16_t ui_games_readBestScore(ui_game_id_t game_id);
void ui_games_writeBestScore(ui_game_id_t game_id, uint16_t value);
void ui_games_syncPersistence(void);

#endif /* UI_GAMES_INTERNAL_H */
