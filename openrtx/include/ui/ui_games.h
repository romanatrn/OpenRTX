/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_GAMES_H
#define UI_GAMES_H

#include <stdbool.h>
#include "core/input.h"
#include "ui/ui_default.h"

void ui_games_init(void);
void ui_games_enterLibrary(ui_state_t *ui_state);
void ui_games_drawLibrary(ui_state_t *ui_state);
void ui_games_drawRunning(void);
bool ui_games_handleLibraryEvent(ui_state_t *ui_state, kbd_msg_t msg);
bool ui_games_handleRunningKeyEvent(kbd_msg_t msg);
bool ui_games_handleRunningStatusEvent(void);

#endif /* UI_GAMES_H */
