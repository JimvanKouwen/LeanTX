# Lean Telemetry

Lua telemetry package with **separate monochrome and color renderers and entry
points**, sharing discovery, sampling, editing, and versioned configuration.
No Lua LVGL API dependency. Requires this repository's `getSourceValue` API.

## Install

Copy the contents of the repository's `sdcard/` directory onto the radio SD card.
Keep the empty `SCRIPTS/LeanTel/config/` directory: Lua does not create it.

- **Monochrome (128x64 / 212x64):** select `LeanTel` as a Script telemetry screen
  in the model's telemetry screen configuration, then open that screen.
- **Color (320x240 / 480x272 / 800x480):** add the `LeanTel` widget to a home-screen
  zone. The zone shows a compact read-only preview. Open the widget full-screen
  for the five views and editing. Use one LeanTel widget per model.

The native telemetry screens remain available. This is an SD-card package;
no firmware rebuild or flash is required if the installed firmware exposes the
APIs used here. Copy this directory explicitly when assembling an SD image;
this repository does not currently have an SD-package build target.

## Views and controls

- **Dashboard:** four or six selectable source cells.
- **Glance:** one large source and three smaller values.
- **Status:** battery/link/current/consumption slots with source labels and stale
  marking. These use the same first four assignments as Dashboard. It does not
  infer battery percentages or declare a link safe from universal thresholds.
- **Inspector:** scroll discovered sensors plus timers and transmitter voltage;
  ENTER toggles details, including firmware min/max when available.
- **Trends:** first two assigned sources, up to 60 one-second samples. Missing or
  stale samples produce gaps. History lives in RAM and covers app execution,
  not a guaranteed flight log; invisible-script scheduling is firmware-owned.

PAGE changes view. Encoder changes view, or scrolls the Inspector. ENTER (or long
ENTER) opens editing; encoder selects a slot or layout; ENTER opens the source
picker. Choose a source, then EXIT to save. EXIT in the picker returns without
changing the source. Failed saves remain in editing; EXIT retries.

Color touch footer: left/right changes pages (or moves selection when editing),
center opens editing/saves/goes back. Tap an edit or picker row to choose it.
Encoder controls remain available. Inspector details currently use ENTER.

The source picker shows current sources first, followed by unavailable sources.
`--` means missing, `~` means stale. Numeric zero is a valid reading. Duplicate
sensor names are shown but not read, because firmware name lookup is ambiguous;
rename those sensors in the model. Structured values (GPS/cell arrays) are not
graphed; GPS coordinates are displayed where space allows and other tables are
marked `Structured`. Narrow monochrome values may end in `>`; Inspector provides
more space. Units not in the formatter's mapping are left unlabeled.

## Configuration

Settings are data-only text under `SCRIPTS/LeanTel/config/`. Model filenames are
encoded reversibly and profiles use separate `-mono` / `-color` names. Renaming a
model's display name preserves its settings; changing its underlying filename
creates a new profile. Source selections use sensor ID, instance, and name,
rather than discovery-order indexes. A sensor rename requires reassignment.

Two generation-numbered snapshots (`.a` / `.b`) alternate, with readback after
writing. Loading falls back to the previous complete snapshot if one is corrupt.
No executable config, writes per frame, or graph data on the SD card. Copies of a
model with different filenames start with defaults. Missing model identity or an
unwritable/missing config directory reports save failure.

## Validation and remaining integration

Run from the repository root: `lua tests/lua/telemetry_test.lua` (Lua 5.3+).
Mocks exercise both profiles and five dimensions, editing, touch, model switches,
zero/stale values, duplicate names, bounded history, failed saves, and recovery.
They do not validate actual fonts, firmware instruction budgets, heap use, or
physical controls. Before removing native screens, test those on the Pocket
simulator and a color simulator, then on the intended radios.

This initial package has fixed view pages and shared assignments within each
profile. Independent user-created pages, per-cell bar/graph modes, grouped
input/output source selection, threshold-based health interpretation, and an
inline cell editor are follow-up work. There are no mixer or RF-control changes.
