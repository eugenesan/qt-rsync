#!/usr/bin/env bash
# Build and run the QtTest suite. No Python, no Qt display required.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD=${QT_RSYNC_BUILD_DIR:-/tmp/opencode/qt-rsync-build}
export QT_QPA_PLATFORM=offscreen
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$BUILD"
ctest --test-dir "$BUILD" --output-on-failure
