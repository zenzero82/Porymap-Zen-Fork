# Studio extension boundary

Porymap Studio-specific code belongs under `src/studio/` with public headers under
`include/studio/`. Existing Porymap models, rendering, project I/O, and widgets
remain the source of truth.

Planned feature areas:

- `imageimport/` — image validation, analysis, matching, and import orchestration.
- `tilesets/` — metatile rendering and future Porytiles process integration.
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