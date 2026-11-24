# Changelog

## Unreleased — correctness pass

- Verify now compares the second dry run's parsed output against the execution
  plan and reports missing/unexpected items; a mismatch marks verification as
  failed.
- Added `verification/enabled` and `verification/rsync` settings. Verification
  remains optional; Synchronize is still available from Selection Ready, as in
  qpy-rsync.
- Added a Cancel button that terminates a running compare, verify or sync
  process and restores the idle state.
- Fixed a QSet aliasing bug in plan mismatch computation.
- Added end-to-end tests for decision-only deletes (the sync dialog shows the
  `-s` rule) and mirror-mode destination-only deletions.

## Unreleased — undo and hierarchy fixes

- Undo now clears the mirrored view decisions before reapplying the snapshot,
  so excluded rows return to the list and the summary updates.
- Decision changes refresh folder aggregates and the visible tree order, and
  unchecked decision boxes are drawn as outlines instead of solid blue blocks.

## Unreleased — real-tree feedback fixes

- Startup with no `--dir-pair` no longer clears the endpoints loaded from the
  cached profile (the SSH source now survives auto-load).
- Added decision Undo (Ctrl+Z and toolbar button). A multi-row Space exclusion
  is one undo step, matching qpy-rsync's decision batching.

## Unreleased — layout parity

- Set explicit column widths close to qpy-rsync: wide Item column, narrow Items
  and Size columns, compact decision pane.
- Suppressed file/folder icons on rows that have no content on that side (the
  blank spacer rows), matching the reference view.

## Unreleased — first real-tree run fixes

- Profile-loaded SSH endpoints keep their `ssh` kind when Compare runs; the
  field now shows the full `user@host:/path` operand. Typed-in SSH operands are
  still parsed into SSH endpoints.
- Normalized trailing slashes from rsync directory paths so folder rows keep
  their names.

## Unreleased — remote-run preparation

- Parse `user@host:/path` endpoints typed into the Source/Destination fields,
  so an SSH profile can be used without loading a profile file.
- Added `docs/FIRST-REMOTE-RUN.md`: exact environment, binaries, cwd, launch
  commands and capture steps for the first side-by-side run on the target
  machine.

## Unreleased — phase 5: parity fixes from real-tree comparison

- Fixed `--dir-pair` so the second operand is captured; local endpoints now
  get a trailing slash so rsync compares directories instead of copying the
  source directory into the destination.
- Set the pane root names from the endpoint names, excluded synthesized folder
  rows from overview totals, and added coloured action glyphs to the decision
  column.
- Added a destination inventory pass after Compare so destination sizes and
  folder aggregates match the Python app.
- Verified side-by-side against qpy-rsync on X11 :50: same 6 changes, same
  source and destination counts/sizes and the same overview totals.
- Added C++-drawn file/folder icons and coloured action icons in the decision
  column (no Python icon generator or cross-language QIcon handoff).
- Verified a larger tree (620 source / 514 destination files) on X11 :50:
  both apps report 106 changes, 2.0 KiB and 106 selected items.
- Added Left/Right keyboard folding and a Collapse/Expand context submenu with
  the same behaviour as the Python/native view.
- Row text now uses the action colours (green create, orange update, brown
  destination-only), matching the reference UI.

## Unreleased — phase 4: profiles and multi-instance

- Added a Profile menu with Save, Save As, Load, recent profiles and Reset.
- Startup restores the cached profile. If it is locked by another process, a
  dialog offers a temporary copy with an editable name, retrying inside the
  same dialog with an orange warning, or quitting without changing settings or
  profiles.
- Temporary copies are not cached as the current profile. Saving under a new
  name promotes them and records the profile.
- Added UI tests for cache restoration and the quit-without-changes path.
- Temporary profiles now own a session log; discarding a temporary profile
  removes its file, lock and logs, while saving under a new name renames the
  logs to the new profile.
- XDG paths are now `qt-rsync` names. A one-time migration copies settings,
  profiles and the cached profile path from the old `qpy-rsync` locations.

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
