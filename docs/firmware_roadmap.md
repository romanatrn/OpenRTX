# Firmware Roadmap

## Goal

This roadmap prioritizes the next meaningful product steps for OpenRTX from the
 perspective of a mature field radio firmware: operator usefulness first,
 architectural fit second, and implementation risk third.

## Prioritization Summary

### Tier 1: high impact, good architectural fit

1. Real scan subsystem
2. On-radio memory management
3. FM operating polish
4. End-to-end backup/restore

### Tier 2: strong product value, moderate complexity

5. M17 channel workflow maturity
6. Service and calibration tools
7. Power-management improvements

### Tier 3: major program of work

8. DMR runtime implementation

## 1. Real Scan Subsystem

### Why it matters

This is the most obvious missing everyday radio feature. The data model already
 contains `SCAN`, `CHSCAN`, a scan flag in `rtxStatus_t`, and per-channel
 `scanList_index`, which strongly suggests the original architecture expected
 scan support.

### Existing hooks

- `openrtx/include/core/state.h`
- `openrtx/include/rtx/rtx.h`
- `openrtx/include/core/cps.h`

### Feature scope

- VFO frequency scan
- channel scan
- scan list filtering
- carrier-operated and time-operated resume modes
- nuisance delete / temporary skip
- optional priority channel watch
- voice prompt and beep feedback for state changes

### Recommended implementation shape

- Add a scan controller in core, not inside the UI.
- UI should start/stop/configure scan, but the scan engine should own timing,
  stepping, lockout, and resume policy.
- `rtx` should expose enough RX state to make scan decisions without pushing too
  much timing logic into hardware drivers.

### Risks

- false-open behavior due to RSSI settling
- tone-squelch interactions on FM
- user confusion if scan state is not clearly rendered and announced

### Suggested milestones

1. VFO scan with simple resume
2. Channel scan over all memories
3. Scan-list aware channel scan
4. Nuisance delete and priority watch

## 2. On-Radio Memory Management

### Why it matters

Field programmability is one of the biggest practical quality-of-life gaps in a
 handheld. Operators should be able to save a found frequency, tweak a repeater,
 and store it without leaving the radio.

### Feature scope

- Save VFO to new channel
- overwrite existing channel from VFO
- edit channel name
- edit RX/TX frequencies and shift model
- edit mode-specific fields like FM tones and M17 routing basics
- assign bank and scan list
- copy memory to VFO and VFO to memory explicitly

### Existing hooks

- `openrtx/src/ui/default/ui.c`
- `openrtx/src/ui/default/ui_menu.c`
- `openrtx/include/core/cps.h`
- `openrtx/src/core/cps.c`

### Recommended implementation shape

- Add a dedicated edit session structure rather than mutating the live channel
  one field at a time.
- Keep channel validation centralized so both the UI and future import/export
  paths use the same rules.
- Separate browse mode from edit mode more clearly than the current menu logic.

### Risks

- accidental overwrite without confirmation
- inconsistent persistence across targets
- hidden coupling between CPS layout and UI assumptions

## 3. FM Operating Polish

### Why it matters

For many users, analog FM quality and completeness define whether the firmware
 feels ready. These features are smaller than DMR or scan, but collectively they
 improve daily usability more than many larger projects.

### Best next additions

- DCS encode/decode
- split RX/TX tone handling where not already covered by current abstractions
- repeater reverse / monitor
- TOT
- BCLO
- open squelch monitor action
- real VOX behavior using `voxLevel`

### Existing hooks

- `openrtx/include/core/settings.h`
- `openrtx/src/rtx/OpMode_FM.cpp`
- `openrtx/src/ui/default/ui.c`
- `openrtx/include/core/cps.h`

### VOX note

`voxLevel` exists in persistent settings, but runtime behavior does not appear
 wired. This is a good example of a feature that can likely be completed with
 limited UI cost if the audio path and PTT gating are approached carefully.

## 4. End-to-End Backup and Restore

### Why it matters

This improves user trust immediately. Once users start relying on memory editing,
 scan lists, and personalized settings, safe backup and restore become critical.

### Existing hooks

- UI trigger in `openrtx/src/ui/default/ui_menu.c`
- backend transport in `openrtx/src/core/backup.c`
- state flags in `openrtx/include/core/state.h`

### Missing glue

- runtime orchestration in the device state machine
- progress UX
- transport/error reporting
- safe restore sequencing and post-restore restart behavior

### Recommended implementation shape

- Handle backup/restore at the device-state level, not in menu rendering code.
- Freeze or narrow the UI during transfer.
- Add explicit status, completion, and failure screens plus voice prompt hooks.

## 5. M17 Channel Workflow Maturity

### Why it matters

M17 is already one of OpenRTX's strongest differentiators. The next step is not
 just deeper protocol work; it is making M17 channels behave like polished radio
 memories rather than loosely coupled settings.

### Candidate work

- per-channel destination policy
- recent/last-heard destination recall
- per-channel CAN handling defaults
- per-channel GPS metadata mode
- improved RX metadata display and call history

### Existing hooks

- `openrtx/include/core/cps.h`
- `openrtx/src/rtx/OpMode_M17.cpp`
- `openrtx/src/ui/default/ui_main.c`
- `openrtx/src/ui/default/ui.c`

## 6. Service and Calibration Tools

### Why it matters

This reduces friction for bring-up, testing, target ports, and repair work.
 It is also the kind of feature that makes the firmware feel serious to advanced
 users and maintainers.

### Candidate tools

- TX power alignment by band
- battery ADC diagnostics
- RSSI diagnostics and sanity view
- PPM trim validation and live readout
- mic gain and audio-path checks
- PA/LNA/receiver state exercise tools

### Existing hooks

- `openrtx/src/ui/module17/ui.c`
- `openrtx/include/calibration/*`
- target radio drivers under `platform/drivers/baseband/*`

## 7. Power-Management Improvements

### Why it matters

Battery life and clean sleep/wake behavior are product features, not only low-
 level engineering concerns.

### Candidate work

- flash deep sleep discipline
- GPS duty cycling
- smarter audio-path power-down
- more aggressive idle state management
- autosave on controlled shutdown or low-power events

### Existing hooks

- `openrtx/src/ui/default/ui.c`
- `openrtx/src/core/state.c`
- `platform/drivers/NVM/W25Qx.c`
- PMU-related target code where present

## 8. DMR Runtime Implementation

### Why it matters

DMR is important strategically, but it is a large undertaking and should not be
 confused with the smaller, high-return work above.

### Current state

- DMR data structures are present in `openrtx/include/core/cps.h`.
- `OPMODE_DMR` exists in `openrtx/include/rtx/rtx.h`.
- `openrtx/src/rtx/rtx.cpp` does not instantiate a DMR handler.

### Recommendation

- Treat DMR as a focused multi-phase program with explicit architecture review,
  test strategy, and target-platform constraints before coding deeply.

## Recommended Execution Order

### Product-first path

1. Scan subsystem
2. On-radio memory management
3. FM polish
4. Backup/restore completion
5. M17 workflow maturity

### Engineering-first path

1. Service/calibration tools
2. Scan subsystem
3. Memory management
4. Backup/restore
5. Power management

## Branching Advice

- Keep each roadmap item in its own branch.
- Split large items into vertical slices that are independently testable.
- Do not combine UI redesign with backend features unless the backend depends on
  the redesign structure.
- Document feature state transitions and persistence rules before coding.

## Suggested Immediate Next Branches

- `feature/scan-engine-vfo`
- `feature/memory-save-from-vfo`
- `feature/fm-vox-and-monitor`
- `feature/flash-backup-runtime`
- `feature/ui-redesign-foundation`
