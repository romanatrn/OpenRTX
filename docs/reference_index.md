# Reference Index

## Purpose

This document is a fast jump table into the most useful code, test, build, and
 project references for continuing work on OpenRTX.

## Core Runtime References

- Global state model: `openrtx/include/core/state.h`
- Persistent settings: `openrtx/include/core/settings.h`
- Codeplug model: `openrtx/include/core/cps.h`
- State initialization and persistence: `openrtx/src/core/state.c`
- Thread orchestration: `openrtx/src/core/threads.c`
- Radio control dispatch: `openrtx/src/rtx/rtx.cpp`
- FM runtime: `openrtx/src/rtx/OpMode_FM.cpp`
- M17 runtime: `openrtx/src/rtx/OpMode_M17.cpp`

## UI References

- Default UI FSM: `openrtx/src/ui/default/ui.c`
- Default UI main screen: `openrtx/src/ui/default/ui_main.c`
- Default UI menu rendering: `openrtx/src/ui/default/ui_menu.c`
- Default UI string registration: `openrtx/src/ui/default/ui_strings.c`
- Module17 UI FSM: `openrtx/src/ui/module17/ui.c`
- Common UI state definitions: `openrtx/include/ui/ui_default.h`
- Localized UI string structs: `openrtx/include/ui/ui_strings.h`

## Accessibility and Audio References

- Voice prompt engine: `openrtx/src/core/voicePrompts.c`
- Voice prompt helpers: `openrtx/src/core/voicePromptUtils.c`
- Voice prompt API: `openrtx/include/core/voicePrompts.h`
- Audio stream: `openrtx/src/core/audio_stream.c`
- Audio codec: `openrtx/src/core/audio_codec.c`
- Audio path routing: `openrtx/src/core/audio_path.cpp`

## Protocol References

- M17 DSP: `openrtx/src/protocols/M17/M17DSP.cpp`
- M17 modulator: `openrtx/src/protocols/M17/M17Modulator.cpp`
- M17 demodulator: `openrtx/src/protocols/M17/M17Demodulator.cpp`
- M17 frame encode/decode: `openrtx/src/protocols/M17/M17FrameEncoder.cpp`,
  `openrtx/src/protocols/M17/M17FrameDecoder.cpp`
- M17 metadata helpers: `openrtx/src/protocols/M17/MetaText.cpp`,
  `openrtx/src/protocols/M17/Callsign.cpp`

## Backup, Restore, and Data Transfer References

- Backup backend: `openrtx/src/core/backup.c`
- Backup API: `openrtx/include/core/backup.h`
- XMODEM transport: `openrtx/src/core/xmodem.c`
- Flash backup UI trigger: `openrtx/src/ui/default/ui_menu.c`

## Scan and Memory References

- Tuner modes including scan: `openrtx/include/core/state.h`
- Scan bit on runtime radio config: `openrtx/include/rtx/rtx.h`
- Channel scan-list field: `openrtx/include/core/cps.h`
- Main browse/edit UI logic: `openrtx/src/ui/default/ui.c`

## Build References

- Main build description: `meson.build`
- Cortex-M4 cross file: `cross_cm4.txt`
- Cortex-M7 cross file: `cross_cm7.txt`
- Local UV3x0 compile helper: `/home/romanat/scripts/uv3x0_compile.sh`

## Emulator and Test References

- Linux emulator target sources: `platform/targets/linux/emulator/`
- Linux platform integration: `platform/targets/linux/platform.c`
- CPS libc backend for host-side testing: `platform/drivers/CPS/cps_io_libc.c`
- Unit tests: `tests/unit/`

Useful existing tests:

- `tests/unit/cps.cpp`
- `tests/unit/ui_check_standby.cpp`
- `tests/unit/M17_demodulator.cpp`
- `tests/unit/M17_metatext.cpp`
- `tests/unit/M17_callsign.cpp`

## External Subprojects and Libraries

- Codec2: `subprojects/codec2`
- XPowersLib: `subprojects/XPowersLib`
- radio_tool: `subprojects/radio_tool`
- Miosix kernel: `lib/miosix-kernel`
- minmea: `lib/minmea`
- QRCode: `lib/QRCode`

## Current Product Gaps To Keep In Mind

- scan modeled, not fully implemented
- on-radio memory edit/save still limited
- DMR modeled, not runtime-enabled
- backup/restore not fully orchestrated
- VOX stored, not obviously active
- default UI ready for workflow redesign

## Recommended Docs Reading Order

1. `docs/project_map.md`
2. `docs/architecture_current.md`
3. `docs/firmware_roadmap.md`
4. `docs/ui_redesign_plan.md`

## External Project References

- Main project overview: `README.md`
- Contribution rules and code organization: `CONTRIBUTING.md`
- OpenRTX website: `https://openrtx.org/`
- Website user/developer docs are maintained in the adjacent site repo noted in
  `CONTRIBUTING.md`
