---
name: Replit build validation isolation
description: Why the native build check must remain separate from the desktop Run workflow.
---

Keep the native build validation available as a standalone check, but do not
attach it to the desktop Run workflow.

**Why:** The desktop launcher already performs an incremental build before
starting the Qt application. Attaching the validation to Run launches a second
build against the same qmake outputs, which can race and can also stop or delay
the VNC application during workflow refreshes.

**How to apply:** When updating named validations or Replit workflows, verify
that Run starts only the desktop workflow. Trigger the standalone build
validation explicitly when a clean or CI-style check is needed.