# OpenRTX Project Map

## Purpose

This document is a practical map of the firmware, with emphasis on where
 features live, how data flows through the system, and which areas are the best
 extension points for future work.

## Top-Level Layout

```text
openrtx/      Core firmware logic, UI, protocol code, shared state
platform/     Hardware-specific implementations, targets, MCU support, drivers
tests/        Unit tests and hardware/platform tests
scripts/      Build and utility scripts
subprojects/  Third-party code used during build or runtime support
lib/          Bundled libraries and RTOS code
docs/         Working design and roadmap notes
```

## Runtime Architecture

The firmware is organized in layers that are mostly cleanly separated.

1. UI and input update shared application state.
2. Core threads translate state into radio configuration.
3. `rtx` selects the active operating mode handler.
4. Mode handlers drive the hardware radio interface.
5. Platform drivers apply low-level hardware changes.

The most important flow to keep in mind is:

```text
UI/input -> state -> threads -> rtxStatus -> OpMode -> radio driver
```

## Main Subsystems

### Core state and orchestration

- `openrtx/src/core/state.c`
  - boot-time initialization
  - loading persistent settings and VFO/channel defaults
  - periodic device-state refresh
- `openrtx/include/core/state.h`
  - global runtime state model
  - tuner modes include `VFO`, `CH`, `SCAN`, and `CHSCAN`
  - includes backup/restore flags that are not fully wired end-to-end yet
- `openrtx/src/core/threads.c`
  - maps UI state into `rtxStatus_t`
  - pushes updated radio configuration into the `rtx` layer

### Radio control layer

- `openrtx/src/rtx/rtx.cpp`
  - central opmode dispatcher
  - selects between `OPMODE_NONE`, `OPMODE_FM`, and `OPMODE_M17`
  - note: `OPMODE_DMR` exists in the data model but does not have an active
    runtime handler here
- `openrtx/include/rtx/rtx.h`
  - common radio configuration/status structure
  - includes mode, frequencies, power, tones, M17 routing metadata, and a scan
    flag

### Protocols

- `openrtx/src/rtx/OpMode_FM.cpp`
  - analog FM operating behavior
- `openrtx/src/rtx/OpMode_M17.cpp`
  - M17 operating behavior including callsign/meta text integration
- `openrtx/src/protocols/M17/*`
  - M17 framing, DSP, modulation, demodulation, callsign/meta text helpers

### User interfaces

- Default handheld UI:
  - `openrtx/src/ui/default/ui.c`
  - `openrtx/src/ui/default/ui_main.c`
  - `openrtx/src/ui/default/ui_menu.c`
  - `openrtx/src/ui/default/ui_strings.c`
- Module17 UI:
  - `openrtx/src/ui/module17/ui.c`
  - `openrtx/src/ui/module17/ui_main.c`
  - `openrtx/src/ui/module17/ui_menu.c`

The default UI is the main product UI. Module17 contains useful calibration and
service-oriented ideas that can inform future work on the default UI.

### Settings, codeplug, and persistence

- `openrtx/include/core/settings.h`
  - global persistent settings such as brightness, squelch, voice prompts,
    GPS time sync, M17 defaults, and PPM correction
- `openrtx/include/core/cps.h`
  - codeplug model for channels, contacts, FM, DMR, and M17 metadata
  - includes latent scan-list support and partially exposed DMR structures
- `openrtx/src/core/cps.c`
  - CPS helper logic
- `platform/drivers/CPS/*`
  - target-specific import/export and storage backends

### Accessibility and audio

- `openrtx/src/core/voicePrompts.c`
- `openrtx/src/core/voicePromptUtils.c`
- `openrtx/src/core/audio_stream.c`
- `openrtx/src/core/audio_codec.c`
- `openrtx/src/core/audio_path.cpp`

This is one of the strongest parts of the product. Voice prompts are deep enough
 that any UI redesign should preserve and expand them rather than treat them as
 an add-on.

### Backup and restore

- `openrtx/src/core/backup.c`
  - includes `eflash_dump()` and `eflash_restore()` implementations via XMODEM
- `openrtx/src/ui/default/ui_menu.c`
  - already exposes Flash Backup and Flash Restore user flows

This feature exists in two halves today: UI trigger and backend data transfer.
The missing piece is the orchestration path that consumes the state flags and
executes the transfer reliably.

## Build Targets and Platform Families

The target composition is visible in `meson.build`.

### Core build groups

- `openrtx_src` - platform-independent core firmware
- `ui_src_default` - standard handheld UI
- `ui_src_module17` - Module17-specific UI

### Supported families and targets

- Linux emulator target for desktop iteration and testing
- TYT MD-3x0 family
- TYT MD-UV3x0 family
- TYT MD-9600
- GD77 / DM-1801 / DM-1701 family
- CS7000 variants
- Module17

### Toolchain notes

- `cross_cm4.txt` and `cross_cm7.txt` are the current cross files for ARM
  targets.
- Local helper scripts should prefer `build_cm4` / `cross_cm4.txt` rather than
  legacy `build_arm` / `cross_arm.txt` naming.

## Subprojects and External Dependencies

The build references these major external components:

- `subprojects/codec2`
  - speech codec dependency used by digital voice paths
- `subprojects/XPowersLib`
  - power-management support for targets that use X-Powers PMUs
- `subprojects/radio_tool`
  - external helper program used by build/packaging flows
- `lib/miosix-kernel`
  - RTOS/runtime foundation on embedded targets
- `lib/minmea`
  - GPS parsing
- `lib/QRCode`
  - QR generation support

When working on new features, first decide if the work belongs in OpenRTX core,
 platform code, or one of these external integrations. Avoid leaking target-
 specific behavior into the shared core unless it is guarded cleanly.

## Test Surface

- `tests/unit/*`
  - includes M17 protocol unit tests, CPS tests, standby UI tests, and Linux
    input-stream tests
- Linux emulator support in `platform/targets/linux/*`

Current testing is good for protocol and targeted core logic, but there is room
 for more UI integration and state-machine regression coverage.

## Product and Architecture Gaps

These are the biggest visible gaps in the current codebase.

- Scan support is modeled but not implemented as a real user-facing subsystem.
- On-radio memory editing is limited compared with browse/navigation.
- DMR is represented in the data model but not in runtime radio operation.
- VOX is stored as a setting but appears unused in live behavior.
- Backup/restore is not fully wired through the runtime state machine.
- Power management is functional but not deeply optimized.
- Default UI lacks a service/calibration surface comparable to the opportunity
  visible in Module17-oriented code.

## Safe Extension Guidelines

- Put protocol-neutral behavior in `openrtx/src/core/*`.
- Put mode-specific runtime behavior in `openrtx/src/rtx/*`.
- Put wire/protocol details in `openrtx/src/protocols/*`.
- Put target-dependent behavior in `platform/*`.
- Put user workflows in the relevant UI implementation, but keep persistence and
  device-side logic out of the UI wherever possible.
- When adding a new setting, wire all four layers deliberately:
  1. persistent model
  2. UI editing flow
  3. runtime use
  4. verification path
