# Changelog

## 0.1.0 — phase 0 scaffold (2026-11-23)

Initial commit of the pure Qt/C++ port of qpy-rsync. No sync functionality is
implemented yet; this release establishes the build, layout, documentation and
the inherited command-line surface.

### Added

- CMake build (C++20, Qt 6 Core/Gui/Widgets/Test, Ninja via `build.sh`) with
  separate `core`, `model`, `ui` libraries and an executable at `bin/qt-rsync`.
- `src/app`: single GUI entry point with the qpy-rsync launcher options
  (`--dir-pair`, `--backup`, `--mode`, `--compare`, `--log-file`,
  `--no-log-file`, `--version`, `--help`) parsed with `QCommandLineParser`.
  There is no second CLI binary.
- `src/core`: C++ type and API skeleton for changes, decisions, filters,
  profiles, execution plans, rsync command specs and XDG path layout. One
  decision store is owned here; the UI will not mirror it.
- `src/model`: `DataStore`, `TableModel` and `OverviewModel` skeletons shaped
  after the proven native-hybrid implementation (structure-of-arrays store,
  Qt item models as projections).
- `src/ui`: empty main window, three-pane comparison widget, overview pane and
  settings dialog. Widgets are constructed in C++; no QML and no `.ui` files.
- `tests`: first QtTest smoke case, runnable with `ctest` under the offscreen
  Qt platform.
- `docs/PORT-PLAN.md` with the phased port and exit criteria, `docs/DECISIONS.md`
  with the up-front architecture decisions, `docs/CLI.md`, and the inherited
  qpy-rsync specification, performance and experiment reports.

### Exit criteria met

- `./build.sh` builds `bin/qt-rsync`.
- `qt-rsync --help` and `qt-rsync --version` print and exit successfully.
- `ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure` passes.
