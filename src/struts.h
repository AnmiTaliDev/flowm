/* SPDX-License-Identifier: MIT */

#ifndef FLOWM_STRUTS_H
#define FLOWM_STRUTS_H

#include <X11/Xlib.h>

#include "wm.h"

void struts_add(WM *wm, Window win);

void struts_remove(WM *wm, Window win);

void struts_refresh(WM *wm, Window win);

bool struts_tracked(const WM *wm, Window win);

void struts_reserved(const WM *wm, int *left, int *right,
                     int *top, int *bottom);

#endif
