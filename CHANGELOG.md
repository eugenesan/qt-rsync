# Changelog

## Unreleased — phase 3: verify and sync

- Added Verify (F8): runs the compare dry run with the temporary decision
  filter file and reports whether the output matches the execution plan.
- Added Synchronize (F9): confirmation dialog with command preview, decision
  rules, counts and destructive-operation warning; runs a write-capable rsync
  with `QProcess`.
- After a successful sync the comparison is marked stale and is never
  refreshed automatically; the user must press Compare.
- Added a fake-rsync UI test covering compare → verify → confirm → sync →
  stale state.
- Added a timestamped journal, rsync progress bar and benign Qt-output
  filtering for captured process output.

## Unreleased — phase 2: first vertical slice

- Lifted the proven native view layer: `DataStore`, `TableModel` and
  `OverviewModel` now live in `src/model/DataStore.h` (header-only, no Python
  bindings, no `.so` handoffs).
- Ported the rsync itemized-output parser with strict record validation and
  destination-only vs delete semantics.
- `ComparisonView` now renders three synchronized panes over one DataStore;
  `OverviewPane` builds the folder tree from the same store.
- `MainWindow` runs a real Compare with `QProcess` using the ported command
  spec, parses stdout, populates the views and reports progress/status.
- Added a QtTest end-to-end check with a fake rsync executable: 2 test targets,
  43 assertions total.
- Added row context menus (copy path/name, Exclude via Filter, Delete on
  Destination with permanent-filter and decision-only options), a column
  header editor, and the four settings pages wired to the XDG settings split.
- Ported the decision-column delegate, Space handling for multi-row selection,
  linked sorting, synchronized selection/scrollbars and the statistics strip.
  Decisions now live only in `core::Decisions`; the view store mirrors them
  for painting, and a UI test verifies three selected rows are excluded at
  once.

## Unreleased — phase 1: core semantics

- Ported the decision model to C++: INCLUDE / EXCLUDE / INHERIT / DELETE,
  inheritance, defaults, reconcile (including type-aware preservation and
  opt-in inherited-destination-only detection), summaries and conflict
  handling. 11 ported cases.
- Ported filter parsing, context-menu phrase generation (`-s` delete rules),
  rsync pattern matching and benign Qt-output cleanup.
- Ported execution plans: item/summary construction, synthetic deletes,
  fingerprints, freshness checks and decision filter-rule generation.
- Ported rsync command construction: compare/verify/sync/inventory specs,
  delete timing, filter merge ordering, nested-backup protection and
  SSH remote-shell handling.
- Ported profiles and storage: JSON profile round-trip with legacy migration,
  XDG settings split, PID-based locks and current-profile caching.
- QtTest suite: 41 tests passing under `./test.sh`.

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
- `docs/port-plan.md` with the phased port and exit criteria, `docs/decisions.md`
  with the up-front architecture decisions, `docs/cli.md`, and the inherited
  qpy-rsync specification, performance and experiment reports.

### Exit criteria met

- `./build.sh` builds `bin/qt-rsync`.
- `qt-rsync --help` and `qt-rsync --version` print and exit successfully.
- `ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure` passes.
