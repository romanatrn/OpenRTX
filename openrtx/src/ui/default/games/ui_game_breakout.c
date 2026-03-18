/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define BREAKOUT_COLS 8
#define BREAKOUT_ROWS 4

typedef struct breakout_state_t
{
    uint8_t bricks[BREAKOUT_ROWS][BREAKOUT_COLS];
    int16_t ball_x;
    int16_t ball_y;
    int8_t dir_x;
    int8_t dir_y;
    uint8_t paddle_x;
    uint16_t score;
    uint16_t best_score;
    bool game_over;
} breakout_state_t;

static breakout_state_t breakout;

static void breakout_resetBricks(void)
{
    for(uint8_t y = 0; y < BREAKOUT_ROWS; y++)
        for(uint8_t x = 0; x < BREAKOUT_COLS; x++)
            breakout.bricks[y][x] = 1;
}

static void breakout_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&breakout, 0, sizeof(breakout));
    breakout.best_score = ui_games_readBestScore(UI_GAME_BREAKOUT);
    breakout_resetBricks();
    breakout.ball_x = BREAKOUT_COLS * 6;
    breakout.ball_y = 30;
    breakout.dir_x = 1;
    breakout.dir_y = -1;
    breakout.paddle_x = 16;
}

static void breakout_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
    {
        if(breakout.paddle_x >= 3)
            breakout.paddle_x -= 3;
    }
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
    {
        if(breakout.paddle_x < (CONFIG_SCREEN_WIDTH - 24))
            breakout.paddle_x += 3;
    }
}

static void breakout_tick(const ui_game_layout_t *layout)
{
    (void) layout;

    if(breakout.game_over)
        return;

    int16_t next_x = breakout.ball_x + breakout.dir_x;
    int16_t next_y = breakout.ball_y + breakout.dir_y;

    if((next_x <= 2) || (next_x >= (CONFIG_SCREEN_WIDTH - 4)))
    {
        breakout.dir_x = -breakout.dir_x;
        next_x = breakout.ball_x + breakout.dir_x;
    }

    if(next_y <= 16)
    {
        breakout.dir_y = 1;
        next_y = breakout.ball_y + breakout.dir_y;
    }

    if((next_y >= (CONFIG_SCREEN_HEIGHT - 10)) &&
       (next_x >= breakout.paddle_x) &&
       (next_x <= (breakout.paddle_x + 20)))
    {
        breakout.dir_y = -1;
        if(next_x < (breakout.paddle_x + 10)) breakout.dir_x = -1;
        else breakout.dir_x = 1;
        next_y = breakout.ball_y + breakout.dir_y;
    }

    for(uint8_t y = 0; y < BREAKOUT_ROWS; y++)
    {
        for(uint8_t x = 0; x < BREAKOUT_COLS; x++)
        {
            if(!breakout.bricks[y][x])
                continue;

            int16_t brick_x = 4 + (x * 15);
            int16_t brick_y = 18 + (y * 7);
            if((next_x >= brick_x) && (next_x <= (brick_x + 12)) &&
               (next_y >= brick_y) && (next_y <= (brick_y + 4)))
            {
                breakout.bricks[y][x] = 0;
                breakout.dir_y = -breakout.dir_y;
                breakout.score += 10;
                if(breakout.score > breakout.best_score)
                {
                    breakout.best_score = breakout.score;
                    ui_games_writeBestScore(UI_GAME_BREAKOUT, breakout.best_score);
                }
                next_y = breakout.ball_y + breakout.dir_y;
            }
        }
    }

    breakout.ball_x = next_x;
    breakout.ball_y = next_y;

    if(breakout.ball_y >= (CONFIG_SCREEN_HEIGHT - 2))
        breakout.game_over = true;
}

static void breakout_draw(const ui_game_layout_t *layout)
{
    (void) layout;

    for(uint8_t y = 0; y < BREAKOUT_ROWS; y++)
    {
        for(uint8_t x = 0; x < BREAKOUT_COLS; x++)
        {
            if(!breakout.bricks[y][x])
                continue;

            point_t pos = {4 + (x * 15), 18 + (y * 7)};
            color_t color = ((x + y) & 1) ? color_white : yellow_fab413;
            gfx_drawRect(pos, 12, 4, color, true);
        }
    }

    gfx_drawRect((point_t){breakout.paddle_x, CONFIG_SCREEN_HEIGHT - 8}, 20, 3, color_white, true);
    gfx_drawRect((point_t){breakout.ball_x, breakout.ball_y}, 3, 3, yellow_fab413, true);
}

static uint16_t breakout_getScore(void)
{
    return breakout.score;
}

static uint16_t breakout_getBestScore(void)
{
    return breakout.best_score;
}

static bool breakout_isGameOver(void)
{
    return breakout.game_over;
}

const ui_game_driver_t ui_game_breakout =
{
    .id = UI_GAME_BREAKOUT,
    .title = "Breakout",
    .tick_period_ms = 45,
    .start = breakout_start,
    .handleInput = breakout_handleInput,
    .tick = breakout_tick,
    .draw = breakout_draw,
    .getScore = breakout_getScore,
    .getBestScore = breakout_getBestScore,
    .isGameOver = breakout_isGameOver,
};
