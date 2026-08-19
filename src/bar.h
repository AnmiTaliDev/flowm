/* bar.h - built-in status bar
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_BAR_H
#define FLOWM_BAR_H

#include "wm.h"

void bar_init(WM *wm);

void bar_shutdown(WM *wm);

void bar_draw(WM *wm);

bool bar_owns_window(const WM *wm, Window win);

void bar_handle_click(WM *wm, int x, int y);

void bar_tick(WM *wm);

#endif
