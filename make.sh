#!/usr/bin/env bash
# Bruce Lee: Dragon Warrior - PS Vita port
# Port code Copyright (C) 2026 Leandro Ferrete.
set -e
export VITASDK="${VITASDK:-/c/vitasdk}"
export PATH="$VITASDK/bin:/usr/bin:$PATH"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/build"
make -j4 2>&1 | tail -160
