# Porymap Studio

This project is Porymap Studio, an upstream-friendly fork of [huderlem/porymap](https://github.com/huderlem/porymap). It remains a native Qt map editor for Pokémon Generation 3 decompilation projects.

## Run & Operate

- `./scripts/replit-build.sh` — configure with qmake and compile Porymap Studio
- `./scripts/replit-run.sh` — build incrementally, then launch the desktop application
- The `Porymap` workflow launches the application in a desktop/VNC preview
- No application secrets or environment variables are required

## Upstream Baseline

- Upstream remote: `https://github.com/huderlem/porymap.git`
- Imported branch: `master`
- Imported commit: `26b919d7ff4b54152de010abd5bf344af2ebe116`
- Fetch future upstream changes with `git fetch upstream`

The fork started from that upstream commit. Studio changes should remain small, modular, and easy to distinguish from upstream code.

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
- `src/studio/` and `include/studio/` — Porymap Studio-specific extension boundary
- `scripts/replit-build.sh` — Replit/Nix-aware build entry point

## Architecture Decisions

- Keep Porymap as its upstream Qt/qmake application; do not convert it to the removed web/API starter structure.
- Treat `upstream/master` as the source baseline and make future product changes as explicit fork commits.
- Keep existing internal names, config keys, executable names, and project formats unless a feature requires otherwise.
- Put Studio-only services under the Studio extension boundary instead of scattering them through unrelated upstream files.
- Reuse `Project`, `Map`, `Layout`, `Tileset`, `Metatile`, and existing renderers before creating new models.
- Discover Nix store paths at build time instead of committing machine-specific store hashes.

## Product

Porymap Studio currently preserves Porymap's map, layout, event, tileset, region-map, encounter, and project editing behavior. Future Studio milestones add image-assisted creation and automation incrementally.

## Developer References

- `docs/PORYMAP_STUDIO_ARCHITECTURE.md`
- `docs/PORYMAP_STUDIO_ROADMAP.md`
- `docs/PORYMAP_STUDIO_TECHNICAL_NOTES.md`

## Gotchas

- Use `./scripts/replit-build.sh` rather than calling `qmake` directly. Replit exposes Qt development modules as split Nix outputs, so the helper assembles qmake, OpenGL include, and linker paths.
- Porymap Studio launches without a game project, but meaningful editing requires a compatible Pokémon decompilation project.
- Build outputs are ignored by Git and remain local.
- There is currently no automated unit-test suite; native builds and targeted manual smoke tests are the regression baseline.