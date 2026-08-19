---
name: Qt qmake on Replit
description: Why Porymap's Qt build needs dynamic qmake and OpenGL paths in Replit's Nix environment.
---

Replit can expose `qmake` while keeping Qt module metadata and OpenGL development headers in separate Nix outputs. A bare `qmake` may therefore report that even core Qt modules are unknown, despite all required packages being installed.

**Why:** Installing broader Qt package sets did not propagate the development search paths. The build only became reliable after dynamically assembling `QMAKEPATH`, `CPATH`, and `LIBRARY_PATH`; hard-coded Nix store hashes would become stale.

**How to apply:** Build through the repository's Replit helper, which derives paths from the active Qt version and linked OpenGL libraries. Keep the upstream qmake project unchanged unless a real cross-platform source change is intended.