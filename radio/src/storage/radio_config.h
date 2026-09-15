/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// New target-independent, bounded-memory radio.yml storage.
//
// Unlike the legacy storage/yaml/yaml_datastructs_* + YamlTreeWalker path
// (still used for models), this module treats radio.yml as a configuration
// document: it never depends on RadioData layout, bit offsets or sizeof().
// Every known field is described once, in a single target-independent
// table (see radio_config.cpp), with a capability predicate that decides
// whether the field is relevant to the currently running target/hardware.
//
// Load/save both stream the file with fixed-size line buffers (no DOM,
// no per-field heap allocation) so the working RAM stays small and bounded
// regardless of file size.

#pragma once

#include <stdint.h>

// Bumped only for semantic migrations that the known/unknown/missing rules
// below cannot express on their own (renamed keys, changed units/ranges,
// structural changes). Simple additions/removals of fields do NOT need a
// version bump.
constexpr uint32_t RADIO_CONFIG_SCHEMA_VERSION = 1;

struct RadioConfigLoadResult {
  // true if radio.yml did not exist (first run / pre-migration radio)
  bool fileMissing = false;
  // number of known+relevant fields that were absent from the file and
  // therefore initialised from the existing runtime default
  uint16_t missingFields = 0;
  // number of known fields whose value was out of range/invalid and was
  // therefore left at its runtime default instead of being applied
  uint16_t invalidFields = 0;
  // configuration should be saved because defaults/missing fields were
  // written into the runtime state
  bool dirty = false;
};

// Loads radio.yml (if present) over the already-defaulted runtime radio
// settings (g_eeGeneral). Must be called after the normal default
// initialisation path (generalDefault()/adcCalibDefaults()/etc.) since
// missing fields simply keep whatever is already in memory.
RadioConfigLoadResult radioConfigLoad(const char* path);

// Streams a merge of the existing radio.yml (if any) with the current
// runtime values into <path>.tmp, then atomically replaces <path>.
// Returns nullptr on success, or a static error string on failure. The
// existing radio.yml is left completely untouched on any failure.
const char* radioConfigSave(const char* path, const char* tmpPath);
