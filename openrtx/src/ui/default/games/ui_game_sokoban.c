/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define SOKOBAN_MAX_WIDTH  10
#define SOKOBAN_MAX_HEIGHT 8
#define SOKOBAN_MAX_BOXES  8

typedef struct sokoban_level_t
{
    uint8_t width;
    uint8_t height;
    const char *rows[SOKOBAN_MAX_HEIGHT];
} sokoban_level_t;

typedef struct sokoban_state_t
{
    uint8_t walls[SOKOBAN_MAX_HEIGHT][SOKOBAN_MAX_WIDTH];
    uint8_t targets[SOKOBAN_MAX_HEIGHT][SOKOBAN_MAX_WIDTH];
    uint8_t boxes[SOKOBAN_MAX_BOXES][2];
    uint8_t num_boxes;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t level_index;
    uint16_t score;
    uint16_t best_score;
    bool game_over;
} sokoban_state_t;

static const sokoban_level_t sokoban_levels[] =
{
    {7, 7, {"#######", "#  .  #", "#  $  #", "# $$@ #", "#  .  #", "#  .  #", "#######"}},
    {8, 7, {"########", "# .  ###", "# $$   #", "### @  #", "#  $$ .#", "#   .  #", "########"}},
    {9, 8, {"#########", "# . .   #", "# $$#$  #", "#   @   #", "#  $$ . #", "#   .   #", "#       #", "#########"}},
    {10, 8, {"##########", "#   .    #", "# $$.$$  #", "#   ##   #", "# @   $  #", "#   . .  #", "#        #", "##########"}},
};

static sokoban_state_t sokoban;

static void sokoban_loadLevel(uint8_t level_index)
{
    const sokoban_level_t *level = &sokoban_levels[level_index];

    memset(sokoban.walls, 0, sizeof(sokoban.walls));
    memset(sokoban.targets, 0, sizeof(sokoban.targets));
    memset(sokoban.boxes, 0, sizeof(sokoban.boxes));
    sokoban.num_boxes = 0;
    sokoban.level_index = level_index;

    for(uint8_t y = 0; y < level->height; y++)
    {
        for(uint8_t x = 0; x < level->width; x++)
        {
            char tile = level->rows[y][x];

            if(tile == '#')
                sokoban.walls[y][x] = 1;
            else if((tile == '.') || (tile == '+') || (tile == '*'))
                sokoban.targets[y][x] = 1;

            if((tile == '$') || (tile == '*'))
            {
                sokoban.boxes[sokoban.num_boxes][0] = x;
                sokoban.boxes[sokoban.num_boxes][1] = y;
                sokoban.num_boxes += 1;
            }

            if((tile == '@') || (tile == '+'))
            {
                sokoban.player_x = x;
                sokoban.player_y = y;
            }
        }
    }
}

static int8_t sokoban_findBox(uint8_t x, uint8_t y)
{
    for(uint8_t i = 0; i < sokoban.num_boxes; i++)
    {
        if((sokoban.boxes[i][0] == x) && (sokoban.boxes[i][1] == y))
            return i;
    }

    return -1;
}

static bool sokoban_isSolved(void)
{
    for(uint8_t i = 0; i < sokoban.num_boxes; i++)
    {
        if(sokoban.targets[sokoban.boxes[i][1]][sokoban.boxes[i][0]] == 0)
            return false;
    }

    return true;
}

static void sokoban_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&sokoban, 0, sizeof(sokoban));
    sokoban.best_score = ui_games_readBestScore(UI_GAME_SOKOBAN);
    sokoban_loadLevel(0);
}

static void sokoban_tryMove(int8_t dx, int8_t dy)
{
    const sokoban_level_t *level = &sokoban_levels[sokoban.level_index];
    int16_t next_x = sokoban.player_x + dx;
    int16_t next_y = sokoban.player_y + dy;

    if((next_x < 0) || (next_y < 0) ||
       (next_x >= level->width) || (next_y >= level->height) ||
       sokoban.walls[next_y][next_x])
    {
        return;
    }

    int8_t box_index = sokoban_findBox(next_x, next_y);
    if(box_index >= 0)
    {
        int16_t push_x = next_x + dx;
        int16_t push_y = next_y + dy;

        if((push_x < 0) || (push_y < 0) ||
           (push_x >= level->width) || (push_y >= level->height) ||
           sokoban.walls[push_y][push_x] ||
           (sokoban_findBox(push_x, push_y) >= 0))
        {
            return;
        }

        sokoban.boxes[box_index][0] = push_x;
        sokoban.boxes[box_index][1] = push_y;
    }

    sokoban.player_x = next_x;
    sokoban.player_y = next_y;
}

static void sokoban_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_UP | KEY_2))
        sokoban_tryMove(0, -1);
    else if(msg.keys & (KEY_DOWN | KEY_8))
        sokoban_tryMove(0, 1);
    else if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
        sokoban_tryMove(-1, 0);
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
        sokoban_tryMove(1, 0);
}

static void sokoban_tick(const ui_game_layout_t *layout)
{
    (void) layout;

    if(!sokoban_isSolved())
        return;

    sokoban.score += 1;
    if(sokoban.score > sokoban.best_score)
    {
        sokoban.best_score = sokoban.score;
        ui_games_writeBestScore(UI_GAME_SOKOBAN, sokoban.best_score);
    }

    if((sokoban.level_index + 1u) < (sizeof(sokoban_levels) / sizeof(sokoban_levels[0])))
        sokoban_loadLevel(sokoban.level_index + 1);
    else
        sokoban.game_over = true;
}

static void sokoban_drawCell(point_t origin, uint8_t tile, color_t color, bool filled)
{
    gfx_drawRect(origin, tile, tile, color, filled);
}

static void sokoban_draw(const ui_game_layout_t *layout)
{
    const sokoban_level_t *level = &sokoban_levels[sokoban.level_index];
    uint8_t tile = layout->tile_size;
    uint16_t board_width = level->width * tile;
    uint16_t board_height = level->height * tile;
    point_t origin =
    {
        (CONFIG_SCREEN_WIDTH - board_width) / 2,
        layout->board_origin.y + ((layout->board_height - board_height) / 2)
    };

    for(uint8_t y = 0; y < level->height; y++)
    {
        for(uint8_t x = 0; x < level->width; x++)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};

            if(sokoban.walls[y][x])
                sokoban_drawCell(pos, tile - 1, color_grey, true);
            else
                sokoban_drawCell(pos, tile - 1, color_grey, false);

            if(sokoban.targets[y][x])
                gfx_drawRect((point_t){pos.x + 2, pos.y + 2}, tile - 5, tile - 5, yellow_fab413, false);
        }
    }

    for(uint8_t i = 0; i < sokoban.num_boxes; i++)
    {
        point_t pos =
        {
            origin.x + (sokoban.boxes[i][0] * tile),
            origin.y + (sokoban.boxes[i][1] * tile)
        };
        sokoban_drawCell((point_t){pos.x + 1, pos.y + 1}, tile - 3, color_white, true);
    }

    gfx_drawRect((point_t){origin.x + (sokoban.player_x * tile) + 2,
                           origin.y + (sokoban.player_y * tile) + 2},
                 tile - 5,
                 tile - 5,
                 yellow_fab413,
                 true);
}

static uint16_t sokoban_getScore(void)
{
    return sokoban.score;
}

static uint16_t sokoban_getBestScore(void)
{
    return sokoban.best_score;
}

static bool sokoban_isGameOver(void)
{
    return sokoban.game_over;
}

const ui_game_driver_t ui_game_sokoban =
{
    .id = UI_GAME_SOKOBAN,
    .title = "Sokoban",
    .tick_period_ms = 90,
    .start = sokoban_start,
    .handleInput = sokoban_handleInput,
    .tick = sokoban_tick,
    .draw = sokoban_draw,
    .getScore = sokoban_getScore,
    .getBestScore = sokoban_getBestScore,
    .isGameOver = sokoban_isGameOver,
};
