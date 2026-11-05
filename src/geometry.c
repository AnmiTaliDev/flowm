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
    { "snap_top",          SNAP_TOP          },
    { "snap_bottom",       SNAP_BOTTOM       },
    { "snap_top_left",     SNAP_TOP_LEFT     },
    { "snap_top_right",    SNAP_TOP_RIGHT    },
    { "snap_bottom_left",  SNAP_BOTTOM_LEFT  },
    { "snap_bottom_right", SNAP_BOTTOM_RIGHT },
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
    int row0 = 0, row1 = uh;

    switch (snap) {
    case SNAP_LEFT:
    case SNAP_TOP_LEFT:
    case SNAP_BOTTOM_LEFT:
        col1 = uw / 2;
        break;
    case SNAP_RIGHT:
    case SNAP_TOP_RIGHT:
    case SNAP_BOTTOM_RIGHT:
        col0 = uw / 2;
        break;
    default:
        break;
    }

    switch (snap) {
    case SNAP_TOP:
    case SNAP_TOP_LEFT:
    case SNAP_TOP_RIGHT:
        row1 = uh / 2;
        break;
    case SNAP_BOTTOM:
    case SNAP_BOTTOM_LEFT:
    case SNAP_BOTTOM_RIGHT:
        row0 = uh / 2;
        break;
    default:
        break;
    }

    *x = ux + col0 + bw;
    *y = uy + row0 + th + bw;
    *w = MAX(1, (col1 - col0) - 2 * bw);
    *h = MAX(1, (row1 - row0) - th - 2 * bw);
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
