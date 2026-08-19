# Porymap Fork

This project is a clean fork baseline of [huderlem/porymap](https://github.com/huderlem/porymap), a Qt map editor for the Pokémon generation 3 decompilation projects.

## Run & Operate

- `./scripts/replit-build.sh` — configure with qmake and compile Porymap
- `./scripts/replit-run.sh` — build incrementally, then launch the desktop application
- The `Porymap` workflow launches the application in a desktop/VNC preview
- No secrets or environment variables are required

## Upstream Baseline

- Upstream remote: `https://github.com/huderlem/porymap.git`
- Imported branch: `master`
- Imported commit: `26b919d7ff4b54152de010abd5bf344af2ebe116`
- Fetch future upstream changes with `git fetch upstream`

The imported Porymap source matches that upstream commit. Replit-specific additions are limited to the native dependency declaration, local build/run helpers, this handoff document, and the qmake cache ignore rule.

## Stack

- C++17
- Qt 6.9 with Core, GUI, Widgets, Network, QML, and Charts
- qmake and GNU Make
- Native Linux desktop application

## Where Things Live

- `porymap.pro` — qmake project and authoritative source list
- `src/` — application implementation
- `include/` — application headers
- `forms/` — Qt Designer UI forms
- `resources/` — icons, themes, images, and bundled text
- `docs/` and `docsrc/` — generated and source documentation
- `scripts/replit-build.sh` — Replit/Nix-aware build entry point

## Architecture Decisions

- Keep Porymap as its upstream Qt/qmake application; do not convert it to the removed web/API starter structure.
- Treat `upstream/master` as the source baseline and make future product changes as explicit fork commits.
- Discover Nix store paths at build time instead of committing machine-specific store hashes.

## Product

Porymap edits maps, layouts, events, tilesets, region maps, encounters, and related data for compatible `pokeruby`, `pokeemerald`, and `pokefirered` decompilation projects.

## Gotchas

- Use `./scripts/replit-build.sh` rather than calling `qmake` directly. Replit exposes Qt development modules as split Nix outputs, so the helper assembles qmake, OpenGL include, and linker paths.
- Porymap launches without a game project, but meaningful editing requires a compatible Pokémon decompilation project.
- Build outputs are ignored by Git and remain local.