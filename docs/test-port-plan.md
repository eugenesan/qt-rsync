# qpy-rsync test port tracker

qpy-rsync currently collects 460 pytest cases. qt-rsync has 85 QtTest cases
(44 core, 41 UI) and three CTest targets (`core`, `ui`, `install`). This file
tracks the conversion so a missing behaviour is explicit rather than implied.

## Conversion rules

1. Port the behaviour, not the Python fixture.
2. Prefer direct widget/model construction over synthetic pointer driving.
3. Keep a differential fixture for pure core rules where the qpy result is the
   oracle.
4. Use qpy's neutral test names (`alice`, `server`, `nas`, `example.test`); no
   private hostnames or usernames.
5. A ported row is complete only when the C++ case fails on the old behaviour
   and passes on the fixed one.

## Status by qpy test module

| qpy module | cases | qt-rsync status |
|---|---:|---|
| `test_main_window_offscreen.py` | 95 | Partial: startup, profile cache, locked profile, compare/verify/sync, settings, closing, sync progress/stale state, basic menus and F-key shortcuts are covered. Full journal states and exact shortcut enablement still need cases. |
| `test_flat_comparison_view.py` | 49 | Partial: three panes, selection sync, folding, context menus, navigation keys and stale root visuals are covered. Remaining: all header context actions, exact tooltips and per-pane local/remote menus. |
| `test_flat_filtering.py` | 31 | Partial: action masks, ancestors, unchanged gate and search matching are covered. Remaining: path/glob filters, directory-only mode, inverse clicks and filter persistence. |
| `test_action_filters.py` | 19 | Mostly ported: initial state, display-only tooltips, excluded count, focus policy, separators, ancestor retention, Ctrl+click inversion and the checkbox context menu. Remaining: exhaustive state combinations. |
| `test_command.py` | 13 | Mostly ported: compare/verify/sync argv, modes, filters, delete timing and remote shell. Remaining: exhaustive option combinations. |
| `test_app_startup_options.py` | 12 | Ported at the CLI level: complete argument set, Qt-only arguments, log-file/no-log-file, compare pair and invalid mode. Qt environment/migration cases are N/A in the C++ port. |
| `test_external_tools.py` | 11 | Blocked: qt-rsync stores tool rows but does not implement placeholder expansion/tool launch yet. Either port that feature or mark the module N/A explicitly. |
| `test_changes.py` | 11 | Ported: decision reconcile/exclude/include/delete. |
| `test_overview.py` | 10 | Partial: share bars, totals, sorting and folder expansion. Remaining: filtering and selection callbacks. |
| `test_delete_decision.py` | 10 | Mostly ported: delete decisions and mirror rules. |
| `test_rsync_parser.py` | 9 | Ported: itemized records, deletes, inventory with size/time, context phrases and rsync pattern matching including anchored/trailing and bracket globs. |
| `test_execution_plan.py` | 9 | Ported: summaries, fingerprints, freshness. |
| `test_sync_confirmation.py` | 8 | Dialog fields, command, rules and warning ported; add a widget assertion for each field. |
| `test_settings_dialog.py` | 8 | Ported in part: diagnostics, defaults, tools, logging, colours, verification visibility. |
| `test_locked_profile_startup.py` | 8 | Ported: retry, temporary copy and quit-without-changes. |
| `test_diff_tool_service.py` | 8 | Local meld launch implemented; placeholder expansion and remote staging remain tied to the external-tools feature gap. |
| `test_sync_modes.py` | 7 | Ported: update/mirror/new-only argv and destination-only decisions. |
| `test_inventory.py` | 7 | Ported at the parser/merge level: source and destination listings, filter handling and sizes. |
| `test_plan_validation.py` | 6 | Ported: verify mismatch/missing/unexpected. |
| `test_journal_behavior.py` | 6 | Partial: journal/status separation and colours. |
| `test_backup_*` | 17 | Core backup paths/protection ported; add inspection/path edge cases. |
| `test_filter_editor_delegate.py`, `test_filter_rule_model.py` | 13 | Editor, highlighter, preview and validation ported; add delegate paint assertions. |
| `test_profile_editor.py` | 5 | Options, preservation, backup, conflict policy and filters ported; add widget round-trip tests. |
| `test_tools_panel.py` | 5 | Table, add/edit/remove and diff command ported; add placeholder-warning tests. |
| Remaining storage/CLI/timing files | ~40 | Core coverage exists; convert edge cases incrementally. |

## Delivered in this pass

- Unchanged gate: default compare hides `NoChange`; inventory adds only after
  the toggle requests it; the destination listing is cached at compare time so
  an inventory request lists Source only; source sizes/times survive the merge.
- Layout modes: Normal, Compare only and Compare with overview are asserted by
  `layoutModesRearrangeWidgets`.
- Packaging: `install` CTest stages the build and checks the binary, desktop
  entry and icon; `packagingResourcesArePresent` checks the source resources and
  embedded window icon.
- Decision checkbox paint: checked and unchecked states are rendered and
  sampled by `decisionCheckboxStatesAreDistinct`.
- Filter editor: rules, comments and line preview are covered by
  `filterEditorValidatesAndListsRules`.
- Action filters: hiding, ancestor retention, keyboard-only focus and separator
  count are covered by new UI cases.
- Directory decision filters: `directoryFiltersApplyOnlyWhenEnabled` checks
  that directories ignore the decision filters until the toggle is on, and that
  a filtered directory stays as a faded, uncounted parent with visible
  children.
- Overview totals: `overviewCountsReportedDirectoriesOnly` checks that a
  reported directory counts as one item of its parent, synthesized folders do
  not, and the overview Total equals the root row's Items.
- Action filters: `actionFilterHidesUnrelatedDirectories` checks that filtering
  to Create keeps only real ancestors and that unrelated folders are neither
  visible nor counted.
- Navigation: Home/End, Ctrl+Up/Down and Left/Right are covered by
  `homeEndJumpToTheVisibleEdges`, `ctrlUpDownWalkTheHierarchy` and
  `leftRightFoldAndNavigateFolders`; `layoutShortcutCyclesModes` covers
  Ctrl+Tab.
- Decision filters: `filterCheckboxesSupportInvertedClicks` and
  `resetExclusionsKeepsOtherDecisions` cover Ctrl+click and exclusion-only
  reset.
- Sync output: `rsyncProgressDerivedFromCounters` covers progress derived from
  rsync counters; UI cases cover the stale state after sync and the concise
  journal.
- CLI: `startupOptionsParseAllOptions` and
  `startupOptionsValidateModeAndComparePair` cover the qpy startup-option
  cases that apply to the C++ executable, including forwarding Qt-only
  switches.
- Menus/shortcuts: `menusAndShortcutsAreWired` asserts the top-level menus,
  layout actions, Undo/Redo actions and the F1-F12 window shortcuts.

## Next conversion batches

1. `test_action_filters.py` exhaustive inversion state combinations.
2. `test_flat_filtering.py` path/glob patterns and directory-only mode.
3. `test_main_window_offscreen.py` exact shortcut enablement and full journal states.
4. `test_flat_comparison_view.py` per-pane menus and exact tooltips.
5. `test_settings_dialog.py` and `test_profile_editor.py` widget round-trips.
6. External tool placeholder expansion/launch (feature gap, not only tests).
7. `test_sync_confirmation.py` per-field dialog assertions.
8. Golden screenshots for main window, settings, sync dialog and profile editor.
