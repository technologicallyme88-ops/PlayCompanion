#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/dungeonrun-tests"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 \
  ../../src/apps_local/dungeonrun/DungeonRunCore.cpp test_dungeonrun.cpp -o "$BUILD_DIR/test_dungeonrun"
"$BUILD_DIR/test_dungeonrun"
