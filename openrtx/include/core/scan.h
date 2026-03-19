/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SCAN_H
#define SCAN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void scan_reset();
void scan_notifyModeChange();
void scan_task();
bool scan_isActive();

#ifdef __cplusplus
}
#endif

#endif /* SCAN_H */
