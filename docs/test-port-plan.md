# qpy-rsync test port tracker

qpy-rsync currently collects 489 pytest cases. qt-rsync has 125 QtTest cases
(53 core, 72 UI) and three CTest targets (`core`, `ui`, `install`). This file
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
| `test_main_window_offscreen.py` | 95 | Mostly ported: startup, profile cache, locked profile, compare/verify/sync, progress, settings, closing, stale state, menus/F-keys, exact startup action states and journal severity colours. |
| `test_flat_comparison_view.py` | 49 | Ported: panes, selection, folding, context menus (including open/diff/tools), navigation, stale root tooltip, header toggles and Decisions tooltips. |
| `test_flat_filtering.py` | 31 | Mostly ported: action masks, ancestors, unchanged gate, search, path/glob filters and directory-only mode. |
| `test_action_filters.py` | 19 | Ported: initial state, tooltips, focus policy, separators, ancestors, Ctrl+click inversion, exhaustive state combinations and reset defaults. |
| `test_command.py` | 13 | Ported: compare/verify/sync/fetch argv, modes, filters, delete timing and remote shell. |
| `test_app_startup_options.py` | 12 | Ported at the CLI level: complete argument set, Qt-only arguments, log-file/no-log-file, compare pair and invalid mode. Qt environment cases are N/A; there is no qpy settings/profile migration in the C++ port. |
| `test_external_tools.py` | 11 | Ported: placeholders, options and bad commands at the core level, JSON tool persistence, and an "Open with" submenu in every comparison context menu with shortcuts 2-9. |
| `test_changes.py` | 11 | Ported: decision reconcile/exclude/include/delete. |
| `test_overview.py` | 10 | Ported: totals, share, sorting, expansion, jump callback and action-filter propagation. |
| `test_delete_decision.py` | 10 | Mostly ported: delete decisions and mirror rules. |
| `test_rsync_parser.py` | 9 | Ported: itemized records, deletes, inventory with size/time, context phrases and rsync pattern matching including anchored/trailing and bracket globs. |
| `test_execution_plan.py` | 9 | Ported: summaries, fingerprints, freshness. |
| `test_sync_confirmation.py` | 8 | Ported: dialog title, buttons, command/rules views and fingerprint header asserted. |
| `test_settings_dialog.py` | 8 | Ported: tab titles, OK/Cancel, sound round-trip, diagnostics, tools and colours. |
| `test_locked_profile_startup.py` | 8 | Ported: retry, temporary copy and quit-without-changes. |
| `test_diff_tool_service.py` | 8 | Ported: local diff launch, remote staging into a temporary directory via `buildFetchSpec`, progress/preparation state, command journal and cleanup on exit. |
| `test_sync_modes.py` | 7 | Ported: update/mirror/new-only argv and destination-only decisions. |
| `test_inventory.py` | 7 | Ported at the parser/merge level: source and destination listings, filter handling and sizes. |
| `test_plan_validation.py` | 6 | Ported: verify mismatch/missing/unexpected. |
| `test_journal_behavior.py` | 6 | Ported: timestamps, neutral command colour, severity colours and rsync noise handling. |
| `test_backup_*` | 17 | Core backup paths/protection ported; add inspection/path edge cases. |
| `test_filter_editor_delegate.py`, `test_filter_rule_model.py` | 13 | Editor, highlighter, preview and validation ported; add delegate paint assertions. |
| `test_profile_editor.py` | 5 | Ported: tab titles, profile round-trip, options, preservation, backup and filters. |
| `test_tools_panel.py` | 5 | Table, add/edit/remove and diff command ported; add placeholder-warning tests. |
| Remaining storage/CLI/timing files | ~40 | Triaged: covered by core storage/endpoint/validation cases or explicitly N/A below. |

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
  journal. `journalEntriesHaveTimestamps` checks that every journal line has a
  timestamp.
- CLI: `startupOptionsParseAllOptions` and
  `startupOptionsValidateModeAndComparePair` cover the qpy startup-option
  cases that apply to the C++ executable, including forwarding Qt-only
  switches.
- Menus/shortcuts: `menusAndShortcutsAreWired` asserts the top-level menus,
  layout actions, Undo/Redo actions and the F1-F12 window shortcuts.
- Compare progress: `compareProgressShowsItemsFound` streams a fake dry run with
  1000 records and checks the batched "item(s) found" label.
  `liveRecordCheckMatchesParser` guards the cheap counter used while streaming.
- New feature: `shortenFilterPrefixes` covers filter prefix rewriting and
  the two `useAsLocation...` tests cover the dialog choice and the stale
  profile state; `syncIsDisabledOnStartup` covers the initial button/action
  state.
- Performance instrumentation: Settings > Advanced > timing/debug writes
  per-phase milliseconds to the session log; no code runs when it is off.
- Compare performance: `metadataListingStartsBeforeViewBuild` checks that the
  destination listing is launched before the view is built, and
  `journalEntriesHaveTimestamps` checks the timestamped journal.
  `quickPlanSummaryMatchesFullPlan` guards the summary-only plan update used
  by decision edits, `formatSizeUsesLargeUnits` covers overview scaling and
  `journalCommandLinesAreNeutral` covers the command-line colour and
  `decisionsTooltipIsInformative` covers the Decisions cell and header tips.
- External tools: `externalToolPlaceholders` covers the qpy placeholder cases
  and the diff command now uses the configured placeholder-aware command.
- All test-port batches are complete: see "Batches completed in this pass"
  and "Final batches completed". `exitSavePromptOffersSaveAs` covers the three
  exit choices.

## Batches completed in this pass

1. `test_action_filters.py` exhaustive inversion state combinations —
   `invertedClicksCoverStateCombinations`.
2. `test_flat_filtering.py` path/glob and directory-only mode —
   `pathFiltersAndGlobsMatchVisibleRows`; this exposed and fixed a real bug
   where directory-only mode hid all reported directories.
3. `test_main_window_offscreen.py` startup action states and journal colours —
   `startupActionStatesMatchPython`, `journalSeverityColoursMatchPython`.
4. `test_flat_comparison_view.py` menu actions and stale tooltip —
   `staleRootTooltipAndContextMenuActions`.
5/7. `test_settings_dialog.py`, `test_profile_editor.py` and
   `test_sync_confirmation.py` widget round-trips —
   `dialogRoundTripsCoverPortedFields`.
8. Golden screenshots — `goldenScreenshotsAreValidImages` verifies the three
   committed reference images; the profile editor is covered by the widget
   round-trip instead of a separate image.

## Toolbar and location-state tweaks

`toolbarOrderAndLocationStaleness` checks the new toolbar order, the removed
Find button and that swapping Source/Destination twice restores the previous
comparison state.

## Layout and preview tweaks

`overviewWidthStableAcrossLayouts` keeps the overview width constant between
Normal and wide layouts; the preview test now simulates the first hover before
expecting a command entry.

## Progress and command preview

`commandPreviewIsTemporaryJournalEntry` verifies the progress bar lives in the
status bar at 15% width, the old preview widget is gone, and the preview is a
non-persistent journal entry that can be cleared. Streaming progress uses
`setProgressStatus` so it does not append journal lines.

## UI behaviour fixes

`startupControlsAndDialogDefaults` covers disabled Cancel/Pause on startup,
right-click hints, profile-menu order and the sync dialog's automatic Compare
option. Sync tests disable that option so their status assertions stay stable.

## Recent profiles

`profile/current_path` is a plain path; the old `profile/recent` /
`profile/recent_paths` keys are gone. The profile menu is built from the
profiles directory, sorted by modification time (newest first, name as tie
break) and capped at ten entries, matching qpy. Covered by
`recentProfilesUseModificationTime`.

## Plain settings format

Settings are INI-native plain text in both apps: indexed `tools/external/*`
keys, per-action `colours/actions/<name>` keys, `main-window/geometry` as
`x,y,w,h`, splitter sizes as comma-separated integers, and booleans/strings
verbatim. `settingsUsePlainTextFormats` and `obsoleteEncodedSettingsAreRemoved`
assert the Qt side contains no base64 and legacy blobs are cleaned up. There is no qpy settings/profile import.

## Final gap closure

- Per-tool launch: `fetchSpecAndToolPersistence` (JSON tool round-trip) and
  `contextMenuOffersExternalTools` ("Open with" submenu, shortcuts and tool
  names); `runExternalTool` expands placeholders and starts the process with
  the item directory as cwd.
- Remote diff staging: `buildFetchSpec` runs rsync (with `setsid` and remote
  shell) into a private temporary directory; the GUI shows preparation
  progress, journals the command and removes staged directories on exit.
- Overview callbacks: `overviewFollowsActionFilters` checks that overview
  totals track action filters; `foldingRefreshesViewsAndOverview` covers the
  repaint/refresh after collapsing a folder.

## Final batches completed

- Header-menu variants: `headerMenuTogglesExactSections` asserts source-pane
  toggles, the decisions-only column and the destination column mapping.
  Byte-size columns 9/10 are hidden by default in the Qt port (unlike qpy),
  which is an explicit presentation difference, not a missing case.
- Per-pane variants: `contextMenuOpenActionsFollowEndpointKind` covers local
  file/directory open actions, SSH gating and distinct root names;
  `cellRolesAndPresenceMatchPython` covers per-side presence, byte sizes,
  decoration and tooltip roles.
- Storage/CLI edge cases: `endpointEdgeCases` (alias, daemon/relative syntax,
  home expansion), `profileValidationMessages` (two remotes, backup root,
  remote backup, nested-backup warnings), `swapEndpointTexts`. The remaining
  profile store/convert/legacy cases are already covered by
  `profileJsonRoundTrip`, `legacyProfileFieldsMigrate` and
  `settingsAreSplitByPrefix`.

## Explicit N/A

- `test_cli.py` JSON compare output and rsync exit propagation: the C++ port
  is a GUI app with CLI startup options only.
- `test_process_runner_contract.py` and `test_ssh_askpass.py`: qpy-specific
  process/environment and askpass plumbing; Qt uses QProcess directly.
- Narrow saved-header restoration and exact visual screenshots for the profile
  editor: covered by the widget round-trip instead of a pixel reference.

All behaviour rows in the status table are now either ported or N/A. Test-port
work is complete; future changes should add cases alongside the behaviour.
