#!/usr/bin/env bash
# Build qt-rsync with the system Qt 6 toolchain. No Python is involved.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD=${QT_RSYNC_BUILD_DIR:-/tmp/opencode/qt-rsync-build}
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$BUILD"
printf 'built %s/bin/qt-rsync\n' "$BUILD"
