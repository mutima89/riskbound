#!/usr/bin/env bash
# Builds and runs the simulation-core unit tests (no Unreal needed). Optional arg: test name filter.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="${RB_CXX_FLAGS:--fsanitize=address,undefined}" >/dev/null
cmake --build "$ROOT/build" -j
"$ROOT/build/RiskboundCoreTests" "$@"
