/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "interfaces/delays.h"
#include "ui/ui_games_internal.h"
#include "ui/ui_default.h"

#define TETRIS_W 10
#define TETRIS_H 14

typedef struct tetromino_t
{
    int8_t blocks[4][4][2];
} tetromino_t;

typedef struct tetris_state_t
{
    uint8_t board[TETRIS_H][TETRIS_W];
    uint8_t piece;
    uint8_t rotation;
    int8_t x;
    int8_t y;
    uint16_t score;
    uint16_t best_score;
    bool game_over;
    uint32_t rng_state;
} tetris_state_t;

static const tetromino_t tetrominoes[] =
{
    {{{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}}},
    {{{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}}},
    {{{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}}},
    {{{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}}},
    {{{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}}},
    {{{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}}},
    {{{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}},
};

static tetris_state_t tetris;

static uint32_t tetris_rand(void)
{
    tetris.rng_state ^= (uint32_t) getTick() + (tetris.rng_state << 7) + (tetris.rng_state >> 3);
    tetris.rng_state ^= tetris.rng_state << 13;
    tetris.rng_state ^= tetris.rng_state >> 17;
    tetris.rng_state ^= tetris.rng_state << 5;
    return tetris.rng_state;
}

static bool tetris_collides(int8_t px, int8_t py, uint8_t piece, uint8_t rot)
{
    for(uint8_t i = 0; i < 4; i++)
    {
        int8_t x = px + tetrominoes[piece].blocks[rot][i][0];
        int8_t y = py + tetrominoes[piece].blocks[rot][i][1];

        if((x < 0) || (x >= TETRIS_W) || (y >= TETRIS_H))
            return true;

        if((y >= 0) && tetris.board[y][x])
            return true;
    }

    return false;
}

static void tetris_spawnPiece(void)
{
    tetris.piece = tetris_rand() % (sizeof(tetrominoes) / sizeof(tetrominoes[0]));
    tetris.rotation = 0;
    tetris.x = 3;
    tetris.y = -1;

    if(tetris_collides(tetris.x, tetris.y, tetris.piece, tetris.rotation))
        tetris.game_over = true;
}

static void tetris_lockPiece(void)
{
    for(uint8_t i = 0; i < 4; i++)
    {
        int8_t x = tetris.x + tetrominoes[tetris.piece].blocks[tetris.rotation][i][0];
        int8_t y = tetris.y + tetrominoes[tetris.piece].blocks[tetris.rotation][i][1];

        if((x >= 0) && (x < TETRIS_W) && (y >= 0) && (y < TETRIS_H))
            tetris.board[y][x] = 1;
    }
}

static void tetris_clearLines(void)
{
    for(int8_t y = TETRIS_H - 1; y >= 0; y--)
    {
        bool full = true;

        for(uint8_t x = 0; x < TETRIS_W; x++)
        {
            if(!tetris.board[y][x])
            {
                full = false;
                break;
            }
        }

        if(full)
        {
            for(int8_t row = y; row > 0; row--)
                memcpy(tetris.board[row], tetris.board[row - 1], TETRIS_W);

            memset(tetris.board[0], 0, TETRIS_W);
            tetris.score += 100;
            if(tetris.score > tetris.best_score)
            {
                tetris.best_score = tetris.score;
                ui_games_writeBestScore(UI_GAME_TETRIS, tetris.best_score);
            }
            y++;
        }
    }
}

static void tetris_stepDown(void)
{
    if(!tetris_collides(tetris.x, tetris.y + 1, tetris.piece, tetris.rotation))
    {
        tetris.y += 1;
        return;
    }

    tetris_lockPiece();
    tetris_clearLines();
    tetris_spawnPiece();
}

static void tetris_start(const ui_game_layout_t *layout)
{
    (void) layout;
    memset(&tetris, 0, sizeof(tetris));
    tetris.best_score = ui_games_readBestScore(UI_GAME_TETRIS);
    tetris.rng_state = 0x41C64E6Du ^ (uint32_t) getTick();
    tetris_spawnPiece();
}

static void tetris_handleInput(kbd_msg_t msg)
{
    if(msg.keys & (KEY_LEFT | KEY_4 | KNOB_LEFT))
    {
        if(!tetris_collides(tetris.x - 1, tetris.y, tetris.piece, tetris.rotation))
            tetris.x -= 1;
    }
    else if(msg.keys & (KEY_RIGHT | KEY_6 | KNOB_RIGHT))
    {
        if(!tetris_collides(tetris.x + 1, tetris.y, tetris.piece, tetris.rotation))
            tetris.x += 1;
    }
    else if(msg.keys & (KEY_UP | KEY_8))
    {
        uint8_t next_rot = (tetris.rotation + 1) % 4;
        if(!tetris_collides(tetris.x, tetris.y, tetris.piece, next_rot))
            tetris.rotation = next_rot;
    }
    else if(msg.keys & (KEY_DOWN | KEY_2))
    {
        tetris_stepDown();
        tetris.score += 1;
        if(tetris.score > tetris.best_score)
        {
            tetris.best_score = tetris.score;
            ui_games_writeBestScore(UI_GAME_TETRIS, tetris.best_score);
        }
    }
}

static void tetris_tick(const ui_game_layout_t *layout)
{
    (void) layout;
    if(!tetris.game_over)
        tetris_stepDown();
}

static void tetris_draw(const ui_game_layout_t *layout)
{
    uint8_t tile = (CONFIG_SCREEN_HEIGHT > 64) ? 8 : 7;
    uint16_t board_width = TETRIS_W * tile;
    uint16_t board_height = TETRIS_H * tile;
    point_t origin =
    {
        (CONFIG_SCREEN_WIDTH - board_width) / 2,
        layout->board_origin.y + ((layout->board_height - board_height) / 2)
    };

    for(uint8_t y = 0; y < TETRIS_H; y++)
    {
        for(uint8_t x = 0; x < TETRIS_W; x++)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};
            gfx_drawRect(pos, tile - 1, tile - 1, color_grey, false);
            if(tetris.board[y][x])
                gfx_drawRect((point_t){pos.x + 1, pos.y + 1}, tile - 3, tile - 3, color_white, true);
        }
    }

    for(uint8_t i = 0; i < 4; i++)
    {
        int8_t x = tetris.x + tetrominoes[tetris.piece].blocks[tetris.rotation][i][0];
        int8_t y = tetris.y + tetrominoes[tetris.piece].blocks[tetris.rotation][i][1];

        if(y >= 0)
        {
            point_t pos = {origin.x + (x * tile), origin.y + (y * tile)};
            gfx_drawRect((point_t){pos.x + 1, pos.y + 1}, tile - 3, tile - 3, yellow_fab413, true);
        }
    }
}

static uint16_t tetris_getScore(void)
{
    return tetris.score;
}

static uint16_t tetris_getBestScore(void)
{
    return tetris.best_score;
}

static bool tetris_isGameOver(void)
{
    return tetris.game_over;
}

const ui_game_driver_t ui_game_tetris =
{
    .id = UI_GAME_TETRIS,
    .title = "Tetris",
    .tick_period_ms = 350,
    .start = tetris_start,
    .handleInput = tetris_handleInput,
    .tick = tetris_tick,
    .draw = tetris_draw,
    .getScore = tetris_getScore,
    .getBestScore = tetris_getBestScore,
    .isGameOver = tetris_isGameOver,
};
