#!/usr/bin/env bash
# Bruce Lee: Dragon Warrior - PS Vita port
# Port code Copyright (C) 2026 Leandro Ferrete.
set -e
export VITASDK="${VITASDK:-/c/vitasdk}"
export PATH="$VITASDK/bin:/usr/bin:$PATH"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
rm -rf build
mkdir -p build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
