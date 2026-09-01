---
name: Image tileset creation transactions
description: Safety boundary for workflows that create native tileset files and declarations together.
---

New tileset workflows must prepare final data before persistence, save assets
before declarations, verify every declaration write, and register the tileset
only after both are complete. Failures before registration must restore
declaration lengths and remove only directories created by that attempt.

**Why:** Native project declarations can make a ROM build reference missing or
default assets if creation exposes a tileset before all writes succeed.

**How to apply:** Any future importer or generator that creates a tileset must
use the checked project creation path rather than calling ordinary creation and
overwriting assets afterward.