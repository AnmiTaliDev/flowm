/* keys.h - key grabbing and action dispatch
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_KEYS_H
#define FLOWM_KEYS_H

#include <X11/Xlib.h>

#include "wm.h"

void keys_grab(WM *wm);

void keys_ungrab(WM *wm);

void keys_handle_keypress(WM *wm, XKeyEvent *ev);

void keys_run_action(WM *wm, const Binding *b);

#endif
