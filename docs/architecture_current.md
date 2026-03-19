# Current Architecture

## Purpose

This document describes how the firmware works today, with the current set of
 responsibilities, runtime flows, and code ownership boundaries. It is intended
 to help contributors reason about changes without first reverse-engineering the
 system from source.

## Architecture Overview

OpenRTX follows a layered model that is cleaner than most embedded radio
 firmware projects:

```text
Input devices
  -> UI state machine
  -> global state model
  -> thread orchestration
  -> rtx configuration/status layer
  -> opmode handler
  -> platform radio driver
  -> hardware peripherals
```

This layering is visible most clearly in:

- `openrtx/src/core/threads.c`
- `openrtx/src/rtx/rtx.cpp`
- `platform/drivers/baseband/*`

## Thread Model

### UI thread

Defined in `openrtx/src/core/threads.c:35`.

Responsibilities:

- read keyboard/input events
- feed the UI FSM
- save a local state snapshot for rendering
- push updated radio configuration into the `rtx` layer
- render UI and advance voice prompt playback

Important detail:

- This thread is where persistent UI state and runtime radio config meet.
- It currently applies the user PPM correction to RX and TX frequencies before
  passing them into `rtxStatus_t`.

### Main/device thread

Defined in `openrtx/src/core/threads.c:122`.

Responsibilities:

- handle platform power button events
- run GPS tasks when present
- periodically refresh global state through `state_task()`

This is the main device supervision loop rather than the UI or protocol loop.

### RTX thread

Defined in `openrtx/src/core/threads.c:167`.

Responsibilities:

- initialize the radio-control subsystem
- run `rtx_task()` while the device is in `RUNNING`
- terminate the active opmode and radio driver on exit

This thread isolates radio runtime behavior from UI timing and state mutation.

## State Model

### Global runtime state

Defined in `openrtx/include/core/state.h`.

Key fields:

- device state: `devStatus`
- currently displayed screen: `ui_screen`
- tuner mode: `VFO`, `CH`, `SCAN`, `CHSCAN`
- current channel and VFO channel
- bank selection state
- persistent settings
- GPS state
- backup/restore flags
- TX disable and tuning step

Observations:

- The state model now participates in scan control and flash transfer orchestration.
- Some modeled features are still only partially surfaced in UI workflows.

### Persistent settings

Defined in `openrtx/include/core/settings.h`.

Current notable stored settings include:

- display brightness/contrast/timer
- squelch and VOX level
- callsign and M17 defaults
- battery icon preference
- GPS time sync behavior
- accessibility and voice prompt options
- M17 meta text
- radio PPM correction

### Codeplug and channel model

Defined in `openrtx/include/core/cps.h`.

Current modeled domains:

- FM channel data and CTCSS support
- DMR channel and contact structures
- M17 channel and contact structures
- common channel fields such as frequencies, power, name, description,
  scan-list index, and group list index

Important architectural note:

- DMR is represented structurally, but the runtime radio layer currently only
  activates FM and M17 opmodes.

## UI Architecture

## Default UI

Main files:

- `openrtx/src/ui/default/ui.c`
- `openrtx/src/ui/default/ui_main.c`
- `openrtx/src/ui/default/ui_menu.c`
- `openrtx/src/ui/default/ui_strings.c`

Responsibilities are roughly split as follows:

- `ui.c`
  - FSM, event handling, edit logic, input widgets, screen transitions
- `ui_main.c`
  - main screen rendering, mode display, status rendering
- `ui_menu.c`
  - menu rendering, entry/value formatting, special settings screens
- `ui_strings.c`
  - language table registration and lookup

### Current behavior pattern

- The UI stores temporary edit values in `ui_state_t`.
- Most edits are staged in UI state and copied into `state` on confirm.
- Some operations are immediate toggles, others are modal edit sessions.

This is workable, but one of the redesign opportunities is to make these edit
 patterns more uniform.

## Module17 UI

Main files:

- `openrtx/src/ui/module17/ui.c`
- `openrtx/src/ui/module17/ui_main.c`
- `openrtx/src/ui/module17/ui_menu.c`

This UI has more board-specific and calibration-oriented flavor. It is a useful
 reference when designing service and engineering workflows for the default UI.

## Radio Control Architecture

### `rtxStatus_t`

Defined in `openrtx/include/rtx/rtx.h`.

This structure is the contract between the UI/core state world and the runtime
 radio control world. It includes:

- mode and status
- RX/TX frequencies
- power and squelch
- FM tone information
- scan bit
- M17 addressing and decoded metadata

### `rtx.cpp`

`openrtx/src/rtx/rtx.cpp` performs three key functions:

- copies the latest configuration from the UI/core side
- switches active opmode handler when mode changes
- forwards periodic updates to the selected handler

Current opmode support:

- `OPMODE_NONE`
- `OPMODE_FM`
- `OPMODE_M17`

Not currently active despite enum support:

- `OPMODE_DMR`

### OpMode handlers

- `openrtx/src/rtx/OpMode_FM.cpp`
- `openrtx/src/rtx/OpMode_M17.cpp`

These are the right places for runtime mode-specific behavior such as squelch,
 TX/RX lifecycle handling, audio routing policy, and protocol-specific radio
 actions.

## Protocol Architecture

M17 is the most fully realized protocol area.

Important files:

- `openrtx/src/protocols/M17/M17DSP.cpp`
- `openrtx/src/protocols/M17/M17Modulator.cpp`
- `openrtx/src/protocols/M17/M17Demodulator.cpp`
- `openrtx/src/protocols/M17/M17FrameEncoder.cpp`
- `openrtx/src/protocols/M17/M17FrameDecoder.cpp`
- `openrtx/src/protocols/M17/MetaText.cpp`
- `openrtx/src/protocols/M17/Callsign.cpp`

Architecturally, this area is already decomposed well enough to support more
 testing and feature work without rewriting the whole stack.

## Platform Architecture

Platform code is divided by concern:

- `platform/targets/*` for model-specific mappings and top-level target config
- `platform/drivers/*` for reusable driver implementations by hardware type
- `platform/mcu/*` for MCU-family support

This is one of the most important boundaries in the codebase. Shared product
 behavior should not drift into target-specific code unless it is truly hardware
 dependent.

## Build Architecture

The build is driven by `meson.build`.

Core structure:

- common platform-independent sources in `openrtx_src`
- separate UI source groups for default and Module17 UIs
- family- and MCU-specific source groups
- final target definitions that compose core + UI + platform + toolchain

Important current build facts:

- Linux emulator targets exist for fast local testing
- embedded Cortex-M4 and Cortex-M7 cross builds are defined
- current cross files are `cross_cm4.txt` and `cross_cm7.txt`
- current common UV3x0 local build convention uses `build_cm4`

## Test Architecture

Current automated tests live mostly in `tests/unit/*`.

Strong existing coverage areas:

- M17 protocol helpers and DSP components
- CPS model behavior
- some UI standby logic
- Linux input stream behavior

The next architectural test opportunity is cross-layer integration:

- UI FSM behavior under scripted inputs
- scan behavior once implemented
- memory-edit persistence flows
- backup/restore orchestration

## Current Architectural Strengths

- strong separation between UI/core/platform concerns
- unusually solid voice prompt and accessibility infrastructure
- reusable protocol layering around M17
- Linux target for fast non-device iteration
- code model already includes future-facing concepts like scan lists and DMR

## Current Architectural Friction Points

- some latent features still exist in the model without full product polish
- UI state machine carries a lot of feature-specific complexity
- backup/restore now has runtime orchestration, but still needs stronger UX and error handling
- DMR has structural presence without runtime ownership
- some settings still need stronger UI and validation, despite new runtime hooks such as VOX

## Recommended Use Of This Document

- Read this before large feature work.
- Use it to decide the right layer for a change.
- Update it when a dormant modeled feature becomes fully operational.
