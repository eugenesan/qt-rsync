# Code map (qt-rsync)

This is a lookup guide, not a specification. For the product spec and the
research behind the port, see the qpy-rsync repository; for build/test commands
see `testing.md`; for why the code is structured this way see `decisions.md`.

## Quick index — "where do I change X?"

| Task | Start here |
|---|---|
| Add/parse an rsync startup option | `src/app/StartupOptions.cpp` (`StartupOptions`, `setupParser`) |
| Change tooltip behaviour or app identity | `src/app/Application.cpp` (`installTooltipStyle`, `run`) |
| Change what one itemized rsync line means | `src/core/Parser.cpp` (`parseRecord`, `parseOutput`, `parseInventory`) |
| Change compare/verify/sync/fetch command line | `src/core/Command.cpp` (`buildCompareSpec`, `buildVerifySpec`, `buildSyncSpec`, `buildFetchSpec`) |
| Change SSH, askpass or `setsid` handling | `src/core/Command.cpp` (`processEnvironment`, `processSpec`) |
| Change filter rule parsing or generated rules | `src/core/Filter.cpp` (`parseAdvancedRule`, `filterArguments`, `phrasesFor`, `patternMatches`) |
| Change endpoint syntax, sync mode or validation messages | `src/core/Profile.cpp` (`parseEndpoint`, `validateProfile`, `SyncProfile::fingerprint`) |
| Change decision semantics / reconcile | `src/core/Decision.cpp` (`Decisions::reconcile`, `effective`, `set`, `resetSubtree`) |
| Change execution plan or sync rules | `src/core/Plan.cpp` (`buildExecutionPlan`, `decisionFilterLines`) |
| Change settings keys, profile JSON, locks or XDG paths | `src/core/Storage.cpp`, `src/core/Paths.cpp` |
| Add an external-tool placeholder | `src/core/ExternalTools.cpp` (`expandPlaceholders`, `placeholderValues`) |
| Change a row's visible columns, roles or sort | `src/model/DataStore.h` (`TableModel::data`, `sort_visible`) → `src/ui/ComparisonView.cpp` (`sortSectionFor`) |
| Change filtering/folding/aggregates | `src/model/DataStore.h` (`rebuild_visible`, `compute_aggregates`, `folded_out`) |
| Change overview totals or folder nodes | `src/model/DataStore.h` (`OverviewModel::build`) → `src/ui/OverviewPane.cpp` (`rebuild`) |
| Change a compare/verify/sync workflow step | `src/ui/MainWindow.cpp` (`startCompare`, `onCompareFinished`, `verifyPlan`, `synchronize`, `onOperationFinished`) |
| Change journal text, command preview, progress, stale state | `src/ui/MainWindow.cpp` (`appendJournalLine`, `setCommandPreview`, `setStatus`, `setStale`) |
| Change keyboard shortcuts | `src/ui/MainWindow.cpp` (`QShortcut` block in the constructor; menu actions there are intentionally shortcut-free) and `src/ui/ActionFilterBar.cpp` (Alt/Ctrl+1..9) |
| Change search stepping | `src/ui/MainWindow.cpp` (`refreshSearchMatches`, `stepSearch`, `findNext`, `findPrevious`) |
| Change a pane delegate/paint/decision checkbox | `src/ui/NativeDelegate.cpp`, `src/ui/DecisionDelegate.cpp`, `src/ui/TreeDelegate.cpp` |
| Change a dialog | `src/ui/{SettingsDialog,ProfileEditorDialog,SyncDialog,UseAsLocationDialog,LockedProfileDialog}.cpp` |
| Change an icon | `src/ui/Icons.cpp` (`ui::icons::toolbarIcon`, `actionIcon`) |

## Build targets and dependencies

- `qt_rsync_core` — all `src/core/*.cpp`; depends on QtCore only (plus STL).
- `qt_rsync_model` — `src/model/DataStore.cpp`; depends on core + Qt Widgets.
  Note: `DataStore.cpp` is a 6-line namespace stub — **all model logic is inline
  in `src/model/DataStore.h`** (lifted from qpy's native experiment).
- `qt_rsync_ui` — all `src/ui/*.cpp`; depends on model + core.
- `qt-rsync` — `src/main.cpp`, `src/app/*.cpp`, `resources/resources.qrc`.
- Tests: `tests/test_core.cpp` (core + model), `tests/test_ui.cpp` (ui + model +
  core + app), CTest targets `core`, `ui`, `install`.

## Subsystem map

### 1. App bootstrap (`src/main.cpp`, `src/app/`)

- `main()` is a five-line trampoline into `app::run`.
- `app::run(argc, argv)` — creates `QApplication`, sets org/app name and version
  (`QT_RSYNC_VERSION` + embedded git commit), installs the window icon, parses
  startup options, removes obsolete encoded settings, shows `MainWindow`, enters
  `exec()`. It deliberately does not force the Fusion style (comment in
  `Application.cpp`).
- `app::installTooltipStyle()` — `QProxyStyle` forcing a 250 ms tooltip wake-up
  and 0 ms fall-asleep grace.
- `struct StartupOptions` / `StartupOptions::parse()` — splits Qt platform/style
  switches from app switches, validates `--mode` and the `--compare` pair,
  exposes `qtArguments` for pass-through.

### 2. Core (`src/core/`)

| File | Key symbols |
|---|---|
| `Change.h/.cpp` | `enum class Action/Kind`, `struct Change` (path, action, kind, itemized code, sizes, times, link target, reason), `isDeletion()`, `isDestinationOnly()`. Data-only; `.cpp` is intentionally empty. |
| `Decision.h/.cpp` | `enum class Decision {Inherit, Include, Exclude, Delete}`, `DecisionSummary`, `class Decisions` — the single source of truth (ADR-002). `reconcile`, `effective`, `effectivePath`, `excludedPaths`, `summary`, conflict API, snapshots. Perf-motivated algorithms (`inherited`, ancestor index in `reconcile`) have comments; do not remove them. |
| `Parser.h/.cpp` | `parseRecord` (`|qpy-rsync|` itemized line), `parseInventory`, `parseOutput`, `parseSyncProgress`, `looksLikeItemizedRecord` (cheap live-count check), `isRsyncSummaryLine`. |
| `Command.h/.cpp` | `ProcessSpec::argv()`, `buildCompareSpec`, `buildVerifySpec`, `buildSyncSpec`, `buildInventorySpec`, `buildFetchSpec`, `explainRsyncExit`; helpers `outFormat`, `commonArguments`, `processEnvironment` (SSH_ASKPASS, display forwarding), `remoteShellArgument`, `syncArguments`. Remote rsync runs under `setsid --wait` (ADR-008). |
| `Filter.h/.cpp` | `FilterRule`, `FilterPhrase`, `parseAdvancedRule`, `validateRule`, `filterTextToRules`, `filterArguments`, `filterFileLines`, `phrasesFor`/`deletePhrasesFor`, `patternMatches` (`segment-wise glob incl. `**`), `cleanQtNoise`. |
| `Profile.h/.cpp` | `Endpoint`, `SyncProfile` (`fingerprint` excludes identity fields), `normalizePosixPath`, `destinationBackupDirectory`, `backupProtectionArguments`, `parseEndpoint`, `validateProfile` (returns coded messages the UI matches on, e.g. `endpoints.two_remote`). |
| `Plan.h/.cpp` | `buildExecutionPlan`, `PlanItem`, summary counters, `decisionFilterLines`, fingerprints/freshness for verify and sync. |
| `Storage.h/.cpp` | `FileLock` (PID file, stale takeover), `Settings` (two INI backends via `isConfigKey()` prefixes — config: `logging/ sound/ verification/ decisions/ advanced/ tools/ colours/`, state: `main-window/*` etc.), `profileToJson/profileFromJson` (JSON profiles, legacy migrations), `saveProfile/loadProfile/listProfiles` (newest mtime first, max 10), `removeObsoleteEncodedSettings`, `encodeIntList/decodeIntList` (geometry/splitters). **No b64, no qpy import.** |
| `ExternalTools.h/.cpp` | `loadExternalTools/saveExternalTools` (`tools/external/count`, `tools/external/N/...`), `placeholderValues`, `expandPlaceholders`, `unknownPlaceholders`, `buildToolCommand` (quote-aware), `itemContext`. |
| `Perf.h` | `ScopedPerf` RAII probe, `perfFlag`, opt-in via Settings → Advanced. Phases named `ui.setChanges.*`, `ui.decision.*`, `addDirs.*`, `overview.build.*`, `compare.*`. |

### 3. Model (`src/model/DataStore.h`)

The lifted native store deliberately keeps `snake_case` and plain enums — this
is a ported boundary, not a style inconsistency to "fix" casually.

- `struct DataStore` — columnar arrays (`paths`, `actions`, `kinds`, `sizes`,
  `times`, `decisions`) plus derived view state (`parents`, `depth`, `synthetic`,
  `collapsed`, `folded_*`, `visible`, `row_of_id`, `dir_items/dir_size`,
  `dest_metadata`, icon caches).
  - Load/structure: `generate` (test fixture), `load_direct`, `append_direct`,
    `derive_structure`, `derive_text`, `add_directories` (synthesized parents +
    virtual root), `truncate`.
  - Visibility pipeline: `rebuild_visible` (ancestor retention → filters →
    only-excluded fast path → tree order → sort + faded directories →
    aggregates), `matches`, `action_allowed`, `search`, `sort_visible`,
    `update_single_visibility` (fast path for one decision edit), `folded_out`.
  - Aggregates: `compute_aggregates`, `compute_destination_counts`,
    `apply_destination_metadata`, `compute_stripes`.
  - Decisions: `set_decision_id`, `toggle_decision`, `decision_log`,
    `g_decision_hook` bridge to `ComparisonView`.
  - `loadChanges(DataStore&, const QVector<core::Change>&)` — core→view bridge.
- `class TableModel : QAbstractTableModel` — one instance per pane
  (0 Source, 1 Decisions, 2 Destination). Roles: `UserRole` = path, `+21` depth,
  `+22` collapsed, `+23` isDir, `+24` decision; `CheckStateRole` for the decision
  box; `DecorationRole`, `ToolTipRole`, `ForegroundRole`. Columns 0/5 name,
  1/6 items, 2/7 size, 3/8 time, 4 decisions, 9/10 byte sizes (hidden by
  default).
- `class OverviewModel : QAbstractItemModel` + `OverviewNode` — folder tree with
  synthesized "Files" and "Total" rows; `build(store, visible_only)`, `sort`,
  `index_for_path`. Filtering inside `build` uses `counts_as_item()`.

### 4. UI (`src/ui/`)

- **`MainWindow.h/.cpp`** — application controller with pimpl `Private* d_`.
  Constructor builds toolbar, menus, locations/filter bar, splitters, status
  bar, four `QProcess`es and their stdout handlers, window-level `QShortcut`s
  (duplicated because KDE exports menu actions to a global menu), settings
  restore, and ~15 view-signal connections. Key groups: compare/inventory,
  plan/verify/sync, profile Save/Save As/recent, filters/tools/diff, journal,
  preview, progress, stale state, layouts, settings persistence (1 s debounce).
- **`ComparisonView.h/.cpp`** — three `QTableView`s over one store and one
  `core::Decisions`, with linked selection/scroll (ADR-002). Entry points:
  `setChanges`, `applyDecision`, `syncDecisionsToStore`, `undoDecision`,
  `applyFilters`, `setShowUnchanged`, `setStale`, `selectPath`, `focusPane`,
  `jumpToEdge`, `moveHierarchy`, `handleCollapseKey`, `foldCurrent/foldAll`,
  `searchMatches`, `showContextMenu`, `showHeaderMenu`. Signals include
  `decisionChanged`, `undoAvailable`, `filterRequested`, `toolRequested`.
- **`ActionFilterBar.h/.cpp`** — view-only checkboxes (never change the plan):
  excluded, five action filters, unchanged, directories, conflicts, Undo button;
  Ctrl+click inversion (`applyInverse`) and Alt/Ctrl+1..9 shortcuts; context
  menus for boxes and excluded.
- **`OverviewPane.h/.cpp`**, **`StatsBar.h/.cpp`** — folder tree, share bar,
  stats footer; signals for folder activation, exclude/restore/filter requests.
- **Delegates** — `NativeDelegate` (folder chevrons, icons, stripes, action
  colours, decision box), `DecisionDelegate` (decision glyph + Space-key batch
  filter), `TreeDelegate` (chevron/indent for name columns).
- **Dialogs/editors** — `SettingsDialog` (four tabs, plain INI settings),
  `ProfileEditorDialog` (Options/Preservation/Backup/Filters), `SyncDialog`
  (confirmation, fingerprint, rules, `compareAfterSync`), `UseAsLocationDialog`
  (both-sides/cancel + optional filter adaptation), `LockedProfileDialog`
  (retry/temporary copy/quit), `FilterEditorWidget` (rules, highlighter,
  line numbers, validation).
- **`Icons.h/.cpp`** — `ui::icons` namespace; procedural grid icons plus
  themed-icon lookup with `QStyle::standardIcon` fallback.

## Conventions

- Namespaces: `qt_rsync::{app,core,model,ui}` and `core::storage`, `core::paths`,
  `ui::icons`.
- Hand-written code: `PascalCase` types, `lowerCamelCase` functions,
  trailing-underscore members; anonymous-namespace constants use `kPrefix`.
  `DataStore.h` keeps the lifted `snake_case` style.
- `enum class` in core; plain enums in `DataStore.h` (integer values are stored).
- Qt custom data roles: `UserRole + 20` share, `+21` depth, `+22` collapsed,
  `+23` isDir, `+24` decision.
- Event filters: `NavigationKeyFilter`, `DecisionKeyFilter`,
  `ActionFilterBar::eventFilter`, `MainWindow::eventFilter` (Esc ends editing,
  hover shows command preview). Other widgets subclass and override instead.
- Layout modes `normal | focus | wide` reparent the overview between splitters
  and persist `main-window/layout-state-<mode>`.
- Tests use a fake `rsync` executable on `PATH`; no backend abstraction (ADR-008).
  Case names describe behaviour, never implementation language.
