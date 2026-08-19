/* events.h - the X event dispatcher
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_EVENTS_H
#define FLOWM_EVENTS_H

#include <X11/Xlib.h>

#include "wm.h"

void events_dispatch(WM *wm, XEvent *ev);

#endif
