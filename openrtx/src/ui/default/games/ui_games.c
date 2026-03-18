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

typedef struct ui_games_runtime_t
{
    const ui_game_driver_t *active_game;
    ui_game_layout_t        layout;
    uint32_t                next_tick;
    bool                    paused;
} ui_games_runtime_t;

static const ui_game_driver_t *games_catalog[] =
{
    &ui_game_snake,
};

static ui_games_runtime_t ui_games_runtime;

static uint8_t ui_games_getCount(void)
{
    return sizeof(games_catalog) / sizeof(games_catalog[0]);
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
    uint8_t count = ui_games_getCount();
    uint8_t selected = ui_state->menu_selected;

    if(selected >= count)
        selected = 0;

    return games_catalog[selected];
}

static void ui_games_startSelection(const ui_state_t *ui_state)
{
    ui_games_runtime.active_game = ui_games_getSelectedGame(ui_state);
    ui_games_runtime.paused = false;
    ui_games_runtime.next_tick = getTick() + ui_games_runtime.active_game->tick_period_ms;
    ui_games_runtime.active_game->start(&ui_games_runtime.layout);
    state.ui_screen = GAME_RUN;
}

static void ui_games_drawLibraryItem(point_t *pos,
                                     const char *title,
                                     const char *subtitle,
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

#if CONFIG_SCREEN_HEIGHT > 64
    if(selected)
    {
        gfx_print(*pos, FONT_SIZE_6PT, TEXT_ALIGN_RIGHT, text_color, subtitle);
    }
#else
    (void) subtitle;
#endif

    pos->y += layout.menu_h;
}

static void ui_games_drawOverlay(const char *line1,
                                 const char *line2,
                                 const char *line3)
{
    point_t box = {layout.horizontal_pad + 2, CONFIG_SCREEN_HEIGHT / 2 - (layout.menu_h + 8)};
    uint16_t box_width = CONFIG_SCREEN_WIDTH - ((layout.horizontal_pad + 2) * 2);
    uint16_t box_height = (CONFIG_SCREEN_HEIGHT > 64) ? 42 : 30;
    point_t accent = {box.x + 2, box.y + 2};

    gfx_drawRect((point_t){box.x - 1, box.y - 1}, box_width + 2, box_height + 2, color_grey, false);
    gfx_drawRect(box,
                 box_width,
                 box_height,
                 color_black,
                 true);
    gfx_drawRect(box,
                 box_width,
                 box_height,
                  color_white,
                 false);

    gfx_drawRect(accent,
                 box_width - 4,
                 (CONFIG_SCREEN_HEIGHT > 64) ? 8 : 6,
                 yellow_fab413,
                 true);
    gfx_print((point_t){0, box.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 18 : 14)},
              FONT_SIZE_8PT,
              TEXT_ALIGN_CENTER,
              color_white,
              line1);

    if(line2 != NULL)
    {
        gfx_print((point_t){0, box.y + ((CONFIG_SCREEN_HEIGHT > 64) ? 28 : 21)},
                  FONT_SIZE_6PT,
                  TEXT_ALIGN_CENTER,
                  color_white,
                  line2);
    }

    if(line3 != NULL)
    {
        gfx_print((point_t){0, box.y + box_height - ((CONFIG_SCREEN_HEIGHT > 64) ? 8 : 6)},
                  FONT_SIZE_6PT,
                  TEXT_ALIGN_CENTER,
                  yellow_fab413,
                  line3);
    }
}

void ui_games_init(void)
{
    memset(&ui_games_runtime, 0, sizeof(ui_games_runtime));
    ui_games_updateLayout();
}

void ui_games_enterLibrary(ui_state_t *ui_state)
{
    ui_games_runtime.active_game = NULL;
    ui_games_runtime.paused = false;
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
                                 games_catalog[i]->subtitle,
                                 selected);
    }

}

void ui_games_drawRunning(void)
{
    char score_buf[24];
    char best_buf[24];
    point_t border =
    {
        ui_games_runtime.layout.board_origin.x - 1,
        ui_games_runtime.layout.board_origin.y - 1
    };

    gfx_fillScreen(color_black);

    if(ui_games_runtime.active_game == NULL)
    {
        gfx_print(layout.top_pos, layout.top_font, TEXT_ALIGN_CENTER, color_white, "Games");
        gfx_print(layout.line3_pos, layout.menu_font, TEXT_ALIGN_CENTER, color_white, "No game loaded");
        return;
    }

    snprintf(score_buf, sizeof(score_buf), "Score %u", ui_games_runtime.active_game->getScore());
    snprintf(best_buf, sizeof(best_buf), "Best %u", ui_games_runtime.active_game->getBestScore());

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

    if(ui_games_runtime.paused)
    {
        ui_games_drawOverlay("Paused", NULL, "Enter resume   Esc menu");
    }
    else if(ui_games_runtime.active_game->isGameOver())
    {
        ui_games_drawOverlay("Game Over", best_buf, "Enter retry   Esc menu");
    }
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
        ui_games_startSelection(ui_state);
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

    if(msg.keys & KEY_ENTER)
    {
        if(ui_games_runtime.active_game->isGameOver())
        {
            ui_games_runtime.active_game->start(&ui_games_runtime.layout);
            ui_games_runtime.paused = false;
            ui_games_runtime.next_tick = getTick() + ui_games_runtime.active_game->tick_period_ms;
        }
        else
        {
            ui_games_runtime.paused = !ui_games_runtime.paused;
            ui_games_runtime.next_tick = getTick() + ui_games_runtime.active_game->tick_period_ms;
        }

        return true;
    }

    if(!ui_games_runtime.paused && !ui_games_runtime.active_game->isGameOver())
    {
        ui_games_runtime.active_game->handleInput(msg);
    }

    return true;
}

bool ui_games_handleRunningStatusEvent(void)
{
    if((ui_games_runtime.active_game == NULL) ||
       ui_games_runtime.paused ||
       ui_games_runtime.active_game->isGameOver())
    {
        return false;
    }

    uint32_t now = getTick();

    while((int32_t)(now - ui_games_runtime.next_tick) >= 0)
    {
        ui_games_runtime.active_game->tick(&ui_games_runtime.layout);
        ui_games_runtime.next_tick += ui_games_runtime.active_game->tick_period_ms;

        if(ui_games_runtime.active_game->isGameOver())
            break;
    }

    return true;
}
