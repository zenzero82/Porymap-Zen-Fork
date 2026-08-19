# Porymap Studio Architecture

This document records the upstream Porymap architecture that Porymap Studio
must reuse. It is a development guide, not a proposal to rewrite the editor.

## Architectural rules

1. Keep the native C++17, Qt, qmake application.
2. Keep Gen 3 project formats and Porymap's serializers authoritative.
3. Preserve internal class, file, config, and executable names when practical.
4. Add Studio behavior under `src/studio/` and `include/studio/`.
5. Isolate pokeemerald-expansion-specific behavior behind explicit
   compatibility checks.
6. Make future UI workflows call non-UI services for analysis and process work.

## Application startup

`src/main.cpp` is the process entry point. It configures Qt display behavior,
creates `QApplication`, creates the loading screen, constructs `MainWindow`,
calls `MainWindow::initialize()`, and enters the Qt event loop.

`MainWindow::initialize()` initializes the editor UI, optionally reopens the
most recent project, restores window state, and shows the main window.

The visible fork identity is centralized in `Studio::ProductInfo`. The internal
application name remains `porymap` so existing settings locations, clipboard
payloads, network user agents, and upstream behavior remain compatible.

## Main window and editor

`MainWindow` in `include/mainwindow.h` and `src/mainwindow.cpp` is the top-level
Qt orchestration layer. It owns dialogs, menus, project lifecycle actions,
status presentation, and connections between the generated UI and editor.

`Editor` in `include/editor.h` and `src/editor.cpp` owns active project, map,
layout, graphics scenes/items, selection modes, and editing behavior. Future
Studio dialogs should not bypass `Editor` and mutate views directly.

Designer forms under `forms/` are compiled into generated `ui_*.h` files.
`forms/mainwindow.ui` defines the File menu, map list, editor tabs, toolbars,
tileset selectors, and map/event/header/connection/wild-encounter surfaces.
Edit the `.ui` source rather than generated headers.

## Project open and save lifecycle

`MainWindow::openProject()` performs the application-level open flow:

1. validate and normalize the selected directory;
2. close the previous project safely;
3. load global, user, and project configuration;
4. initialize scripting;
5. create `Project` and connect its signals;
6. assign it to `Editor`;
7. validate and load project data;
8. populate the UI and open an initial map; and
9. enable the editor and persist recent-project state.

`Project` in `include/project.h` and `src/project.cpp` is the repository and
cache for maps, layouts, tilesets, parsed project metadata, and file watchers.
Its `load`, `saveAll`, `saveMap`, `saveLayout`, `saveTilesets`, and configuration
methods remain the only supported persistence boundary for Studio features.

Project JSON is handled with `OrderedJson` so unknown data can be preserved.
Headers, defines, scripts, and other decompilation sources are parsed with
existing project and `ParseUtil` code. `FileDialog` centralizes user file
selection, while `QFileSystemWatcher` integration reports external changes.

## Existing New Map workflow

`forms/mainwindow.ui` declares `action_NewMap`.
`MainWindow::initExtraSignals()` connects it to
`MainWindow::openNewMapDialog()`, which opens `NewMapDialog` from
`include/ui/newmapdialog.h`, `src/ui/newmapdialog.cpp`, and
`forms/newmapdialog.ui`.

New map settings are represented by `Project::NewMapSettings`. They include map
name/group information, `Layout::Settings`, and `MapHeader`.

`Project::createNewMap()` generates the map constant, creates or loads its
layout, registers the map in the project, marks it as not yet persisted, and
emits `mapCreated`.

`Project::createNewLayout()` initializes dimensions, border dimensions,
primary/secondary tileset labels, binary layout paths, default block data, and
tileset assets before registering the layout.

The future New Map From Image flow should collect and validate inputs, then hand
a normal `Project::NewMapSettings`/layout result to this lifecycle. It must not
invent a separate map format or write project files from the dialog.

## Core models and storage

### Map

`Map` in `include/core/map.h` contains:

- a referenced `Layout`;
- a `MapHeader`;
- object, warp, coordinate, background, and heal-location events;
- map connections;
- scripts and custom attributes;
- persistence/dirty state; and
- a `QUndoStack`.

Maps do not own their shared layout. Changes should enter existing edit history
and dirty-state paths so Save, close prompts, and scripting stay correct.

### Layout

`Layout` in `include/core/maplayout.h` contains:

- map and border dimensions;
- primary and secondary `Tileset` references;
- map and border `Blockdata`;
- render/cache images; and
- undo history.

`Layout::setBlock`, `setMetatileId`, resizing methods, and related edit commands
are the supported mutation path. Layout block and border data are persisted in
the binary files already used by compatible decompilation projects.

### Blocks, tiles, and metatiles

`Blockdata` stores the layout's block grid. `Block` carries the metatile ID,
collision value, and elevation. `Metatile` describes the layered 16x16 map cell,
and `Tile` describes the underlying 8x8 tile assignment, flips, and palette.

These types are the future image-import output vocabulary. A matcher should
produce metatile IDs and confidence information; layout mutation should still
use `Layout` and existing edit commands.

## Tilesets, palettes, and rendering

`Tileset` in `include/core/tileset.h` owns:

- tileset graphics;
- metatiles and metatile attributes;
- palette paths and loaded palettes;
- primary/secondary ID ranges; and
- load/save helpers.

`Project::loadLayoutTilesets()`, `loadTilesetAssets()`, and
`loadTilesetMetatileLabels()` form the existing asset-loading pipeline.
`Tileset::getMetatile()`, `getTileTileset()`, `getPalette()`, and range helpers
already resolve paired primary/secondary assets.

Reusable rendering functions are in `src/ui/imageproviders.cpp`:

- `getMetatileImage()` renders one metatile;
- `getMetatileSheetImage()` renders a selected range or paired tilesets; and
- the renderer accepts layer order, layer opacity, size, and palette mode.

`MetatileImageExporter::getImage()` is an existing UI wrapper around sheet
rendering. `Layout::render()`, `renderBorder()`, and `renderCollision()` produce
map-level pixmaps. `LayoutPixmapItem`, `BorderMetatilesPixmapItem`, and
`CollisionPixmapItem` display those results in graphics scenes.

`Tileset` stores loaded and preview palettes. `paletteutil` handles color
conversion, while `PaletteEditor` and `PaletteColorSearch` provide the existing
editing UI.

Future matching code should extract a UI-independent metatile-rendering service
from these proven functions rather than screenshotting a `QGraphicsScene`.

## Collision, events, and warps

Collision and elevation are fields on `Block`. `Layout` implements collision
rendering and fill operations; `Editor` supplies collision editing mode and the
`MovementPermissionsSelector`. Studio collision suggestions must remain
previewable and undoable before changing blocks.

The `Event` hierarchy in `include/core/events.h` represents object events,
warps, triggers, background events, hidden items, secret bases, and heal
locations. `Map` owns grouped events; `Project` loads and saves their JSON.
`EventPixmapItem` and event frames provide the existing UI.

Map connections use the separate `MapConnection` model and connection graphics
items. Image-assisted layout creation must not infer, delete, or rewrite events,
warps, or connections unless a later feature explicitly asks the user to do so.

## Supported platforms and build

`porymap.pro` is the authoritative build manifest. It builds a C++17 Qt Widgets
application using Core and GUI plus optional Charts, QML, and Network modules.
It lists all sources, headers, forms, resources, and the bundled QtGifImage
dependency.

- Replit/Linux: `./scripts/replit-build.sh`
- Generic Linux/macOS: qmake followed by Make
- Windows: qmake followed by MinGW Make or Qt Creator

The Replit helper discovers split Nix Qt/OpenGL paths dynamically. Do not commit
Nix store hashes. `.github/workflows/build.yml` performs build-only Linux and
Windows checks and uploads a Windows executable artifact; it does not publish a
release.

## Test baseline

The imported project has no unit-test directory, test target, or test framework.
Current regression protection is:

1. clean qmake generation;
2. successful compiler/linker completion on Linux and Windows;
3. native startup smoke testing; and
4. focused manual tests with a compatible decompilation project.

Future pure Studio services should add isolated tests when their first behavior
is introduced. UI tests should not be invented as part of the foundation.

## Studio extension boundary

The first module is `Studio::ProductInfo`. Future modules belong under:

- `studio/imageimport/`
- `studio/tilesets/`
- `studio/build/`

Create each area with its first real feature, not with empty placeholder
classes. Studio services may consume existing Porymap models and renderers but
must not duplicate project parsing, serialization, validation, or edit history.