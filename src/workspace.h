/* workspace.h - virtual desktop handling
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_WORKSPACE_H
#define FLOWM_WORKSPACE_H

#include "wm.h"

void workspace_init(WM *wm);

void workspace_switch(WM *wm, int ws);

void workspace_next(WM *wm);
void workspace_prev(WM *wm);

int workspace_client_count(const WM *wm, int ws);

bool workspace_has_urgent(const WM *wm, int ws);

#endif
