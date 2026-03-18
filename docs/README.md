# OpenRTX Working Docs

This folder captures working documentation for firmware planning, UI redesign,
project structure, and medium-term implementation strategy.

These notes are meant to help contributors quickly recover context, understand
 the codebase layout, and continue work without re-discovering the same design
 decisions.

## Documents

- `docs/project_map.md` - codebase map, subsystem ownership, build targets,
  external subprojects, and where to extend the firmware safely.
- `docs/architecture_current.md` - current runtime architecture, data flow,
  thread model, UI structure, and where major responsibilities live today.
- `docs/firmware_roadmap.md` - prioritized feature roadmap with rationale,
  implementation notes, risks, and recommended sequencing.
- `docs/ui_redesign_plan.md` - UI redesign direction, information architecture,
  interaction model, accessibility goals, and concrete UI feature proposals.
- `docs/reference_index.md` - curated file references, build targets, tests,
  external dependencies, and project links for fast re-entry.

## Current Context

- The firmware already has strong foundations in FM, M17, voice prompts,
  persistent settings, and multi-target platform support.
- The next highest-value product work is centered on scan, on-radio memory
  editing, FM operating polish, backup/restore completion, and better service
  tooling.
- Recent branch work added radio PPM correction and fixed the ability to clear
  entered radio offset values back to zero in the default UI.

## How To Use These Notes

- Start with `docs/project_map.md` to understand subsystem boundaries.
- Read `docs/architecture_current.md` when you need to reason about how the
  current firmware behaves rather than how it should evolve.
- Use `docs/firmware_roadmap.md` to pick the next feature branch.
- Use `docs/ui_redesign_plan.md` when a feature touches menus, workflows,
  voice prompts, or user interaction.
- Use `docs/reference_index.md` as a jump table into important files and
  external project dependencies.
- Keep these docs updated when adding new major workflows, settings,
  protocol-level behavior, or build/runtime dependencies.
