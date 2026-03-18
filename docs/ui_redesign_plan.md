# UI Redesign Plan

## Goal

Modernize the default OpenRTX handheld UI so that it feels more intentional,
 faster to operate in the field, easier to learn, and more consistent across FM,
 M17, scan, memory, and accessibility workflows.

This is not a visual-only redesign. It is a workflow redesign.

## Design Principles

### 1. Radio-first interaction

The radio should favor actions operators perform while mobile or under time
 pressure:

- change channel or frequency quickly
- inspect signal state immediately
- access scan, monitor, and transmit-relevant actions with minimal steps
- make temporary changes without getting lost in menus

### 2. Clear separation between browse, quick action, and edit modes

The current UI has many behaviors folded into the same state machine. A redesign
 should make three user states obvious:

- browse: safe navigation
- quick action: short-lived operational commands
- edit: potentially persistent changes

### 3. Strong status density on the main screen

The main screen should answer these questions at a glance:

- what mode am I in
- what frequency or channel am I on
- what transmit frequency will be used
- what squelch/tone/scan state is active
- what special mode is armed: M17, GPS, battery, lock, bank, memory, etc

### 4. Accessibility is a first-class requirement

Voice prompts, beeps, phonetic spelling, and predictable navigation are already
 a competitive strength. The redesigned UI must preserve and deepen that.

### 5. Consistency across features

Every configurable item should follow the same pattern:

- view current value
- enter edit mode
- hear or see incremental change
- confirm or cancel
- return to the same place with clear feedback

## Current UI Strengths

- Strong voice prompt integration
- Reasonably complete radio settings menu
- Good M17-specific metadata support
- Multiple targets share a common default UI path

## Current UI Pain Points

- Too much functional depth is buried in menu traversal.
- Editing flows are inconsistent across numbers, toggles, and text.
- Main-screen affordances do not fully reflect latent features like scan.
- Browse and edit behaviors can feel coupled and stateful in hard-to-predict
  ways.
- There is not yet a clear quick-action layer for high-frequency operator tasks.

## Proposed Information Architecture

### Main screen layers

#### Primary line

- channel name or VFO label
- mode badge: FM, M17, future DMR
- memory/bank indicator

#### Primary tuning line

- RX frequency or channel number in dominant typography
- optional dual line for TX frequency or shift

#### Secondary status line

- bandwidth
- tone state
- scan state
- GPS state
- battery state
- lock state

#### Dynamic context strip

Shows situational information only when relevant:

- M17 destination or source metadata
- scan mode and resume state
- low battery or backup/restore progress
- active quick action hint

## Proposed Navigation Model

### Home actions

- knob/up/down: channel or frequency navigation
- enter: open quick action panel or context action
- menu: open full menu
- long-enter: mode-aware shortcut such as scan start/stop or save-to-memory
- esc/back: cancel active edit or close overlay

### Quick action panel

This is the key redesign element.

The quick action panel should expose the high-frequency tasks that are currently
 too buried:

- scan on/off
- monitor / reverse
- save VFO to memory
- change bandwidth
- tone quick edit
- M17 destination quick change
- lock keypad
- brightness / volume shortcut where hardware permits

This panel should be small, mode-aware, and fully voice-prompt friendly.

### Full menu groups

Recommended top-level groups:

- Radio
- Memories
- Scan
- M17 / Digital
- GPS
- Accessibility
- Device
- Service

This is clearer than mixing operational and device-management tasks in long flat
 menus.

## Feature-Specific UI Recommendations

### Scan UI

- main-screen scan badge
- quick action to start/stop scan
- scan submenu for mode, resume, priority, and temporary skip handling
- active scan overlay showing source set: VFO, all channels, bank, scan list

### Memory management UI

- add `Save to Memory` from VFO main screen
- add `Edit Channel` when in channel mode
- add `Copy to VFO`
- add conflict-safe overwrite confirmation

### FM polish UI

- quick action for monitor/reverse
- grouped tone settings page
- TOT and BCLO under Radio or FM submenu
- VOX under Radio with clear enable and sensitivity wording

### M17 UI

- quick destination recall from recent contacts
- compact destination/CAN summary on main screen when in M17
- easier meta text discoverability without crowding primary operation

### Backup/restore UI

- move under Device
- dedicated transfer progress screen
- explicit warnings and success/failure completion screen

### Service menu UI

- hidden or opt-in entry path to avoid casual user confusion
- read-only diagnostics first
- write-capable calibration tools only after confirmation gates

## Visual and Interaction Direction

Even on constrained displays, the UI can feel more deliberate.

### Typography and density

- make the tuned frequency visually dominant
- use smaller stable labels for mode/status information
- avoid clutter by reserving one region for transient context

### Screen behavior

- prefer overlays and short panels over full-screen mode switching for small
  actions
- use a consistent edit frame for numeric input, text input, and toggles
- ensure every edit screen clearly indicates confirm and cancel behavior

### Audio behavior

- every quick action should have a concise prompt path
- scanning should announce start/stop and optionally source set
- memory save should announce target channel and success/failure

## Technical Redesign Strategy

### Phase 1: foundation without visual overhaul

- introduce a quick-action abstraction in the UI FSM
- cleanly separate browse vs edit vs transient action states
- centralize shared input widgets for number, toggle, and text editing

### Phase 2: menu architecture cleanup

- regroup top-level menus by user intent
- reduce deep path length for common tasks
- add scan and memory workflows

### Phase 3: status-rich main screen

- redesign layout regions
- add badges and context strip
- add scan and memory visual states

### Phase 4: service and advanced workflows

- diagnostics screens
- calibration UI
- backup/restore progress and recovery flows

## Implementation Notes

### Recommended code areas to touch first

- `openrtx/src/ui/default/ui.c`
- `openrtx/src/ui/default/ui_main.c`
- `openrtx/src/ui/default/ui_menu.c`
- `openrtx/include/ui/ui_default.h`
- `openrtx/src/core/voicePromptUtils.c`

### Important constraints

- keep voice prompt ordering and string table stability in mind
- avoid hard-coding target-specific layout assumptions into shared logic
- preserve low-resolution display usability
- keep actions reachable on no-keyboard variants where supported

## Suggested First UI Redesign Branches

- `feature/ui-quick-actions`
- `feature/ui-main-screen-status-redesign`
- `feature/ui-memory-edit-flow`
- `feature/ui-scan-workflow`
- `feature/ui-service-menu-foundation`

## Definition of Success

The redesign is successful if an operator can do the following with fewer steps
 and less ambiguity than today:

- start and stop scan
- save a VFO to memory
- edit a repeater or tone setup safely
- inspect M17 routing state
- trigger backup/restore with confidence
- recover from mistakes without wondering what state the UI is in
