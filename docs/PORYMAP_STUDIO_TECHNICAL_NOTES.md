# Porymap Studio Technical Notes

These notes define the likely implementation path for the next rendering and
tileset milestones. They document integration seams only; no matcher or
Porytiles integration is implemented in the foundation.

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

## Future Porytiles integration

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

### Recommended integration

Start with a subprocess adapter rather than linking or copying Porytiles:

1. Let the user configure a Porytiles executable.
2. Query and validate its version before enabling actions.
3. Use `QProcess` with an argument list, never a shell command string.
4. Stage source/output in a dedicated temporary or preview directory.
5. Capture stdout, stderr, exit status, warnings, and elapsed time.
6. Support cancellation and a bounded timeout.
7. Validate every expected output before replacing project assets.
8. Show a preview/summary and require confirmation.
9. Refresh assets through existing `Project`/`Tileset` loaders and file-watcher
   behavior.

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