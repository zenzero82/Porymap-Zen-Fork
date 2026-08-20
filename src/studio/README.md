# Studio extension boundary

Porymap Studio-specific code belongs under `src/studio/` with public headers under
`include/studio/`. Existing Porymap models, rendering, project I/O, and widgets
remain the source of truth.

Current and planned feature areas:

- `MapImageAnalyzer` — Phase 1 PNG validation and map-grid analysis only.
- `MetatileRenderService` — Phase 1 deterministic metatile rendering only.
- Future `imageimport/` — matching and import orchestration.
- Future `tilesets/` — Porytiles process integration.
- `build/` — project build/test process orchestration.

Create these subdirectories only when their first real implementation is added.
Do not add empty service classes or duplicate Porymap data models merely to fill
out the directory tree.

Studio modules should:

1. depend on existing Porymap APIs instead of copying them;
2. keep UI code separate from analysis and process logic;
3. expose explicit results and errors rather than mutating global state silently;
4. isolate expansion-specific behavior behind compatibility checks; and
5. preserve the Gen 3 project formats already handled by Porymap.

The Phase 1 dialog intentionally stops at **READY FOR MATCHING**. It must not
create maps or layouts, write image data into a project, run matching, or
modify tilesets/collision data.