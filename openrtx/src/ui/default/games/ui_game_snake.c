/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "interfaces/delays.h"
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define SNAKE_MAX_CELLS 216

typedef struct snake_state_t
{
    uint8_t  body_x[SNAKE_MAX_CELLS];
    uint8_t  body_y[SNAKE_MAX_CELLS];
    uint16_t length;
    int8_t   dir_x;
    int8_t   dir_y;
    int8_t   next_dir_x;
    int8_t   next_dir_y;
    uint8_t  food_x;
    uint8_t  food_y;
    uint16_t score;
    uint16_t best_score;
    bool     game_over;
} snake_state_t;

static snake_state_t snake;
static uint32_t snake_rng_state = 0x53A9B41Du;

static uint32_t snake_nextRandom(void)
{
    uint32_t tick = (uint32_t) getTick();

    snake_rng_state ^= tick + 0x9E3779B9u + (snake_rng_state << 6) + (snake_rng_state >> 2);
    snake_rng_state ^= snake_rng_state << 13;
    snake_rng_state ^= snake_rng_state >> 17;
    snake_rng_state ^= snake_rng_state << 5;

    return snake_rng_state;
}

static bool snake_isCellOccupied(uint8_t x, uint8_t y)
{
    for(uint16_t i = 0; i < snake.length; i++)
    {
        if((snake.body_x[i] == x) && (snake.body_y[i] == y))
            return true;
    }

    return false;
}

static void snake_spawnFood(const ui_game_layout_t *layout)
{
    uint16_t cell_count = layout->cols * layout->rows;

    if(snake.length >= cell_count)
    {
        snake.game_over = true;
        return;
    }

    uint16_t offset = snake_nextRandom() % cell_count;

    for(uint16_t i = 0; i < cell_count; i++)
    {
        uint16_t cell = (offset + i) % cell_count;
        uint8_t x = cell % layout->cols;
        uint8_t y = cell / layout->cols;

        if(!snake_isCellOccupied(x, y))
        {
            snake.food_x = x;
            snake.food_y = y;
            return;
        }
    }
}

static bool snake_isReverse(int8_t dx, int8_t dy)
{
    return (snake.length > 1) &&
           (dx == -snake.dir_x) &&
           (dy == -snake.dir_y);
}

static void snake_setDirection(int8_t dx, int8_t dy)
{
    if(((dx == 0) && (dy == 0)) || snake_isReverse(dx, dy))
        return;

    snake.next_dir_x = dx;
    snake.next_dir_y = dy;
}

static void snake_start(const ui_game_layout_t *layout)
{
    memset(&snake, 0, sizeof(snake));
    snake.best_score = ui_games_readBestScore(UI_GAME_SNAKE);
    snake_rng_state ^= (uint32_t) getTick();

    snake.length = 3;
    snake.dir_x = 1;
    snake.dir_y = 0;
    snake.next_dir_x = 1;
    snake.next_dir_y = 0;

    uint8_t start_x = layout->cols / 2;
    uint8_t start_y = layout->rows / 2;

    for(uint8_t i = 0; i < snake.length; i++)
    {
        snake.body_x[i] = start_x - i;
        snake.body_y[i] = start_y;
    }

    snake_spawnFood(layout);
}

static void snake_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_UP | KEY_2))
        snake_setDirection(0, -1);
    else if(msg.keys & (KEY_DOWN | KEY_8))
        snake_setDirection(0, 1);
    else if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
        snake_setDirection(-1, 0);
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
        snake_setDirection(1, 0);
}

static void snake_tick(const ui_game_layout_t *layout)
{
    if(snake.game_over)
        return;

    snake.dir_x = snake.next_dir_x;
    snake.dir_y = snake.next_dir_y;

    int16_t next_x = snake.body_x[0] + snake.dir_x;
    int16_t next_y = snake.body_y[0] + snake.dir_y;

    if((next_x < 0) || (next_y < 0) ||
       (next_x >= layout->cols) || (next_y >= layout->rows))
    {
        snake.game_over = true;
        return;
    }

    bool grow = (next_x == snake.food_x) && (next_y == snake.food_y);
    uint16_t collision_length = grow ? snake.length : (snake.length - 1);

    for(uint16_t i = 0; i < collision_length; i++)
    {
        if((snake.body_x[i] == next_x) && (snake.body_y[i] == next_y))
        {
            snake.game_over = true;
            return;
        }
    }

    uint16_t new_length = snake.length + (grow ? 1 : 0);
    if(new_length > SNAKE_MAX_CELLS)
        new_length = SNAKE_MAX_CELLS;

    for(uint16_t i = new_length - 1; i > 0; i--)
    {
        snake.body_x[i] = snake.body_x[i - 1];
        snake.body_y[i] = snake.body_y[i - 1];
    }

    snake.body_x[0] = next_x;
    snake.body_y[0] = next_y;
    snake.length = new_length;

    if(grow)
    {
        snake.score += 1;
        if(snake.score > snake.best_score)
        {
            snake.best_score = snake.score;
            ui_games_writeBestScore(UI_GAME_SNAKE, snake.best_score);
        }
        snake_spawnFood(layout);
    }
}

static void snake_drawCell(const ui_game_layout_t *layout,
                           uint8_t x,
                           uint8_t y,
                           color_t color,
                           bool fill)
{
    point_t pos =
    {
        layout->board_origin.x + (x * layout->tile_size),
        layout->board_origin.y + (y * layout->tile_size)
    };

    uint16_t size = layout->tile_size;
    if(size > 2)
        size -= 1;

    gfx_drawRect(pos, size, size, color, fill);
}

static void snake_draw(const ui_game_layout_t *layout)
{
    snake_drawCell(layout, snake.food_x, snake.food_y, yellow_fab413, false);

    for(uint16_t i = snake.length; i > 0; i--)
    {
        color_t color = (i == 1) ? yellow_fab413 : color_white;
        snake_drawCell(layout, snake.body_x[i - 1], snake.body_y[i - 1], color, true);
    }
}

static uint16_t snake_getScore(void)
{
    return snake.score;
}

static uint16_t snake_getBestScore(void)
{
    return snake.best_score;
}

static bool snake_isGameOver(void)
{
    return snake.game_over;
}

const ui_game_driver_t ui_game_snake =
{
    .id = UI_GAME_SNAKE,
    .title = "Snake",
    .tick_period_ms = 180,
    .start = snake_start,
    .handleInput = snake_handleInput,
    .tick = snake_tick,
    .draw = snake_draw,
    .getScore = snake_getScore,
    .getBestScore = snake_getBestScore,
    .isGameOver = snake_isGameOver,
};
