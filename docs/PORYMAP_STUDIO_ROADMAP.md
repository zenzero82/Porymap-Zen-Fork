# Porymap Studio Roadmap

This roadmap is intentionally high level. Each milestone should be delivered as
a small, buildable change that preserves existing Porymap behavior and remains
compatible with future upstream merges.

## Milestone 0 — Foundation and fork setup

**Status:** Foundation prepared.

- Establish visible Porymap Studio identity and preserve Porymap attribution.
- Document architecture and compatibility boundaries.
- Create the Studio extension boundary.
- Verify native Replit builds and prepare build-only Windows CI.
- Document metatile-rendering and Porytiles integration paths.

## Milestone 1 — New Map From Image UI

- Add a File menu entry and Qt dialog.
- Reuse existing map, layout, and tileset selectors.
- Load PNG files, preview them, report dimensions, and validate grid alignment.
- Keep image analysis outside the dialog.
- Do not create or modify a map yet.

## Milestone 2 — Metatile rendering service

- Extract deterministic, UI-independent rendering from existing image-provider
  and tileset APIs.
- Render every valid primary/secondary metatile with explicit layer and palette
  settings.
- Add focused tests for IDs, dimensions, layer order, and invalid assets.

## Milestone 3 — Exact image-to-metatile matching

- Split source artwork into metatile-sized cells.
- Match exact pixels against rendered metatiles.
- Return IDs and unmatched-cell diagnostics without changing a layout.
- Create a normal Porymap map and layout only after a complete exact reconstruction is confirmed.
- Populate standard blockdata in memory and leave persistence to Porymap's normal save flow.

## Milestone 4 — Fuzzy matching and confidence

- Add measurable color/pixel distance.
- Return ranked candidates and confidence scores.
- Keep thresholds visible and deterministic.
- Run fuzzy matching only as an explicit follow-up when exact matching leaves
  unresolved cells.
- Keep fuzzy suggestions separate from map-creation approval.

## Milestone 5 — Import preview and correction

**Status: Complete.**

- Preview proposed layout results.
- Highlight uncertain and unmatched cells.
- Let users choose ranked alternatives before committing.
- Require an explicit approval for every non-exact cell.
- Revalidate source pixels and rendered candidates before map creation.
- Keep native regression coverage for approval, replacement, clearing, stale
  source/render rejection, exact preservation, and approved-ID resolution.

## Milestone 6 — Generate Porymap layouts

**Status: Complete.**

- Convert an approved result into normal `Layout`/`Blockdata` edits.
- Use existing undo, dirty-state, validation, and save paths.
- Never write a parallel map format.
- Reject incomplete, duplicate, out-of-bounds, stale, or tileset-invalid
  blockdata before a map is registered.
- Keep native regression coverage for row-major placement, exact and approved
  fuzzy IDs, and default collision/elevation values.

## Milestone 7 — Porytiles integration

**Status: complete.**

- Invokes a pinned, explicitly selected Porytiles executable through `QProcess`
  with structured arguments.
- Stages inputs and generated output in an isolated temporary directory.
- Captures diagnostics, supports timeout/cancellation, validates generated
  assets, and requires an explicit preview confirmation before replacement.
- Preserves unrelated target files and refreshes active Porymap tilesets only
  after confirmed compile output is installed.

## Milestone 8 — Generate tilesets from complete artwork

- Convert source artwork into decompiled Porytiles-format assets.
- Compile and validate generated Porymap-format tileset output.
- Surface palette and tile-allocation failures clearly.

## Milestone 9 — Smart collision assistance

- Suggest collision/elevation values from imported terrain.
- Require preview and confirmation.
- Commit through normal undoable layout edits.

## Milestone 10 — Terrain and autotile brushes

- Define reusable terrain rules.
- Paint connected terrain while preserving manual control.
- Keep generic behavior separate from expansion-specific rules.

## Milestone 11 — Build and test ROM integration

- Add configurable project build/test commands.
- Stream output, support cancellation, and report actionable errors.
- Do not assume one decompilation-project layout.

## Milestone 12 — Optional AI-assisted tools

- Keep AI features optional and isolated.
- Never send project assets without explicit user action.
- Prefer deterministic editor tools whenever they solve the same problem.

## Ongoing compatibility requirements

- Preserve `pokeruby`, `pokeemerald`, and `pokefirered` support where practical.
- Treat `pokeemerald-expansion` as the primary future target without changing
  generic Porymap behavior.
- Do not rewrite the app, change Gen 3 formats, remove existing features, or
  introduce ROM-engine changes as editor features.
