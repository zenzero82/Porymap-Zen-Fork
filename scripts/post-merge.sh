#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Native Porymap has no package-install or database-migration step. Reuse the
# idempotent validation build so merged changes are compiled and its focused
# native tests run before workflows are reconciled.
./scripts/replit-build.sh