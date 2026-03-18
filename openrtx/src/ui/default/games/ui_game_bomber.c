/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "interfaces/delays.h"
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define BOMBER_W 9
#define BOMBER_H 7
#define BOMBER_ENEMIES 2

typedef struct bomber_state_t
{
    uint8_t blocks[BOMBER_H][BOMBER_W];
    uint8_t player_x;
    uint8_t player_y;
    uint8_t enemy_x[BOMBER_ENEMIES];
    uint8_t enemy_y[BOMBER_ENEMIES];
    uint16_t score;
    uint16_t best_score;
    uint32_t rng_state;
    int8_t bomb_x;
    int8_t bomb_y;
    uint8_t bomb_timer;
    uint8_t blast_timer;
    bool game_over;
} bomber_state_t;

static const uint8_t bomber_map[BOMBER_H][BOMBER_W] =
{
    {0,1,0,1,0,1,0,1,0},
    {0,0,1,0,0,0,1,0,0},
    {1,0,1,0,1,0,1,0,1},
    {0,0,0,0,0,0,0,0,0},
    {1,0,1,0,1,0,1,0,1},
    {0,0,1,0,0,0,1,0,0},
    {0,1,0,1,0,1,0,1,0},
};

static bomber_state_t bomber;

static uint32_t bomber_rand(void)
{
    bomber.rng_state ^= (uint32_t) getTick() + (bomber.rng_state << 7) + (bomber.rng_state >> 5);
    bomber.rng_state ^= bomber.rng_state << 13;
    bomber.rng_state ^= bomber.rng_state >> 17;
    bomber.rng_state ^= bomber.rng_state << 5;
    return bomber.rng_state;
}

static bool bomber_isBlocked(uint8_t x, uint8_t y)
{
    return bomber.blocks[y][x] != 0;
}

static bool bomber_isBlastCell(uint8_t x, uint8_t y)
{
    if(bomber.blast_timer == 0)
        return false;

    if((x == bomber.bomb_x) && (y == bomber.bomb_y))
        return true;
    if((x + 1 == bomber.bomb_x) && (y == bomber.bomb_y) && !bomber_isBlocked(x, y))
        return true;
    if((x == bomber.bomb_x + 1) && (y == bomber.bomb_y) && !bomber_isBlocked(x, y))
        return true;
    if((x == bomber.bomb_x) && (y + 1 == bomber.bomb_y) && !bomber_isBlocked(x, y))
        return true;
    if((x == bomber.bomb_x) && (y == bomber.bomb_y + 1) && !bomber_isBlocked(x, y))
        return true;

    return false;
}

static void bomber_spawn(void)
{
    memcpy(bomber.blocks, bomber_map, sizeof(bomber.blocks));
    bomber.player_x = 0;
    bomber.player_y = 0;
    bomber.enemy_x[0] = BOMBER_W - 1;
    bomber.enemy_y[0] = BOMBER_H - 1;
    bomber.enemy_x[1] = BOMBER_W - 1;
    bomber.enemy_y[1] = 0;
    bomber.bomb_x = -1;
    bomber.bomb_y = -1;
    bomber.bomb_timer = 0;
    bomber.blast_timer = 0;
}

static void bomber_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&bomber, 0, sizeof(bomber));
    bomber.best_score = ui_games_readBestScore(UI_GAME_BOMBER);
    bomber.rng_state = 0x6D2B79F5u ^ (uint32_t) getTick();
    bomber_spawn();
}

static void bomber_tryMove(int8_t dx, int8_t dy)
{
    int16_t x = bomber.player_x + dx;
    int16_t y = bomber.player_y + dy;

    if((x < 0) || (y < 0) || (x >= BOMBER_W) || (y >= BOMBER_H))
        return;
    if(bomber_isBlocked(x, y))
        return;
    if((bomber.bomb_timer > 0) && (x == bomber.bomb_x) && (y == bomber.bomb_y))
        return;

    bomber.player_x = x;
    bomber.player_y = y;
}

static void bomber_placeBomb(void)
{
    if(bomber.bomb_timer > 0)
        return;

    bomber.bomb_x = bomber.player_x;
    bomber.bomb_y = bomber.player_y;
    bomber.bomb_timer = 7;
}

static void bomber_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_UP | KEY_2))
        bomber_tryMove(0, -1);
    else if(msg.keys & (KEY_DOWN | KEY_8))
        bomber_tryMove(0, 1);
    else if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
        bomber_tryMove(-1, 0);
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
        bomber_tryMove(1, 0);
    else if(msg.keys & (KEY_ENTER | KEY_5))
        bomber_placeBomb();
}

static void bomber_moveEnemy(uint8_t idx)
{
    static const int8_t move_x[4] = {1, -1, 0, 0};
    static const int8_t move_y[4] = {0, 0, 1, -1};

    for(uint8_t tries = 0; tries < 4; tries++)
    {
        uint8_t choice = (bomber_rand() + tries) % 4;
        int16_t x = bomber.enemy_x[idx] + move_x[choice];
        int16_t y = bomber.enemy_y[idx] + move_y[choice];

        if((x < 0) || (y < 0) || (x >= BOMBER_W) || (y >= BOMBER_H))
            continue;
        if(bomber_isBlocked(x, y))
            continue;
        if((bomber.bomb_timer > 0) && (x == bomber.bomb_x) && (y == bomber.bomb_y))
            continue;

        bomber.enemy_x[idx] = x;
        bomber.enemy_y[idx] = y;
        break;
    }
}

static void bomber_checkHits(void)
{
    if(bomber_isBlastCell(bomber.player_x, bomber.player_y))
        bomber.game_over = true;

    for(uint8_t i = 0; i < BOMBER_ENEMIES; i++)
    {
        if((bomber.enemy_x[i] == bomber.player_x) && (bomber.enemy_y[i] == bomber.player_y))
            bomber.game_over = true;

        if(bomber_isBlastCell(bomber.enemy_x[i], bomber.enemy_y[i]))
        {
            bomber.score += 100;
            if(bomber.score > bomber.best_score)
            {
                bomber.best_score = bomber.score;
                ui_games_writeBestScore(UI_GAME_BOMBER, bomber.best_score);
            }

            bomber.enemy_x[i] = (i == 0) ? (BOMBER_W - 1) : (BOMBER_W - 1);
            bomber.enemy_y[i] = (i == 0) ? (BOMBER_H - 1) : 0;
        }
    }
}

static void bomber_tick(const ui_game_layout_t *layout)
{
    (void) layout;

    if(bomber.game_over)
        return;

    for(uint8_t i = 0; i < BOMBER_ENEMIES; i++)
        bomber_moveEnemy(i);

    if(bomber.bomb_timer > 0)
    {
        bomber.bomb_timer -= 1;
        if(bomber.bomb_timer == 0)
            bomber.blast_timer = 2;
    }
    else if(bomber.blast_timer > 0)
    {
        bomber_checkHits();
        bomber.blast_timer -= 1;
        if(bomber.blast_timer == 0)
        {
            bomber.bomb_x = -1;
            bomber.bomb_y = -1;
        }
    }

    bomber_checkHits();
}

static void bomber_draw(const ui_game_layout_t *layout)
{
    uint8_t tile = (CONFIG_SCREEN_HEIGHT > 64) ? 12 : 9;
    uint16_t board_width = BOMBER_W * tile;
    uint16_t board_height = BOMBER_H * tile;
    point_t origin =
    {
        (CONFIG_SCREEN_WIDTH - board_width) / 2,
        layout->board_origin.y + ((layout->board_height - board_height) / 2)
    };

    for(uint8_t y = 0; y < BOMBER_H; y++)
    {
        for(uint8_t x = 0; x < BOMBER_W; x++)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};

            gfx_drawRect(pos, tile - 1, tile - 1, color_grey, false);
            if(bomber.blocks[y][x])
                gfx_drawRect((point_t){pos.x + 1, pos.y + 1}, tile - 3, tile - 3, color_grey, true);
            if(bomber_isBlastCell(x, y))
                gfx_drawRect((point_t){pos.x + 2, pos.y + 2}, tile - 5, tile - 5, yellow_fab413, true);
        }
    }

    for(uint8_t i = 0; i < BOMBER_ENEMIES; i++)
    {
        point_t pos = {origin.x + (bomber.enemy_x[i] * tile), origin.y + (bomber.enemy_y[i] * tile)};
        gfx_drawRect((point_t){pos.x + 2, pos.y + 2}, tile - 5, tile - 5, color_white, false);
    }

    if(bomber.bomb_timer > 0)
    {
        point_t pos = {origin.x + (bomber.bomb_x * tile), origin.y + (bomber.bomb_y * tile)};
        gfx_drawCircle((point_t){pos.x + (tile / 2), pos.y + (tile / 2)}, (tile / 2) - 1, color_white);
        gfx_drawRect((point_t){pos.x + 3, pos.y + 3}, tile - 7, tile - 7, color_white, true);
    }

    {
        point_t pos = {origin.x + (bomber.player_x * tile), origin.y + (bomber.player_y * tile)};
        gfx_drawRect((point_t){pos.x + 2, pos.y + 2}, tile - 5, tile - 5, yellow_fab413, true);
    }
}

static uint16_t bomber_getScore(void)
{
    return bomber.score;
}

static uint16_t bomber_getBestScore(void)
{
    return bomber.best_score;
}

static bool bomber_isGameOver(void)
{
    return bomber.game_over;
}

const ui_game_driver_t ui_game_bomber =
{
    .id = UI_GAME_BOMBER,
    .title = "Bomber Lite",
    .tick_period_ms = 220,
    .start = bomber_start,
    .handleInput = bomber_handleInput,
    .tick = bomber_tick,
    .draw = bomber_draw,
    .getScore = bomber_getScore,
    .getBestScore = bomber_getBestScore,
    .isGameOver = bomber_isGameOver,
};
