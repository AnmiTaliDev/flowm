/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include "scratchpad.h"
#include "client.h"
#include "ewmh.h"
#include "bar.h"
#include "log.h"

static void
scratchpad_hide(WM *wm, Client *c)
{
    bool was_focused = wm->focused == c;

    if (c->is_sticky)
        client_set_sticky(wm, c, false);
    client_hide(wm, c);
    c->workspace = WORKSPACE_HIDDEN;

    if (was_focused) {
        wm->focused = NULL;
        client_focus(wm, client_most_recent(wm, c));
    }
    bar_draw(wm);
}

static void
scratchpad_show(WM *wm, Client *c)
{
    c->workspace = wm->current_ws;
    ewmh_set_client_desktop(wm, c);
    client_show(wm, c);
    client_raise(wm, c);
    client_focus(wm, c);
    bar_draw(wm);
}

void
scratchpad_toggle(WM *wm)
{
    Client *c = wm->scratch;

    if (c == NULL) {
        if (wm->focused == NULL)
            return;
        wm->scratch = wm->focused;
        log_debug("scratchpad: 0x%lx assigned", wm->scratch->win);
        scratchpad_hide(wm, wm->scratch);
        return;
    }

    if (c->workspace == wm->current_ws || c->is_sticky)
        scratchpad_hide(wm, c);
    else
        scratchpad_show(wm, c);
}
