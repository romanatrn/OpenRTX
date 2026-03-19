/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "interfaces/delays.h"
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define MINES_W 8
#define MINES_H 6
#define MINES_COUNT 8

typedef struct mines_state_t
{
    uint8_t mines[MINES_H][MINES_W];
    uint8_t open[MINES_H][MINES_W];
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint16_t score;
    uint16_t best_score;
    uint32_t rng_state;
    bool game_over;
} mines_state_t;

static mines_state_t mines;

static uint32_t mines_rand(void)
{
    mines.rng_state ^= (uint32_t) getTick() + (mines.rng_state << 7) + (mines.rng_state >> 1);
    mines.rng_state ^= mines.rng_state << 13;
    mines.rng_state ^= mines.rng_state >> 17;
    mines.rng_state ^= mines.rng_state << 5;
    return mines.rng_state;
}

static uint8_t mines_countAround(uint8_t x, uint8_t y)
{
    uint8_t count = 0;
    for(int8_t dy = -1; dy <= 1; dy++)
        for(int8_t dx = -1; dx <= 1; dx++)
        {
            int8_t nx = x + dx;
            int8_t ny = y + dy;
            if((nx >= 0) && (ny >= 0) && (nx < MINES_W) && (ny < MINES_H) && mines.mines[ny][nx])
                count += 1;
        }
    return count;
}

static void mines_openCell(uint8_t x, uint8_t y)
{
    if((x >= MINES_W) || (y >= MINES_H) || mines.open[y][x])
        return;

    mines.open[y][x] = 1;
    if(mines.mines[y][x])
    {
        mines.game_over = true;
        return;
    }

    mines.score += 1;
    if(mines.score > mines.best_score)
    {
        mines.best_score = mines.score;
        ui_games_writeBestScore(UI_GAME_MINES, mines.best_score);
    }

    if(mines_countAround(x, y) == 0)
    {
        for(int8_t dy = -1; dy <= 1; dy++)
            for(int8_t dx = -1; dx <= 1; dx++)
            {
                int8_t nx = x + dx;
                int8_t ny = y + dy;
                if((nx >= 0) && (ny >= 0) && (nx < MINES_W) && (ny < MINES_H))
                    mines_openCell(nx, ny);
            }
    }
}

static void mines_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&mines, 0, sizeof(mines));
    mines.best_score = ui_games_readBestScore(UI_GAME_MINES);
    mines.rng_state = 0x51F2D3C4u ^ (uint32_t) getTick();

    uint8_t placed = 0;
    while(placed < MINES_COUNT)
    {
        uint8_t x = mines_rand() % MINES_W;
        uint8_t y = mines_rand() % MINES_H;
        if(!mines.mines[y][x])
        {
            mines.mines[y][x] = 1;
            placed += 1;
        }
    }
}

static void mines_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
    {
        if(mines.cursor_x > 0) mines.cursor_x -= 1;
    }
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
    {
        if(mines.cursor_x + 1 < MINES_W) mines.cursor_x += 1;
    }
    else if(msg.keys & (KEY_UP | KEY_2))
    {
        if(mines.cursor_y > 0) mines.cursor_y -= 1;
    }
    else if(msg.keys & (KEY_DOWN | KEY_8))
    {
        if(mines.cursor_y + 1 < MINES_H) mines.cursor_y += 1;
    }
    else if(msg.keys & (KEY_ENTER | KEY_5))
    {
        mines_openCell(mines.cursor_x, mines.cursor_y);
    }
}

static void mines_tick(const ui_game_layout_t *layout)
{
    (void) layout;
}

static void mines_draw(const ui_game_layout_t *layout)
{
    uint8_t tile = (CONFIG_SCREEN_HEIGHT > 64) ? 12 : 9;
    point_t origin = {(CONFIG_SCREEN_WIDTH - (tile * MINES_W)) / 2,
                      layout->board_origin.y + ((layout->board_height - (tile * MINES_H)) / 2)};
    char buf[4];

    for(uint8_t y = 0; y < MINES_H; y++)
    {
        for(uint8_t x = 0; x < MINES_W; x++)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};
            bool cursor = (x == mines.cursor_x) && (y == mines.cursor_y);

            gfx_drawRect(pos, tile - 1, tile - 1, cursor ? yellow_fab413 : color_grey, false);
            if(mines.open[y][x])
            {
                gfx_drawRect((point_t){pos.x + 1, pos.y + 1}, tile - 3, tile - 3, color_white, true);
                if(mines.mines[y][x])
                    gfx_drawCircle((point_t){pos.x + (tile / 2), pos.y + (tile / 2)}, 2, color_black);
                else
                {
                    uint8_t count = mines_countAround(x, y);
                    if(count > 0)
                    {
                        snprintf(buf, sizeof(buf), "%u", count);
                        gfx_print((point_t){pos.x + ((tile - 1) / 2), pos.y + (tile / 2)},
                                  FONT_SIZE_6PT,
                                  TEXT_ALIGN_CENTER,
                                  color_black,
                                  buf);
                    }
                }
            }
        }
    }
}

static uint16_t mines_getScore(void)
{
    return mines.score;
}

static uint16_t mines_getBestScore(void)
{
    return mines.best_score;
}

static bool mines_isGameOver(void)
{
    return mines.game_over;
}

const ui_game_driver_t ui_game_mines =
{
    .id = UI_GAME_MINES,
    .title = "Mines",
    .tick_period_ms = 120,
    .start = mines_start,
    .handleInput = mines_handleInput,
    .tick = mines_tick,
    .draw = mines_draw,
    .getScore = mines_getScore,
    .getBestScore = mines_getBestScore,
    .isGameOver = mines_isGameOver,
};
