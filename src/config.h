/* config.h - configuration loading for flowm
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_CONFIG_H
#define FLOWM_CONFIG_H

#include <stdbool.h>

#include "wm.h"

void config_defaults(Config *cfg);

bool config_load(Config *cfg, const char *explicit_path);

void config_free(Config *cfg);

bool config_apply_option(Config *cfg, const char *key, const char *value);

bool config_parse_chord(const char *chord, unsigned int *mods_out,
                        unsigned long *keysym_out);

bool config_parse_action(const char *spec, Binding *b);

#endif
