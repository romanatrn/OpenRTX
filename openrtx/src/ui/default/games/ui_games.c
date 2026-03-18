/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "interfaces/delays.h"
#include "ui/ui_games.h"
#include "ui/ui_games_internal.h"

typedef enum ui_game_phase_t
{
    UI_GAME_PHASE_TITLE = 0,
    UI_GAME_PHASE_PLAYING,
    UI_GAME_PHASE_PAUSED,
    UI_GAME_PHASE_GAME_OVER,
} ui_game_phase_t;

typedef struct ui_games_persist_t
{
    uint16_t snake_best;
    uint16_t tetris_best;
    uint16_t bomber_best;
    uint16_t mines_best;
} ui_games_persist_t;

typedef struct ui_games_runtime_t
{
    const ui_game_driver_t *active_game;
    ui_game_layout_t        layout;
    ui_games_persist_t      persist;
    uint32_t                next_tick;
    uint8_t                 selected_game;
    ui_game_phase_t         phase;
    bool                    persist_dirty;
} ui_games_runtime_t;

static const ui_game_driver_t *games_catalog[] =
{
    &ui_game_snake,
    &ui_game_tetris,
    &ui_game_bomber,
    &ui_game_mines,
};

static ui_games_runtime_t ui_games_runtime;

static uint8_t ui_games_getCount(void)
{
    return sizeof(games_catalog) / sizeof(games_catalog[0]);
}

static void ui_games_loadPersistence(void)
{
    ui_games_runtime.persist.snake_best = state.settings.snake_high_score;
    ui_games_runtime.persist.tetris_best = state.settings.tetris_high_score;
    ui_games_runtime.persist.bomber_best = state.settings.bomber_high_score;
    ui_games_runtime.persist.mines_best = state.settings.mines_high_score;
    ui_games_runtime.persist_dirty = false;
}

uint16_t ui_games_readBestScore(ui_game_id_t game_id)
{
    switch(game_id)
    {
        case UI_GAME_SNAKE:   return ui_games_runtime.persist.snake_best;
        case UI_GAME_TETRIS:  return ui_games_runtime.persist.tetris_best;
        case UI_GAME_BOMBER:  return ui_games_runtime.persist.bomber_best;
        case UI_GAME_MINES:   return ui_games_runtime.persist.mines_best;
        default:              return 0;
    }
}

void ui_games_writeBestScore(ui_game_id_t game_id, uint16_t value)
{
    switch(game_id)
    {
        case UI_GAME_SNAKE:
            if(value > ui_games_runtime.persist.snake_best)
            {
                ui_games_runtime.persist.snake_best = value;
                ui_games_runtime.persist_dirty = true;
            }
            break;

        case UI_GAME_TETRIS:
            if(value > ui_games_runtime.persist.tetris_best)
            {
                ui_games_runtime.persist.tetris_best = value;
                ui_games_runtime.persist_dirty = true;
            }
            break;

        case UI_GAME_BOMBER:
            if(value > ui_games_runtime.persist.bomber_best)
            {
                ui_games_runtime.persist.bomber_best = value;
                ui_games_runtime.persist_dirty = true;
            }
            break;

        case UI_GAME_MINES:
            if(value > ui_games_runtime.persist.mines_best)
            {
                ui_games_runtime.persist.mines_best = value;
                ui_games_runtime.persist_dirty = true;
            }
            break;
    }
}

void ui_games_syncPersistence(void)
{
    if(!ui_games_runtime.persist_dirty)
        return;

    state.settings.snake_high_score = ui_games_runtime.persist.snake_best;
    state.settings.tetris_high_score = ui_games_runtime.persist.tetris_best;
    state.settings.bomber_high_score = ui_games_runtime.persist.bomber_best;
    state.settings.mines_high_score = ui_games_runtime.persist.mines_best;
    ui_games_runtime.persist_dirty = false;
}

static void ui_games_updateLayout(void)
{
    ui_game_layout_t *layout = &ui_games_runtime.layout;

#if CONFIG_SCREEN_HEIGHT > 64
    layout->tile_size = 8;
    layout->cols = 18;
    layout->rows = 12;
    layout->header_height = 18;
#else
    layout->tile_size = 7;
    layout->cols = 16;
    layout->rows = 7;
    layout->header_height = 12;
#endif

    layout->board_width = layout->cols * layout->tile_size;
    layout->board_height = layout->rows * layout->tile_size;
    layout->board_origin.x = (CONFIG_SCREEN_WIDTH - layout->board_width) / 2;
    layout->board_origin.y = layout->header_height +
                             ((CONFIG_SCREEN_HEIGHT - layout->header_height - layout->board_height) / 2);
}

static const ui_game_driver_t *ui_games_getSelectedGame(const ui_state_t *ui_state)
{
    uint8_t selected = ui_state->menu_selected;

    if(selected >= ui_games_getCount())
        selected = 0;

    return games_catalog[selected];
}

static void ui_games_beginTitle(const ui_state_t *ui_state)
{
    ui_games_runtime.selected_game = ui_state->menu_selected;
    ui_games_runtime.active_game = ui_games_getSelectedGame(ui_state);
    ui_games_runtime.phase = UI_GAME_PHASE_TITLE;
    ui_games_runtime.next_tick = 0;
    state.ui_screen = GAME_RUN;
}

static void ui_games_beginPlay(void)
{
    ui_games_runtime.active_game->start(&ui_games_runtime.layout);
    ui_games_runtime.next_tick = getTick() + ui_games_runtime.active_game->tick_period_ms;
    ui_games_runtime.phase = UI_GAME_PHASE_PLAYING;
}

static void ui_games_drawLibraryItem(point_t *pos,
                                     const char *title,
                                     bool selected)
{
    color_t text_color = color_white;

    if(selected)
    {
        point_t rect = {0, pos->y - layout.menu_h + 3};
        gfx_drawRect(rect, CONFIG_SCREEN_WIDTH, layout.menu_h + 2, yellow_fab413, true);
        text_color = color_black;
    }

    gfx_print(*pos, layout.menu_font, TEXT_ALIGN_LEFT, text_color, title);

    pos->y += layout.menu_h;
}

static void ui_games_drawTitleArt(ui_game_id_t game_id)
{
    point_t center = {CONFIG_SCREEN_WIDTH / 2, (CONFIG_SCREEN_HEIGHT / 2) + 4};

    switch(game_id)
    {
        case UI_GAME_SNAKE:
            for(uint8_t i = 0; i < 5; i++)
            {
                point_t pos = {center.x - 16 + (i * 6), center.y + ((i > 2) ? 4 : 0)};
                gfx_drawRect(pos, 5, 5, (i == 4) ? yellow_fab413 : color_white, true);
            }
            gfx_drawRect((point_t){center.x + 16, center.y - 8}, 5, 5, yellow_fab413, false);
            break;

        case UI_GAME_TETRIS:
            gfx_drawRect((point_t){center.x - 20, center.y - 12}, 17, 5, color_white, true);
            gfx_drawRect((point_t){center.x - 8, center.y - 6}, 11, 11, yellow_fab413, true);
            gfx_drawRect((point_t){center.x + 7, center.y - 12}, 5, 17, color_white, true);
            gfx_drawRect((point_t){center.x + 16, center.y - 4}, 11, 11, color_grey, true);
            break;

        case UI_GAME_BOMBER:
            gfx_drawRect((point_t){center.x - 16, center.y - 10}, 10, 10, color_white, true);
            gfx_drawRect((point_t){center.x + 6, center.y - 8}, 12, 12, color_white, false);
            gfx_drawCircle((point_t){center.x + 12, center.y - 2}, 6, yellow_fab413);
            gfx_drawRect((point_t){center.x + 10, center.y - 4}, 5, 5, yellow_fab413, true);
            gfx_drawLine((point_t){center.x + 16, center.y - 8}, (point_t){center.x + 22, center.y - 16}, color_white);
            gfx_drawRect((point_t){center.x - 20, center.y + 10}, 40, 3, color_grey, true);
            break;

        case UI_GAME_MINES:
            gfx_drawRect((point_t){center.x - 18, center.y - 12}, 36, 24, color_grey, false);
            gfx_drawLine((point_t){center.x - 6, center.y - 10}, (point_t){center.x - 6, center.y + 10}, color_grey);
            gfx_drawLine((point_t){center.x + 6, center.y - 10}, (point_t){center.x + 6, center.y + 10}, color_grey);
            gfx_drawLine((point_t){center.x - 18, center.y - 2}, (point_t){center.x + 18, center.y - 2}, color_grey);
            gfx_drawLine((point_t){center.x - 18, center.y + 6}, (point_t){center.x + 18, center.y + 6}, color_grey);
            gfx_drawCircle((point_t){center.x + 12, center.y - 8}, 4, yellow_fab413);
            gfx_drawRect((point_t){center.x - 16, center.y + 8}, 8, 8, color_white, true);
            break;
    }
}

static void ui_games_drawOverlay(const char *line1,
                                 const char *line2,
                                 const char *line3)
{
    uint16_t box_width = (CONFIG_SCREEN_HEIGHT > 64) ? 84 : 58;
    uint16_t box_height = (CONFIG_SCREEN_HEIGHT > 64) ? 54 : 38;
    point_t box =
    {
        (CONFIG_SCREEN_WIDTH - box_width) / 2,
        (CONFIG_SCREEN_HEIGHT - box_height) / 2
    };

    gfx_drawRect((point_t){box.x - 1, box.y - 1}, box_width + 2, box_height + 2, color_grey, false);
    gfx_drawRect(box, box_width, box_height, color_black, true);
    gfx_drawRect(box, box_width, box_height, color_white, false);

    gfx_print((point_t){0, box.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 16 : 13)},
              FONT_SIZE_8PT,
              TEXT_ALIGN_CENTER,
              yellow_fab413,
              line1);

    if(line2 != NULL)
    {
        gfx_print((point_t){0, box.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 31 : 24)},
                  FONT_SIZE_6PT,
                  TEXT_ALIGN_CENTER,
                  color_white,
                  line2);
    }

    if(line3 != NULL)
    {
        gfx_print((point_t){0, box.y + box_height - ((CONFIG_SCREEN_HEIGHT > 64) ? 11 : 8)},
                  FONT_SIZE_6PT,
                  TEXT_ALIGN_CENTER,
                  color_white,
                  line3);
    }
}

static void ui_games_drawTitleScreen(void)
{
    const ui_game_driver_t *game = ui_games_runtime.active_game;
    char best_buf[24];

    gfx_fillScreen(color_black);
    gfx_drawRect((point_t){layout.horizontal_pad, layout.top_h + 8},
                 CONFIG_SCREEN_WIDTH - (layout.horizontal_pad * 2),
                 CONFIG_SCREEN_HEIGHT - (layout.top_h + 12),
                 color_grey,
                 false);

    gfx_print((point_t){0, layout.top_h + 20},
              FONT_SIZE_16PT,
              TEXT_ALIGN_CENTER,
              yellow_fab413,
              game->title);
    ui_games_drawTitleArt(game->id);

    snprintf(best_buf, sizeof(best_buf), "Best %u", ui_games_readBestScore(game->id));
    gfx_print((point_t){0, CONFIG_SCREEN_HEIGHT - ((CONFIG_SCREEN_HEIGHT > 64) ? 18 : 14)},
              FONT_SIZE_6PT,
              TEXT_ALIGN_CENTER,
              yellow_fab413,
              best_buf);
}

static void ui_games_drawGameOverScreen(void)
{
    const ui_game_driver_t *game = ui_games_runtime.active_game;
    char score_buf[24];
    char best_buf[24];
    point_t panel =
    {
        layout.horizontal_pad,
        (CONFIG_SCREEN_HEIGHT > 64) ? 18 : 14
    };
    uint16_t panel_width = CONFIG_SCREEN_WIDTH - (layout.horizontal_pad * 2);
    uint16_t panel_height = CONFIG_SCREEN_HEIGHT - (panel.y * 2);

    snprintf(score_buf, sizeof(score_buf), "Score %u", game->getScore());
    snprintf(best_buf, sizeof(best_buf), "Best %u", game->getBestScore());

    gfx_fillScreen(color_black);
    gfx_drawRect(panel, panel_width, panel_height, color_grey, false);
    gfx_print((point_t){0, panel.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 14 : 12)},
              FONT_SIZE_8PT,
              TEXT_ALIGN_CENTER,
              yellow_fab413,
              "Game Over");
    gfx_print((point_t){0, panel.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 30 : 22)},
              FONT_SIZE_6PT,
              TEXT_ALIGN_CENTER,
              color_white,
              score_buf);
    gfx_print((point_t){0, panel.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 42 : 30)},
              FONT_SIZE_6PT,
              TEXT_ALIGN_CENTER,
              color_white,
              best_buf);
}

void ui_games_init(void)
{
    memset(&ui_games_runtime, 0, sizeof(ui_games_runtime));
    ui_games_updateLayout();
    ui_games_loadPersistence();
}

void ui_games_enterLibrary(ui_state_t *ui_state)
{
    ui_games_runtime.active_game = NULL;
    ui_games_runtime.phase = UI_GAME_PHASE_TITLE;
    ui_games_runtime.next_tick = 0;
    ui_games_updateLayout();

    if(ui_state->menu_selected >= ui_games_getCount())
        ui_state->menu_selected = 0;
}

void ui_games_drawLibrary(ui_state_t *ui_state)
{
    point_t pos = layout.line1_pos;
    uint8_t count = ui_games_getCount();

    gfx_fillScreen(color_black);
    gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER, color_white, "Games");

    for(uint8_t i = 0; i < count; i++)
    {
        bool selected = (ui_state->menu_selected == i);
        ui_games_drawLibraryItem(&pos,
                                 games_catalog[i]->title,
                                 selected);
    }
}

void ui_games_drawRunning(void)
{
    char score_buf[24];
    point_t border =
    {
        ui_games_runtime.layout.board_origin.x - 1,
        ui_games_runtime.layout.board_origin.y - 1
    };

    if(ui_games_runtime.active_game == NULL)
    {
        gfx_fillScreen(color_black);
        gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER, color_white, "Games");
        gfx_print(layout.line3_pos, layout.menu_font, TEXT_ALIGN_CENTER, color_white, "No game loaded");
        return;
    }

    if(ui_games_runtime.phase == UI_GAME_PHASE_TITLE)
    {
        ui_games_drawTitleScreen();
        return;
    }

    if(ui_games_runtime.phase == UI_GAME_PHASE_GAME_OVER)
    {
        ui_games_drawGameOverScreen();
        return;
    }

    gfx_fillScreen(color_black);
    snprintf(score_buf, sizeof(score_buf), "Score %u", ui_games_runtime.active_game->getScore());

    gfx_print(layout.top_pos,
              layout.top_font,
              TEXT_ALIGN_LEFT,
              color_white,
              "%s",
              ui_games_runtime.active_game->title);
    gfx_print(layout.top_pos,
              FONT_SIZE_6PT,
              TEXT_ALIGN_RIGHT,
              color_white,
              "%s",
              score_buf);
    gfx_drawRect(border,
                 ui_games_runtime.layout.board_width + 2,
                 ui_games_runtime.layout.board_height + 2,
                 color_grey,
                 false);

    ui_games_runtime.active_game->draw(&ui_games_runtime.layout);

    if(ui_games_runtime.phase == UI_GAME_PHASE_PAUSED)
        ui_games_drawOverlay("Paused", "Enter to resume", "Esc returns to menu");
}

bool ui_games_handleLibraryEvent(ui_state_t *ui_state, kbd_msg_t msg)
{
    uint8_t count = ui_games_getCount();

    if(msg.keys & (KEY_UP | KEY_LEFT | KEY_2 | KEY_4 | KNOB_LEFT))
    {
        if(ui_state->menu_selected > 0)
            ui_state->menu_selected -= 1;
        else
            ui_state->menu_selected = count - 1;

        return true;
    }

    if(msg.keys & (KEY_DOWN | KEY_RIGHT | KEY_8 | KEY_6 | KNOB_RIGHT))
    {
        if(ui_state->menu_selected + 1 < count)
            ui_state->menu_selected += 1;
        else
            ui_state->menu_selected = 0;

        return true;
    }

    if(msg.keys & KEY_ENTER)
    {
        ui_games_beginTitle(ui_state);
        return true;
    }

    if(msg.keys & KEY_ESC)
    {
        state.ui_screen = MENU_TOP;
        ui_state->menu_selected = 0;
        return true;
    }

    return false;
}

bool ui_games_handleRunningKeyEvent(kbd_msg_t msg)
{
    if(ui_games_runtime.active_game == NULL)
    {
        state.ui_screen = MENU_GAMES;
        return true;
    }

    if(msg.keys & KEY_ESC)
    {
        state.ui_screen = MENU_GAMES;
        return true;
    }

    if(ui_games_runtime.phase == UI_GAME_PHASE_TITLE)
    {
        if(msg.keys & KEY_ENTER)
            ui_games_beginPlay();

        return true;
    }

    if(ui_games_runtime.phase == UI_GAME_PHASE_GAME_OVER)
    {
        if(msg.keys & KEY_ENTER)
            ui_games_beginPlay();

        return true;
    }

    if(msg.keys & KEY_ENTER)
    {
        if(ui_games_runtime.phase == UI_GAME_PHASE_PAUSED)
        {
            ui_games_runtime.phase = UI_GAME_PHASE_PLAYING;
            ui_games_runtime.next_tick = getTick() + ui_games_runtime.active_game->tick_period_ms;
        }
        else
        {
            ui_games_runtime.phase = UI_GAME_PHASE_PAUSED;
        }

        return true;
    }

    if(ui_games_runtime.phase == UI_GAME_PHASE_PLAYING)
        ui_games_runtime.active_game->handleInput(msg);

    return true;
}

bool ui_games_handleRunningStatusEvent(void)
{
    if((ui_games_runtime.active_game == NULL) ||
       (ui_games_runtime.phase != UI_GAME_PHASE_PLAYING))
    {
        return false;
    }

    uint32_t now = getTick();

    while((int32_t)(now - ui_games_runtime.next_tick) >= 0)
    {
        ui_games_runtime.active_game->tick(&ui_games_runtime.layout);
        ui_games_runtime.next_tick += ui_games_runtime.active_game->tick_period_ms;

        if(ui_games_runtime.active_game->isGameOver())
        {
            ui_games_runtime.phase = UI_GAME_PHASE_GAME_OVER;
            break;
        }
    }

    return true;
}
