# Porymap Studio Technical Notes

These notes define the likely implementation path for the next rendering and
tileset milestones. They document integration seams only; no matcher or
Porytiles integration is implemented in the foundation.

## Phase 3 map creation

The New Map From Image flow can create a normal Porymap map only when every
source cell has an exact metatile match. The commit step validates the map name,
map group, dimensions, tilesets, layout identifier, and row-major cell coverage
again so stale analysis cannot be applied.

The new map and layout are created through `Project::createNewMap()`. Matched
metatile IDs are applied as standard `Blockdata` through the layout undo stack;
collision and elevation use their default zero values. Creation remains
in-memory and unsaved until the user invokes Porymap's existing save flow.

This phase does not overwrite existing maps or layouts, perform fuzzy matching,
infer collision/elevation, or write a parallel map format.

## Future metatile rendering

### Existing reusable path

Porymap already loads everything a matcher needs:

1. `Project::loadLayoutTilesets()` resolves layout tileset labels.
2. `Project::loadTilesetAssets()` loads graphics, palettes, metatiles, and
   attributes.
3. `Tileset` resolves paired primary/secondary IDs and palettes.
4. `getMetatileImage()` in `src/ui/imageproviders.cpp` renders a single
   metatile.
5. `getMetatileSheetImage()` renders an ID range or a paired sheet.
6. `MetatileImageExporter::getImage()` proves that the renderer can produce a
   `QImage` without capturing the editor canvas.

The future service should reuse or extract the image-provider implementation.
It should not render by screenshotting `QGraphicsScene`, because scene state,
zoom, overlays, and selection can make that output nondeterministic.

### Proposed service contract

A future `studio/tilesets` renderer should accept:

- primary and optional secondary `Tileset` references;
- one metatile ID or an explicit ID range;
- layer order and layer opacity;
- true-palette versus preview-palette mode; and
- a requested output size.

It should return a `QImage` plus a structured error for missing assets, invalid
IDs, or unavailable palettes. Rendering must not mutate global configuration.

The current exporter temporarily changes render-related project settings. A
pure Studio renderer should make those settings explicit inputs before matching
work begins.

### Phase 1 image analysis boundary

`Studio::MapImageAnalyzer` decodes a selected PNG without scaling or otherwise
changing its decoded pixel data. It reports the exact source dimensions, then
validates that both dimensions align to `Metatile::pixelSize()`. In the current
Porymap renderer this resolves to **16 × 16 pixels**, but Studio deliberately
reads the value from `Metatile` rather than hard-coding it.

`Studio::MetatileRenderService` renders each valid primary and secondary
metatile through the existing `getMetatileImage()` path. That retains Porymap's
palette selection, layer composition, flips, transparency, and primary/
secondary ownership rules. It snapshots the active layout's layer order and
opacity before the dialog opens; if no layout is active, it snapshots Porymap's
global/default layer settings. Results are deterministic `QImage` values and
are rendered afresh for every analysis because Porymap's editable tileset
models do not expose reliable revision signals.

Analysis is only READY when every available candidate rendered successfully.
Manual map dimensions are accepted only when they exactly match the aligned,
unchanged PNG dimensions and pass `Project::mapDimensionsValid()`.

Phase 1 deliberately ends after the dialog reports **READY FOR MATCHING**.
It does not create or edit a map, layout, tileset, collision data, or project
file.

### Phase 2 exact reconstruction boundary

`Studio::ImageMetatileMatcher` splits only the unchanged, grid-aligned source
image into metatile-sized cells. It normalizes source cells and the Phase 1
candidate images to `QImage::Format_RGBA8888`, then performs exact byte-for-byte
pixel matching. Exact matching remains the first and preferred path.

The matcher retains a coordinate, source-cell image, and matched metatile
identity (when present) for every cell. It builds reconstructed and difference
`QImage` previews in memory: a complete exact result is byte-equal to the
normalized source image, while unmatched cells are visibly highlighted for
review. The dialog offers Original, Reconstructed, and Differences tabs plus
an unmatched-cell inspector. None of these actions creates, changes, or saves
maps, layouts, tilesets, collision data, or project files.

### Phase 4 fuzzy matching boundary

Fuzzy matching is an explicit second analysis step offered only after exact
matching leaves cells unresolved. It never replaces or downgrades an exact
match. For each non-exact cell, the matcher compares normalized, premultiplied
RGBA channel values and reports mean absolute channel distance normalized to
the range 0.0–1.0. Premultiplication ensures invisible RGB values in fully
transparent pixels do not distort visual similarity.

Candidates are ordered by ascending distance, with their original deterministic
render order breaking ties. Confidence combines closeness with the normalized
distance gap between the best and second-best candidates. Equal best
candidates therefore have zero confidence rather than appearing certain.

The dialog exposes maximum distance and minimum confidence as percentages:

- a best candidate within both thresholds is an accepted suggestion;
- a candidate within the distance threshold but below minimum confidence is
  uncertain; and
- a candidate outside the distance threshold is rejected.

Accepted means only that the deterministic thresholds accepted the suggestion.
It does not mean the user approved the metatile for import. Fuzzy results show
ranked alternatives and colored diagnostics, but the Create Map action remains
available only for complete exact reconstructions. Manual selection and
approval belong to the correction milestone.

Fuzzy evaluations are cached for repeated source cells and only the candidates
needed for confidence and display are partially sorted. The dialog also reports
progress for every source cell and permits cancellation between cells, keeping
large analyses observable and interruptible without changing ranking order.

### Phase 5 correction and approval boundary

Fuzzy classification and user approval are separate states. Selecting a ranked
candidate marks that cell as edited; the user must explicitly approve the
selection before it can contribute to map creation. Replacing a selection
returns the cell to edited state, and clearing it makes the cell unresolved.

Corrections live only in the dialog. Each correction retains the unchanged
source-cell pixels, metatile identity, source tileset identity, and the rendered
candidate image that the user reviewed. The reconstructed and difference
previews overlay edited and approved alternatives so the visible proposal
matches the blockdata that would be created.

The Create Map action is enabled only when every non-exact cell has a validated
approval. At commit time the dialog repeats PNG decoding, tileset rendering,
dimension validation, and exact matching. An approval is accepted only when its
source pixels, metatile identity, tileset ownership, and rendered candidate
pixels still match the fresh analysis. Stale or incomplete approvals block map
creation. Approved metatile IDs then flow through the existing in-memory
`Blockdata` creation path; collision and elevation remain zero and no project
files are written until Porymap's normal save action.

The dialog and native tests share the same approval validation and metatile-ID
resolution helper. Regression coverage verifies that incomplete and cleared
corrections block creation, replacements require re-approval, changed source or
rendered pixels invalidate approvals, exact cells retain their exact IDs, and
approved fuzzy cells resolve to the selected metatile ID.

### Phase 6 normal layout generation boundary

The approved-cell conversion is a shared, UI-independent operation. It requires
complete, unique, in-bounds cell coverage for the selected dimensions and
revalidates every correction before producing row-major `Blockdata`. Exact
cells retain their exact metatile IDs, approved corrections use the explicitly
selected IDs, and collision/elevation remain zero.

The dialog passes that standard `Blockdata` to `Project::createNewMap()`.
Project creation remains the authority for map and layout identifiers, map
dimensions, tilesets, group registration, and metatile validity. The initial
layout replacement is a single `ImportMetatiles` command on the existing layout
undo stack. The normal `mapCreated`/`layoutCreated` signals select and display
the map through the editor, and the existing dirty-state and save APIs remain
the only persistence path.

If validation fails, no map is registered and any newly allocated layout is
removed before signals are emitted. Studio does not write map, layout, group,
or project files directly and does not define a parallel map representation.

#### Reproducible developer verification

1. Build with `./scripts/replit-build.sh`.
2. Open an existing project and choose **File → New Map From Image...**.
3. Select any PNG whose width and height are divisible by the displayed
   metatile size, select a primary and secondary tileset, and run analysis.
4. Confirm the exact source dimensions, detected map dimensions, non-zero
   rendered-metatile count, and **READY FOR MATCHING** status.
5. In a debug build, use **Export Debug Metatiles...** and confirm deterministic
   local names such as `primary_0000.png` and `secondary_0000.png`.
6. Repeat with a PNG whose width or height is not divisible by the metatile
   size and confirm that alignment fails before rendering or project changes.
7. For a PNG known to use the selected tilesets, run analysis and confirm that
   the reconstructed tab is visually identical to the original, the
   differences tab is transparent, and the summary reports 100.0% exact
   matches.
8. For a PNG containing at least one non-tileset cell, confirm that unmatched
   cells are counted, highlighted in the differences tab, and listed by
   **Review Unmatched**, with no project dirty state or file change.

### Matching pipeline boundary

The next milestones should keep this flow independent from the dialog:

```text
PNG
  -> decode and validate
  -> split into metatile-sized cells
  -> render available metatile candidates
  -> exact comparison
  -> ranked/fuzzy comparison when needed
  -> result with IDs, confidence, and unmatched cells
  -> preview
  -> approved normal Layout edits
```

The matcher should return data only. It must not create files or mutate
`Project`, `Map`, or `Layout`. An import coordinator can later translate an
approved result through existing undoable edit paths.

## Porytiles subprocess integration

### Current command-line model

Porytiles is a separate tileset compiler/decompiler. Its current documentation
uses the `porytiles-legacy` command. A primary compilation example is:

```text
porytiles-legacy compile-primary -dual-layer -Wall \
  -o <pokeemerald>/data/tilesets/primary/<name> \
  <decompiled-source-directory> \
  <pokeemerald>/include/constants/metatile_behaviors.h
```

The corresponding decompile form is:

```text
porytiles-legacy decompile-primary \
  -o <decompiled-output-directory> \
  <pokeemerald>/data/tilesets/primary/<name> \
  <pokeemerald>/include/constants/metatile_behaviors.h
```

Secondary tilesets use equivalent secondary commands and also depend on their
primary tileset context. The exact CLI and directory naming can change between
Porytiles releases, so integration must detect and pin a supported version
instead of assuming whichever executable happens to be on `PATH`.

### Inputs and outputs

Decompiled/Porytiles-format source uses editable layer artwork and metadata,
including layer PNGs such as `bottom.png`, `middle.png`, and `top.png`, plus
attribute data and optional animation assets.

Compilation produces the assets Porymap already consumes, including:

- compiled `tiles.png`;
- hardware palettes under `palettes/`;
- `metatiles.bin`; and
- metatile attribute output.

The behavior constants header is required to map named behaviors to the numeric
attributes stored by the target decompilation project.

### Complete artwork generation

Phase 8 accepts one PNG containing equal-height bottom, middle, and top layers
stacked vertically. Each layer must be aligned to the native 16×16 metatile
grid. Studio splits the sheet without resampling, counts unique 8×8 pixel tiles
in deterministic row-major order, and rejects artwork that exceeds the selected
primary or secondary tileset's tile and color budgets.

The generated source package contains `bottom.png`, `middle.png`, `top.png`,
and a legacy Porytiles `attributes.csv` using `id,behavior` rows initialized to
`MB_NORMAL`. Collision/elevation inference is intentionally deferred to Phase
9. The package is written only to a new user-selected directory outside the
active project. The dialog displays every generated layer and the complete CSV
for review, then requires explicit confirmation before passing the immutable
package path to the existing staged Porytiles compile dialog. Secondary
generation validates a complete paired primary source before creating output.

### Smart collision and elevation assistance

Collision and elevation are map-block fields, not Porytiles metatile
attributes. Phase 9 therefore keeps these suggestions out of `attributes.csv`
and runs them after image-to-metatile matching in New Map From Image.

The UI-independent suggester examines each 16×16 terrain cell. Low opacity
produces a high-confidence blocked suggestion; opaque cells remain walkable
unless a fully opaque bright boundary provides a conservative raised-edge cue.
Cells without reliable visual evidence retain collision/elevation zero and are
marked low confidence. Every result includes an explanation.

Users review all cells in a table and explicitly choose Apply Suggestions,
Keep Defaults, or Cancel. Accepted values are set through `Block` setters and
travel with the existing `ImportMetatiles` undo command. No project file is
written by analysis or review.

### Implemented integration

The Studio integration uses a subprocess adapter rather than linking or copying
Porytiles:

1. Let the user configure an explicit Porytiles executable and exact version.
2. Query and validate its version before starting an operation.
3. Use `QProcess` with an argument list, never a shell command string.
4. Stage source/output in a dedicated temporary or preview directory.
5. Capture stdout, stderr, exit status, warnings, and elapsed time.
6. Support cancellation and a bounded timeout.
7. Validate expected files, non-empty binary/palette output, and readable PNGs
   before replacing project assets.
8. Show a preview/summary and require confirmation.
9. Preserve unrelated target files, swap the prepared directory with rollback,
   and refresh through existing tileset loaders only after confirmation.

The Tileset Editor exposes this workflow through **File → Porytiles
Integration…**. Unsaved in-editor changes block the operation. Primary and
secondary compile/decompile commands use the documented legacy positional
forms, with the required primary source or compiled context staged for
secondary operations.

This approach minimizes modifications to both projects, keeps licensing and
versioning boundaries clear, and allows Porytiles to evolve independently.

### Alternatives considered

- **Bundle an executable:** convenient for users but adds platform packaging,
  update, provenance, and license-notice responsibilities. Consider only after
  the subprocess adapter is stable.
- **Link Porytiles as a library:** currently creates a tight build/API coupling
  and would complicate Windows/macOS packaging. Do not use as the first
  integration.
- **Copy Porytiles code:** would create a difficult-to-update fork inside this
  fork. Do not do this.
- **Require a global PATH install:** simple but brittle. Support explicit
  executable selection and use PATH only as an optional discovery mechanism.

### Safety and compatibility

- Never overwrite tileset assets until compilation succeeds and outputs pass
  validation.
- Keep backups or use atomic replacement.
- Never execute project-provided command strings.
- Treat expansion-specific behaviors as project configuration, not hard-coded
  generic Porymap behavior.
- Record the Porytiles version and invocation in logs for reproducibility.

## Windows validation

The CI workflow proves that the current source configures and compiles with a
dynamic Qt/MinGW toolchain. It does not replace testing on a Windows PC.

Before shipping a feature, verify:

- native window titles, dialogs, file pickers, and scaling;
- loading a real compatible project;
- Unicode and space-containing paths;
- process cancellation and path quoting for future Porytiles integration; and
- packaging of all required Qt runtime libraries.