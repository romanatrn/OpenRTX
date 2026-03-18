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

typedef struct game2048_state_t
{
    uint16_t board[4][4];
    uint16_t best_tile;
    uint16_t score;
    bool game_over;
    uint32_t rng_state;
} game2048_state_t;

static game2048_state_t game2048;

static uint32_t game2048_rand(void)
{
    game2048.rng_state ^= (uint32_t) getTick() + (game2048.rng_state << 6) + (game2048.rng_state >> 2);
    game2048.rng_state ^= game2048.rng_state << 13;
    game2048.rng_state ^= game2048.rng_state >> 17;
    game2048.rng_state ^= game2048.rng_state << 5;
    return game2048.rng_state;
}

static void game2048_spawn(void)
{
    uint8_t empties[16][2];
    uint8_t count = 0;

    for(uint8_t y = 0; y < 4; y++)
        for(uint8_t x = 0; x < 4; x++)
            if(game2048.board[y][x] == 0)
            {
                empties[count][0] = x;
                empties[count][1] = y;
                count += 1;
            }

    if(count == 0)
        return;

    uint8_t choice = game2048_rand() % count;
    game2048.board[empties[choice][1]][empties[choice][0]] = ((game2048_rand() & 3) == 0) ? 4 : 2;
}

static void game2048_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&game2048, 0, sizeof(game2048));
    game2048.best_tile = ui_games_readBestScore(UI_GAME_2048);
    game2048.rng_state = 0x7F4A7C15u ^ (uint32_t) getTick();
    game2048_spawn();
    game2048_spawn();
}

static bool game2048_moveLine(uint16_t line[4])
{
    uint16_t tmp[4] = {0};
    uint8_t pos = 0;
    bool moved = false;

    for(uint8_t i = 0; i < 4; i++)
    {
        if(line[i] == 0)
            continue;

        if((pos > 0) && (tmp[pos - 1] == line[i]))
        {
            tmp[pos - 1] *= 2;
            game2048.score += tmp[pos - 1];
            if(tmp[pos - 1] > game2048.best_tile)
            {
                game2048.best_tile = tmp[pos - 1];
                ui_games_writeBestScore(UI_GAME_2048, game2048.best_tile);
            }
            moved = true;
        }
        else
        {
            if(line[i] != tmp[pos])
                moved = true;
            tmp[pos++] = line[i];
        }
    }

    for(uint8_t i = 0; i < 4; i++)
    {
        if(line[i] != tmp[i])
            moved = true;
        line[i] = tmp[i];
    }

    return moved;
}

static bool game2048_applyMove(int8_t dx, int8_t dy)
{
    bool moved = false;
    uint16_t line[4];

    for(uint8_t outer = 0; outer < 4; outer++)
    {
        for(uint8_t inner = 0; inner < 4; inner++)
        {
            uint8_t x = (dx > 0) ? (3 - inner) : ((dx < 0) ? inner : outer);
            uint8_t y = (dy > 0) ? (3 - inner) : ((dy < 0) ? inner : outer);
            if(dx == 0)
                x = outer;
            if(dy == 0)
                y = outer;
            line[inner] = game2048.board[y][x];
        }

        if(game2048_moveLine(line))
            moved = true;

        for(uint8_t inner = 0; inner < 4; inner++)
        {
            uint8_t x = (dx > 0) ? (3 - inner) : ((dx < 0) ? inner : outer);
            uint8_t y = (dy > 0) ? (3 - inner) : ((dy < 0) ? inner : outer);
            if(dx == 0)
                x = outer;
            if(dy == 0)
                y = outer;
            game2048.board[y][x] = line[inner];
        }
    }

    return moved;
}

static bool game2048_hasMoves(void)
{
    for(uint8_t y = 0; y < 4; y++)
    {
        for(uint8_t x = 0; x < 4; x++)
        {
            if(game2048.board[y][x] == 0)
                return true;
            if((x < 3) && (game2048.board[y][x] == game2048.board[y][x + 1]))
                return true;
            if((y < 3) && (game2048.board[y][x] == game2048.board[y + 1][x]))
                return true;
        }
    }
    return false;
}

static void game2048_handleInput(kbd_msg_t msg)
{
    bool moved = false;

    if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
        moved = game2048_applyMove(-1, 0);
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
        moved = game2048_applyMove(1, 0);
    else if(msg.keys & (KEY_UP | KEY_2))
        moved = game2048_applyMove(0, -1);
    else if(msg.keys & (KEY_DOWN | KEY_8))
        moved = game2048_applyMove(0, 1);

    if(moved)
        game2048_spawn();

    if(!game2048_hasMoves())
        game2048.game_over = true;
}

static void game2048_tick(const ui_game_layout_t *layout)
{
    (void) layout;
}

static void game2048_draw(const ui_game_layout_t *layout)
{
    uint8_t tile = (CONFIG_SCREEN_HEIGHT > 64) ? 20 : 12;
    point_t origin = {(CONFIG_SCREEN_WIDTH - (tile * 4)) / 2,
                      layout->board_origin.y + ((layout->board_height - (tile * 4)) / 2)};
    char buf[6];

    for(uint8_t y = 0; y < 4; y++)
    {
        for(uint8_t x = 0; x < 4; x++)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};
            color_t cell = (game2048.board[y][x] == 0) ? color_grey : color_white;
            gfx_drawRect(pos, tile - 1, tile - 1, cell, game2048.board[y][x] != 0);
            gfx_drawRect(pos, tile - 1, tile - 1, color_grey, false);
            if(game2048.board[y][x] != 0)
            {
                snprintf(buf, sizeof(buf), "%u", game2048.board[y][x]);
                gfx_print((point_t){pos.x + ((tile - 1) / 2), pos.y + (tile / 2)},
                          FONT_SIZE_6PT,
                          TEXT_ALIGN_CENTER,
                          color_black,
                          buf);
            }
        }
    }
}

static uint16_t game2048_getScore(void)
{
    return game2048.score;
}

static uint16_t game2048_getBestScore(void)
{
    return game2048.best_tile;
}

static bool game2048_isGameOver(void)
{
    return game2048.game_over;
}

const ui_game_driver_t ui_game_2048 =
{
    .id = UI_GAME_2048,
    .title = "2048",
    .tick_period_ms = 120,
    .start = game2048_start,
    .handleInput = game2048_handleInput,
    .tick = game2048_tick,
    .draw = game2048_draw,
    .getScore = game2048_getScore,
    .getBestScore = game2048_getBestScore,
    .isGameOver = game2048_isGameOver,
};
