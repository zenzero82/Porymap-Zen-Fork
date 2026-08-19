---
name: Windows Qt CI toolchain
description: Keep Qt MinGW builds paired with their matching compiler toolchain in GitHub Actions.
---

Use the MinGW toolchain that matches the installed Qt distribution for Windows Qt CI builds. Do not rely on the version preinstalled on a GitHub-hosted Windows runner.

**Why:** Qt's entry-point library and the runner's newer MinGW runtime can be ABI-incompatible, producing an unresolved `__imp___argc` linker error even though qmake configuration and compilation otherwise succeed.

**How to apply:** Install the matching Qt tool through `jurplel/install-qt-action` and place it first on `PATH`. For the Qt 6.8 MinGW distribution used here, install `tools_mingw1310,qt.tools.win64_mingw1310`; if the Qt distribution changes, verify its matching toolchain instead of assuming this exact version.