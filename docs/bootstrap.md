# qt-rsync — new-session bootstrap prompt

## 0. How to use this

Copy this prompt verbatim into a **fresh session** whose working directory is
`/home/user/Develop/src/qt-rsync`. It contains everything needed to continue
development without access to the earlier qpy-rsync sessions. The canonical
copy is committed as `docs/bootstrap.md` so it survives reboots and /tmp
cleanup.

## 1. Project identity and constraints

- **qt-rsync** is a pure **Qt 6 / C++20 Widgets** graphical frontend for
  `rsync`; it is a direct port of **qpy-rsync** (PySide6/Python) at
  `/home/user/Develop/src/qpy-rsync`.
- Single source of truth: one C++ core decision store / comparison dataset;
  the Qt item models only project it. No Python runtime, no QML, no `.ui`
  files, no second internal state, no Python↔C++ bridge.
- Version: `0.2.1` (tag `0.2.1`, `CMakeLists.txt`, `debian/changelog`).
  qpy-rsync is `0.4.1` (tag `0.4.1`, branch `main`).
- **Never push** unless the user explicitly approves. All work is committed
  locally on `master`.
- **Do not repack `.deb` files on every iteration** — package rebuilds only at
  release milestones, when the user asks.
- qpy-rsync is the **behavioural oracle** for porting: compare outcomes for
  the same fixture, not test structure. Test names must remain neutral
  (no Python/C++ implementation details in the name).
- Settings/state must be plain human-editable INI values — **no b64 anywhere**.
  No settings/profile import/migration from qpy into Qt. Profiles stay JSON.
- Recent git state (at the time this prompt was written):
  - `master` contains release 0.2.1 (`0ee3800`) fast-forwarded by the split
    prep: `c0bfd79` readability comments, `9663332` minimal docs/code map,
    then README enhancements and this bootstrap document. Ahead of
    `origin/master`; clean; **not pushed**.
  - `master-b4-split` at `0ee3800` (tag `0.2.1`) — snapshot of master before
    the separation work; `master-split` was fast-forwarded into master and
    deleted.
  - qpy `main` at `a77134f` (release 0.4.1, tag `0.4.1`), clean.
  - Release packages built: `/home/user/Develop/src/qt-rsync_0.2.1-0_amd64.deb`
    and `/home/user/Develop/src/qpy-rsync_0.4.1-0_amd64.deb`.

## 2. Commands

```sh
# Build (no Python involved)
QT_RSYNC_BUILD_DIR=/home/user/Develop/src/qt-rsync/build ./build.sh

# Build + full QtTest suite (offscreen, no X server needed)
QT_RSYNC_BUILD_DIR=/home/user/Develop/src/qt-rsync/build ./test.sh

# Single test / list functions
QT_QPA_PLATFORM=offscreen ./build/tests/ui_tests <FunctionName>
QT_QPA_PLATFORM=offscreen ./build/tests/ui_tests -functions
```

- Toolchain: system Qt 6.10.2, GCC 15.2, CMake ≥3.20, Ninja. C++20, AUTOMOC,
  AUTORCC, AUTOUIC **off**. GUI tests use `QT_QPA_PLATFORM=offscreen`.
- User runs the app from an NFS mount `~/Develop/src-nuc7.remote` mapped to
  local `~/Develop/src`. Stale binaries were once caused by NFS caching; if a
  newly built binary appears old, remount with
  `mount -o remount,nofacache,noac`.
- qpy reference suite (never run inside Qt work unless asked):
  `cd /home/user/Develop/src/qpy-rsync && QT_QPA_PLATFORM=offscreen PYTHONPATH=src /usr/bin/python3 -m pytest tests/unit -q -p no:cacheprovider`
- X11 (needed only for interacting with the real GUI): display `:50` or `:0`
  via `DISPLAY=... xdotool ...`; offscreen is enough for all automated tests.

## 3. Architecture and code map

### Read this first

`docs/code-map.md` is the authoritative file/class/function index. Highlights:

- `src/main.cpp` → `app::run` (`src/app/Application.cpp`) owns QApplication setup,
  CLI parsing, tooltip proxy style, window icon, then `MainWindow`.
- `src/core/` is pure domain logic: `Parser` (rsync lines), `Command` (argv,
  SSH/setsid), `Filter` (rules), `Profile` (endpoints/validation/fingerprint),
  `Decision` (single decision store), `Plan` (sync rules/summaries),
  `Storage` (settings + JSON profiles + locks), `ExternalTools`, `Perf`.
- `src/model/DataStore.h` is a 1600-line **header-only** store lifted from
  qpy's native experiment (`DataStore.cpp` is a stub). It deliberately keeps
  `snake_case`; do not "fix" its style. `TableModel` (one per pane) and
  `OverviewModel` project the one store. `loadChanges()` is the core→view bridge.
- `src/ui/MainWindow.cpp` is the application controller (pimpl `Private`). Its
  constructor is ~1100 lines; use `docs/code-map.md` to navigate by section.
  Compare/verify/sync flows, journal, preview, progress, profiles, settings
  debounce and all QShortcuts live here.
- `src/ui/ComparisonView.cpp` owns three linked `QTableView`s and decision
  handling; `ActionFilterBar.cpp` owns view-only filter checkboxes and
  Alt/Ctrl+1..9; `OverviewPane.cpp` + `StatsBar.cpp` own the folder overview;
  `NativeDelegate/DecisionDelegate/TreeDelegate` paint trees and checkboxes;
  dialogs: Settings, ProfileEditor, Sync, UseAsLocation, LockedProfile,
  FilterEditorWidget; `Icons.cpp` builds procedural and themed icons.
- Tests: `tests/test_core.cpp` (53) and `tests/test_ui.cpp` (76) plus `install`.

### Key architecture patterns

- ADR-002 single source of truth: `core::Decisions` + `model::DataStore`; views
  are projections. `ComparisonView::syncDecisionsToStore()` mirrors decisions.
- Qt custom roles: `UserRole` = path, `+20` share, `+21` depth, `+22` collapsed,
  `+23` isDir, `+24` decision.
- Window-level `QShortcut`s duplicate menu actions because KDE exports menu
  actions to a global menu; menu QActions intentionally have empty shortcuts.
- Settings are two INI backends split by `Settings::isConfigKey()` prefix;
  writes are debounced 1 s via `queueSetting()`/`flushSettings()`.
- Layout modes `normal | focus | wide` reparent the overview between splitters
  and persist per-mode state.

## 4. Remaining parity / tasks needing qpy context

### Intentional differences / explicit N/A — do not "port" these

- No headless CLI: qpy `cli.py` JSON compare output and rsync exit propagation
  are N/A (ADR-005).
- No process-runner abstraction or askpass test layer: Qt uses `QProcess`
  directly (ADR-008). Note: this also means no headless remote guard — see real
  gap #8 below.
- No qpy settings/profile import, no base64, no `DataStore::load_binary()` QPYN
  dump; no dual Python/native view or `QPY_PYTHON_VIEW` switch.
- Byte-size columns 9/10 hidden by default in Qt.
- Narrow saved-header restoration and pixel-exact profile-editor screenshots
  replaced by widget round-trips.
- `QPY_RSYNC_PERF_LOG` is qpy-only; Qt writes timings to session log/stderr.
- Recent profiles ordered by mtime in Qt, by access time in qpy.

### Genuine unported behaviour (each needs qpy source context)

Ordered roughly by user impact. Qt file names are where the change belongs.

1. **Backup directory inspection** — qpy `services/backup_inspection.py`
   (`inspect_backup_directory`) + `test_backup_inspection.py` (5 cases):
   tri-state `nonempty` (disabled/missing/empty/non-empty/file-instead-of-dir/
   remote unchecked), status message with "may replace older files", warning
   logged once without the status prefix. Qt: new `src/core/BackupInspection.*`
   + `MainWindow.cpp` status update + QtTest in core/UI.
2. **Diff tool resolution** — qpy `services/diff_tool_service.py`
   (`resolve_diff_tool`, `_git_config`, `_SUPPORTED_TOOLS`) +
   `test_diff_tool_service.py`: honour `diff.guitool`, `diff.tool`,
   `difftool.<name>.path`, then meld/kdiff3/kompare/vimdiff, with actionable
   errors. Qt: `MainWindow.cpp::openDiff` + core helper + test.
3. **Inventory advanced flags** — qpy `backend/inventory.py`
   (`gather_inventory(two_pass=...)`, `gather_source_paths`) +
   `test_inventory.py`/`test_inventory_service.py`: `advanced/dest_metadata_two_pass`
   and `advanced/inventory_preload` change listing strategy and caching. Qt's
   SettingsDialog exposes both keys but `MainWindow.cpp` never reads them —
   wire into `startDestinationInventory()` / `startInventory()`.
4. **Strict rsync parse contract** — qpy `backend/rsync_parser.py`
   (`parseRecord`, `RsyncParseError`) + `test_rsync_parser.py` (bad size, broken
   11-char itemized code, separator token in filename are fatal; deletion
   placeholder size/time suppressed; transfer marker wins over timestamp-only
   metadata). Qt `src/core/Parser.cpp` currently drops bad rows — add
   differential cases.
5. **Verification gating** — qpy `main_window.py::_validate_selected_plan` +
   `services/plan_validation_service.py` + `test_execution_gate.py`: by default
   reuse the reviewed comparison; a second rsync runs only when *both*
   `verification/enabled` and `verification/rsync` are set; mismatch appears in
   a scrollable dialog that explicitly says sync is not blocked. Qt
   `MainWindow.cpp::verifyPlan/onOperationFinished` always launches rsync and
   ignores `verification/rsync`.
6. **Overview context menu "Use as Source/Destination"** — qpy
   `test_use_as_location.py`: only directory rows offer it. Qt
   `OverviewPane.cpp::showFolderMenu` lacks the action; `MainWindow::useAsLocation`
   already exists — add signal + action + test.
7. **Sync confirmation dialog details** — qpy `gui/sync_confirmation.py` +
   `test_sync_confirmation.py`: heading/details change when verification ran;
   vertical splitter between command/rules; stats table ignores wheel. Qt
   `SyncDialog.cpp` has fixed boxes and no `verified` parameter.
8. **Headless/askpass guards** — qpy `backend/ssh_askpass.py::askpass_environment`
   + `test_ssh_askpass.py`: missing askpass helper is an actionable error, no
   `DISPLAY`/`WAYLAND_DISPLAY` refuses remote before rsync, `QT_QPA_PLATFORM`
   not forwarded. Qt `Command.cpp::processEnvironment` silently omits. This is
   a real hang-prevention gap despite being declared N/A.
9. **Metadata watchdog** — qpy `test_metadata_watchdog_clears_the_comparing_status`:
   60 s watchdog restores "Comparison complete" if destination metadata hangs.
   Qt has no watchdog on the metadata process.
10. **Missing local directory checks** — qpy `test_compare_refuses_missing_local_directories`,
    `test_inventory_refuses_missing_local_directories`,
    `test_startup_warns_about_missing_remembered_locations`,
    `test_open_missing_directory_shows_a_warning`: exact warn vs refuse wording
    in startup, `startCompare`, `startInventory`, open-button handlers.
11. **Tool placeholder warning** — qpy `gui/tools_panel.py::ToolDialog._check_command`
    + `test_tools_panel.py`: live "Unknown placeholder(s): ..." label. Qt
    `SettingsDialog.cpp::ExternalToolDialog` has the help text but not the label.
12. **Execution gate state machine** — qpy `core/execution_gate.py::ExecutionGate`
    + `test_execution_gate.py`: `InvalidGateTransition`, verified→confirm→execute
    path, gate log field. Qt uses booleans (`stale`, `verified`, `planDirty`).
    Only port if gating diagnostics parity is wanted.
13. **Session log timestamp** — qpy `test_session_log_uses_local_time`: full ISO
    timestamp with local UTC offset. Qt `MainWindow.cpp::appendJournalLine` uses
    `QTime::currentTime()` (`HH:mm:ss.zz`, no date/offset).
14. **Settings section API** — qpy `gui/settings_dialog.py::show_section` +
    test: open on a requested tab with fallback to tab 0. Qt has no such API.
15. **Profiles directory creation at startup** — qpy
    `test_profiles_directory_is_created`; Qt does not create it eagerly.
16. **Empty action colour** — qpy `test_an_empty_or_missing_colour_key_means_unmarked`:
    Qt `MainWindow.cpp::applyActionColours` honours empty, but reopening
    `SettingsDialog` maps it back to the default colour.

### Implemented in Qt but with no Qt test (qpy test is the oracle)

- `test_execution_plan.py`: incremental fingerprint equals full rebuild;
  deleted size includes reported file sizes without metadata.
- `test_sync_service.py::test_known_exit_explanation`: `core::explainRsyncExit`
  exists but `MainWindow::onOperationFinished` never uses it (wiring gap).
- `test_sync_progress_parser.py`: `ir-chk` incremental counter, summary-only
  journal (Qt tests only cover `to-chk`).
- `test_compare_service.py`: crashed run reported as error, transfer progress
  kept out of journal, parse timing logged on finish.
- `test_filter_editor_delegate.py`: line-number gutter repaints long lines,
  rule preview defaults to two lines.
- `test_profile_editor.py`: rsync-option tooltips, backup tab browse/open buttons.
- `test_settings_dialog.py`: command preview does not grow with long commands.
- `test_icons.py` (3): themed icon preference has no dedicated Qt test.
- qpy's new `test_undo_button_follows_decision_history` — Qt has the Undo button
  but only asserts the menu QAction.
- `test_flat_filtering.py`: unknown destination size blank not zero, search
  matches cached and following visible rows, changed decisions reviewable.

### qpy test modules with no Qt test at all

N/A by design: `test_cli.py`, `test_process_runner_contract.py`,
`test_process_environment.py`, `test_perf_log.py`, `test_hybrid_parity.py`,
`test_native_hybrid.py`, `test_sync_smoke_runner.py`, `test_sync_test_scripts.py`,
`test_ssh_askpass.py` (but see gap #8).

Real coverage gaps: `test_backup_inspection.py`, `test_execution_gate.py`,
`test_compare_service.py`, `test_inventory_service.py`, `test_sync_service.py`,
`test_action_colours.py`, `test_plain_settings.py`, `test_profile_store.py`,
`test_convert_profiles.py`, `test_filter_settings.py`, `test_backup_settings.py`,
`test_models.py`, `test_workflow_state.py` (the latter group is covered
indirectly by storage/core cases, not dedicated round-trips).

### qpy technical details available only in the qpy checkout

- `docs/design.md` — full product spec: endpoint/SSH + askpass discovery (§5),
  filters and "Delete on Destination" (§6), backups (§7), rsync options (§8),
  **profile JSON and all settings keys/formats (§9)**, journal model (§10),
  external-tool placeholders and diff-tool resolution (§11), security (§13).
- `docs/debugging.md` — `QPY_RSYNC_DEBUG`, probes, `QPY_RSYNC_PERF_LOG`, the
  `|qpy-rsync|` field-separator ambiguity rule.
- `native-hybrid/` and `native-qt-model/` — source of the lifted C++
  `DataStore.h`, measurements and why nanobind bindings were dropped.
- Removed-from-Qt-but-still-in-qpy research: `docs/performance-analysis.md`,
  `plan-profiling.md`, `python-tuning-plan.md`, `cpp-extension-analysis.md`,
  `cpp-qt-conversion-analysis.md`, `roadmap.md`, `installation.md`.
- `tools/SYNC-SMOKE-TESTS.md` and `tools/run-sync-smoke-test.sh` — real-tree
  remote sync safety procedure referenced by Qt's `docs/testing.md`.
- `src/qpy_rsync/services/` and `backend/process_runner.py` — semantics with no
  Qt equivalent layer; needed for service-level parity and process contracts.

## 5. Documentation set and cleanup rationale

### Retained (minimal set)

- `docs/code-map.md` — file/class/function lookup guide (new).
- `docs/decisions.md` — ADR-001/002/003/005/008 in force; ADR-004/006/007 are
  one-line "resolved" notes; ADR-006 records qt-rsync-native XDG paths with no
  qpy import and no base64; ADR-008 records direct QProcess + fake-rsync tests.
- `docs/testing.md` — commands, oracle strategy, current 129-case status,
  deliberate N/A differences, timing diagnostics, remote smoke safety.
- `README.md` — user guide with CLI example, Files and locations, Remote
  hosts, Development and debugging, Troubleshooting, a "Differences from
  qpy-rsync" section, and a documentation table pointing to qpy-rsync's
  `design.md`, `installation.md`, `debugging.md`, `performance-analysis.md`,
  `native-hybrid/README.md` and `native-qt-model/CONCLUSION.md`.
- `docs/bootstrap.md` — this prompt; the hand-off for a new session.
- `CHANGELOG.md` — full change history.

### Deleted as historical/obsolete (15 files)

Byte-for-byte or near copies of qpy-rsync research that no longer applies to a
shipped pure-C++ port: `cpp-extension-analysis.md`,
`cpp-qt-conversion-analysis.md`, `native-Qt-model-experiment-plan.md`,
`native-qt-model-conclusion.md`, `hybrid-view-notes.md`, `hybrid-view-plan.md`,
`performance-analysis.md`, `plan-profiling.md`, `python-tuning-plan.md`,
`roadmap.md`. Completed/stale qt plans: `port-plan.md` (wrong case counts and
"remaining work"), `test-port-plan.md` (batch history; its N/A list and rules
were merged into testing.md). Stale one-off artefacts: `FIRST-REMOTE-RUN.md`
(claimed qpy settings were imported — explicitly false), `cli.md` (merged into
README), `design.md` (old Python spec; newer qpy version is authoritative).

All of those can be read in the qpy-rsync checkout if ever needed. Full rationale
is in the commit that performed the cleanup.

## 6. Conventions a maintainer must preserve

- qpy is the behavioural oracle; port one representative case per C++ rule,
  assert identical outcomes for the same fixture, use neutral names.
- Profiles are JSON; settings/state are plain INI scalars — no b64, no qpy
  import/migration. `--dir-pair`, `--backup`, `--mode`, `--compare`,
  `--log-file`, `--no-log-file`, `--version`, `--help` are the CLI.
- Filter checkboxes are view-only; they never change the plan or rsync command.
- Ctrl+1 inverts "only excluded"; Ctrl+S/D/N/W focus panes; Ctrl+[/] focus the
  location edits; Alt/Ctrl+1..9 are application-wide filter shortcuts;
  Ctrl+G/Ctrl+Shift+G step search results.
- Build/test with the commands in §2; commit locally after every change; update
  CHANGELOG, debian/changelog and the qpy case count in testing.md when needed.
- Never push; never repack .deb files except at release milestones.
- Code-map and testing.md are the only docs that maintainers need day to day.

## 7. Where to look in qpy-rsync when porting behaviour

qpy checkout: `/home/user/Develop/src/qpy-rsync` (branch `main`, at `a77134f`, tag `0.4.1`).

- `src/qpy_rsync/gui/main_window.py` — application controller; compare/verify/
  sync flows, journal, progress, profile prompts, shortcuts, stale state.
- `src/qpy_rsync/gui/flat_comparison_view.py` — three linked panes, search,
  folding, context menus, per-pane models and delegates.
- `src/qpy_rsync/gui/flat_change_model.py` — Python view model (the C++
  `DataStore.h` corresponds to the native-hybrid model, not this one).
- `src/qpy_rsync/gui/action_filters.py` — filter checkboxes and inversion.
- `src/qpy_rsync/gui/overview.py`, `stats bar` — overview semantics.
- `src/qpy_rsync/backend/{rsync_command.py,rsync_parser.py,rsync_filters.py,
  decision_filters.py,execution_plan.py}` — core rule equivalents of
  `src/core/{Command,Parser,Filter,Plan}.cpp`.
- `src/qpy_rsync/storage/{settings.py,profile_store.py,plain_state.py}` —
  settings/profile formats; `native-hybrid/` is the source of the lifted C++
  model/view code and holds the historical measurements.
- `tests/unit/` — the behavioural oracle; `test_hybrid_parity.py` documents
  pure-Python vs native-view equivalence and is the best guide for the C++
  store. Never copy Python test structure literally.

## 8. Session hand-off checklist

- Before editing, confirm both trees are clean with `git status --short`.
- After any source change: build, run `./test.sh`, commit locally with a clear
  message, update `CHANGELOG.md` / `debian/changelog` / `docs/testing.md`
  counts when behaviour or test counts change.
- Do not push; do not repack Debian packages unless the user asks.
- When a behaviour is missing, inspect qpy first (`/home/user/Develop/src/qpy-rsync/src/qpy_rsync/...`),
  then implement the Qt equivalent and add a QtTest case named for the
  behaviour, not the implementation.
