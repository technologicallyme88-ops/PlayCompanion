#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-terminalhack-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 test_terminalhack.cpp -o "$BUILD_DIR/test_terminalhack"
"$BUILD_DIR/test_terminalhack"
