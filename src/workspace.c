/* workspace.c - virtual desktop switching
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "workspace.h"
#include "client.h"
#include "ewmh.h"
#include "bar.h"
#include "log.h"
#include "util.h"

void
workspace_init(WM *wm)
{
    int i;

    for (i = 0; i < FLOWM_MAX_WORKSPACES; i++) {
        xstrlcpy(wm->workspaces[i].name,
                 wm->config.workspace_names[i],
                 sizeof(wm->workspaces[i].name));
        wm->workspaces[i].focused = NULL;
    }
    wm->current_ws = 0;
}

int
workspace_client_count(const WM *wm, int ws)
{
    const Client *c;
    int n = 0;

    for (c = wm->clients; c != NULL; c = c->next)
        if (c->workspace == ws)
            n++;
    return n;
}

bool
workspace_has_urgent(const WM *wm, int ws)
{
    const Client *c;

    for (c = wm->clients; c != NULL; c = c->next)
        if (c->workspace == ws && c->is_urgent)
            return true;
    return false;
}

void
workspace_switch(WM *wm, int ws)
{
    Client *c;
    Client *to_focus;

    if (ws < 0 || ws >= wm->config.workspace_count ||
        ws == wm->current_ws)
        return;

    log_debug("workspace: %d -> %d", wm->current_ws + 1, ws + 1);

    wm->workspaces[wm->current_ws].focused = wm->focused;

    wm->current_ws = ws;

    for (c = wm->clients; c != NULL; c = c->next)
        if (c->workspace == ws)
            client_show(wm, c);
    for (c = wm->clients; c != NULL; c = c->next)
        if (c->workspace != ws)
            client_hide(wm, c);

    to_focus = wm->workspaces[ws].focused;
    if (to_focus != NULL && to_focus->workspace != ws)
        to_focus = NULL;
    if (to_focus == NULL) {
        for (c = wm->clients; c != NULL; c = c->next) {
            if (c->workspace == ws && !c->never_focus) {
                to_focus = c;
                break;
            }
        }
    }

    wm->focused = NULL;
    client_focus(wm, to_focus);

    ewmh_update_desktops(wm);
    bar_draw(wm);
}

void
workspace_next(WM *wm)
{
    workspace_switch(wm, (wm->current_ws + 1)
                         % wm->config.workspace_count);
}

void
workspace_prev(WM *wm)
{
    workspace_switch(wm, (wm->current_ws - 1
                          + wm->config.workspace_count)
                         % wm->config.workspace_count);
}
