/* client.h - managed client lifecycle, frames, focus and geometry
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_CLIENT_H
#define FLOWM_CLIENT_H

#include <stdbool.h>
#include <X11/Xlib.h>

#include "wm.h"

int client_titlebar_height(const WM *wm, const Client *c);

int client_close_button_width(const WM *wm);

Client *client_from_window(WM *wm, Window win);

Client *client_from_frame(WM *wm, Window frame);

Client *client_manage(WM *wm, Window win, bool existing);

void client_unmanage(WM *wm, Client *c, bool destroyed);

void client_focus(WM *wm, Client *c);

void client_focus_cycle(WM *wm, bool forward);

void client_move_resize(WM *wm, Client *c, int x, int y, int w, int h,
                        bool honor_hints);

void client_apply_size_hints(const Client *c, int *w, int *h);

void client_update_size_hints(WM *wm, Client *c);

void client_update_wm_hints(WM *wm, Client *c);

void client_set_urgent(WM *wm, Client *c, bool urgent);

void client_update_title(WM *wm, Client *c);

void client_set_fullscreen(WM *wm, Client *c, bool fullscreen);

void client_toggle_maximize(WM *wm, Client *c);

void client_center(WM *wm, Client *c);

void client_close(WM *wm, Client *c);

void client_kill(WM *wm, Client *c);

void client_raise(WM *wm, Client *c);
void client_lower(WM *wm, Client *c);

void client_show(WM *wm, Client *c);
void client_hide(WM *wm, Client *c);

void client_send_to_workspace(WM *wm, Client *c, int ws);

void client_draw_frame(WM *wm, Client *c);

void client_snap_position(const WM *wm, const Client *c, int *x, int *y);

void client_update_decor_colors(WM *wm, Client *c);

#endif
