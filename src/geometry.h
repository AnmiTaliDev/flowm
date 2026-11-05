/* SPDX-License-Identifier: MIT */

#ifndef FLOWM_GEOMETRY_H
#define FLOWM_GEOMETRY_H

typedef enum ClientSnap {
    SNAP_NONE = 0,
    SNAP_LEFT,
    SNAP_RIGHT
} ClientSnap;

void geometry_snap_rect(ClientSnap snap, int ux, int uy, int uw, int uh,
                        int th, int bw, int *x, int *y, int *w, int *h);

void geometry_snap_edges(int distance, int ux, int uy, int uw, int uh,
                         int th, int bw, int cw, int ch, int *x, int *y);

#endif
