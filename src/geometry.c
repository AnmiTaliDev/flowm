/* SPDX-License-Identifier: MIT */

#include <stdlib.h>

#include "geometry.h"
#include "util.h"

typedef struct SnapName {
    const char *name;
    ClientSnap  snap;
} SnapName;

static const SnapName snap_names[] = {
    { "snap_left",         SNAP_LEFT         },
    { "tile_left",         SNAP_LEFT         },
    { "snap_right",        SNAP_RIGHT        },
    { "tile_right",        SNAP_RIGHT        },
};

bool
geometry_snap_from_name(const char *name, ClientSnap *out)
{
    size_t i;

    for (i = 0; i < LENGTH(snap_names); i++) {
        if (str_ieq(name, snap_names[i].name)) {
            *out = snap_names[i].snap;
            return true;
        }
    }
    return false;
}

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

void
geometry_snap_edges(int distance, int ux, int uy, int uw, int uh,
                    int th, int bw, int cw, int ch, int *x, int *y)
{
    int fw = cw + 2 * bw;
    int fh = ch + th + 2 * bw;

    if (distance <= 0)
        return;

    if (abs(*x - bw - ux) < distance)
        *x = ux + bw;
    if (abs((*x - bw) + fw - (ux + uw)) < distance)
        *x = ux + uw - fw + bw;
    if (abs((*y - th - bw) - uy) < distance)
        *y = uy + th + bw;
    if (abs((*y - th - bw) + fh - (uy + uh)) < distance)
        *y = uy + uh - fh + th + bw;
}
