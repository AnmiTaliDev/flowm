/* SPDX-License-Identifier: MIT */

#include "geometry.h"
#include "util.h"

void
geometry_snap_rect(ClientSnap snap, int ux, int uy, int uw, int uh,
                   int th, int bw, int *x, int *y, int *w, int *h)
{
    int col0 = 0, col1 = uw;

    if (snap == SNAP_LEFT)
        col1 = uw / 2;
    else if (snap == SNAP_RIGHT)
        col0 = uw / 2;

    *x = ux + col0 + bw;
    *y = uy + th + bw;
    *w = MAX(1, (col1 - col0) - 2 * bw);
    *h = MAX(1, uh - th - 2 * bw);
}
