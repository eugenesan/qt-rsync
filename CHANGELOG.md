# Changelog

All notable changes to qt-rsync. Versions group a few related milestones
rather than every commit; the port started at 0.0.1 and reached 0.2.0 on
2026-11-24.

## 0.2.0 — 2026-11-24

Parity follow-up: decision terminology, directory filters, overview totals,
and refreshed documentation and packaging.

### Changed

- Tooltips use a uniform 250 ms wake-up delay with no fall-asleep grace, so
  moving between widgets no longer shows the next tooltip instantly.
- The Decisions header now draws section borders.
- Toolbar order: swap button before the sync mode selector, Settings before
  the layout button, and the standalone Find button removed (keyboard search
  shortcuts unchanged).
- Backup and Log buttons open their menus on left-click, with their own action
  as the first entry. The menus are now owned by the buttons (InstantPopup), so
  clicking between them switches menus instead of triggering Clear Log.
- Changing Source/Destination marks the comparison stale; swapping twice
  restores the previous state.
- Compare uses `refreshstructure-symbolic` and the app menu uses
  `open-menu-symbolic` when the icon theme provides them.
- The progress pane is gone. A fixed-width progress bar (15% of the window)
  sits at the right end of the status bar, and progress messages share the
  status field instead of a separate label. Streaming updates do not spam the
  journal.
- The rsync command preview is now a temporary, selectable last entry in the
  journal: no timestamp, not written to the log file. It shows Compare by
  default, switches to Verify on hover (when enabled), clears when a command
  starts and returns when the app is idle. Repeated hover events replace the
  single entry with no blank line, any real journal line removes the preview
  first, and it is shown in a faded italic colour with a "compare rsync
  preview:" or "verify rsync preview:" marker. The preview appears only after
  the first hover of Compare or Verify.
- Menu-button chevrons point down and sit at the bottom-left of the button.
- The overview pane keeps the same width in Normal and "Compare with overview"
  layouts.

### Fixed

- Clear Log now asks for Approve/Cancel before truncating the session log.
- Cancel and Pause start disabled and are only enabled while an rsync operation
  runs; Pause no longer stays active after Compare.
- Buttons with right-click menus show a small chevron and a tooltip hint.
- The log button opens Settings when no log is configured; the backup button
  opens the backup editor when no backup directory is set. Tooltips reflect
  that state.
- The profile menu lists Edit... directly under Save.
- Journal default text follows the palette text colour used by the overview.

### Added

- The sync confirmation dialog has an enabled-by-default "Compare again after
  synchronization" option.
- The Filters button and dialog state that rules are stored in the profile.

### Fixed

- The last qpy model-import experiment was removed: `DataStore::load_binary()`
  (the `QPYN` native dump reader) was dead code. Nothing imports qpy settings,
  profiles or model dumps now.
- The profile menu lists the ten most recently modified profiles (name as tie
  break) instead of a name-sorted list; the old recent-profile keys are gone.
- All settings are now plain text. Base64 is gone: tools use indexed INI keys,
  colours use one key per action, and window/splitter state uses comma-separated
  integers. There is no settings or profile import from qpy-rsync.
- Collapsing a folder forces a full repaint of all three comparison panes and
  refreshes the overview, removing stale cells painted over the panes below.

### Added

- Per-tool "Open with" context menu, remote diff staging and overview filter
  tracking close the last test-port gaps; the tracker is now fully consistent.
- The README was rewritten in the qpy-rsync style and uses new KDE-theme
  screenshots. The screenshots, the application icon and the desktop entry
  live in `resources/`.
- Overview has a Files row under Total for root-level objects, size-based
  Share percentages that add up, and Share as the default sorted/indicated
  column. Only Total starts expanded; folder rows start collapsed.
- Compare Items and the overview count every displayed item, including a
  reported directory as one item of its parent. Synthesized navigation
  folders and faded filtered-out parents are not counted, so the overview
  Total equals the root row's Items.
- Every overview folder has a go-jump icon left of the tree line. Clicking it
  reveals and centers the folder in the comparison panes; single and double
  clicks keep the default select/expand behaviour.
- A folder that does not exist on one side is blank in that pane, without a
  fold chevron, folder icon or name.
- Excluding items keeps the cursor in the list: it moves to the next visible
  row after the last excluded item, the previous visible row when nothing
  follows, or the root row when no items remain. The cursor follows item
  positions, not stale row numbers, so a shrinking list cannot leave it out of
  range. The compared folder no longer has an exclude checkbox and ignores
  Space.
- Overview folders can be excluded with Space or the context menu, restored
  from their subtree, and excluded by an rsync filter from the same menu.
  Adding a filter from the overview marks the comparison stale.
- The completion-sound setting now plays a sound: a comparison plays the
  desktop success sound after its destination metadata finishes, and a
  synchronization plays success or error. It uses canberra, then pw-play or
  paplay with the freedesktop/Oxygen files, and the terminal bell as fallback.
- Restored the qpy keyboard navigation: Home/End jump to the first/last
  visible item, Ctrl+Up/Down walk the folder hierarchy, and Left/Right
  fold/unfold a folder and then move to its parent or first child.
- The decision-filter checkboxes now support Ctrl+click inversion and the
  right-click menu (Show only this, Show everything but this, Show all, Reset
  to default). The Excluded toggle offers Reset All Exclusions.
- “Show only this” and the action filters hide directories whose own action is
  filtered and that contain no visible rows. Only real ancestors of visible
  items remain, and only visible reported folders are counted in Items and the
  overview.
- Synchronization progress is derived from rsync's to-chk/ir-chk counters and
  shown on the progress bar instead of relying on rsync's byte percentage.
- Synchronization output is filtered like Compare: progress and itemized
  records no longer flood the journal, which keeps only the summary and
  problems. After a sync the comparison is stale, the root row shows a stale
  suffix like qpy, the state turns orange, the single status message is no
  longer duplicated, progress resets to a completed label and Sync/Verify stay
  disabled until a new comparison.
- Ctrl+Tab / Ctrl+Shift+Tab cycle the compare layouts again; the navigation
  filter no longer swallows the shortcut.
- Startup options are validated (mode and compare pair) and Qt-only switches
  such as `-style Fusion` are forwarded to QApplication again.
- Compare progress is visible again: the dry run streams its itemized records
  into a live "N item(s) found" label with an indeterminate bar. The label is
  updated every 1000 records, like qpy, instead of once per rsync line.
  Synchronization progress updates only when the percentage changes, with a
  250 ms pacing fallback so partial batches still update during long runs.
- Every journal line now carries a timestamp, including rsync summaries and
  state markers, and is written to the session log. `--log-file` and
  `--no-log-file` are honoured at startup again.
- Decision mirroring walks decided subtrees through a child index instead of
  scanning every path for every decision, and the folder tree order is cached
  between filter rebuilds, removing the UI stall on large comparisons.
- The destination metadata listing now starts as soon as the dry run ends and
  runs in parallel with parsing and view construction, like qpy, instead of
  after them. The unused per-record description and the duplicate plan rebuild
  after metadata are gone, and deleted-size lookups use an ancestor index
  instead of scanning the whole metadata table per directory.
- Comparison journal timings now report real wall-clock values (including the
  total "Comparison ready in" time) instead of hard-coded zeroes.
- The overview no longer scans every folder's children for "Files" for every
  item, the decision child index is cached, and `buildDefaults` skips a whole
  pass. A fresh 640k-item comparison now spends about 1.2 s building the view
  instead of roughly 10 s.
- Decision edits use a summary-only plan update and defer the expensive
  fingerprints until Sync/Verify, and focus restoration no longer builds a
  half-million-entry hash. Excluding one item dropped from about 700 ms to
  roughly 150 ms on the same comparison.
- The destination listing starts when the dry run ends, not alongside it, so
  two rsync passes never run on the same endpoints at once and the compare
  progress label is not interrupted. Journal command lines use the neutral
  colour even when a path contains a word like "error", and preference
  changes no longer force a synchronous settings write inside UI slots.
- Overview statistics scale sizes to MiB/GiB instead of always showing KiB.
- The Decisions column tooltip now follows qpy: wrapped source and destination
  paths, the action, both modification times, changed-item counts on folders,
  stale-comparison help, and a `Decisions* (N modified)` header tooltip.
- A single decision no longer rebuilds the full overview tree; only the totals
  are refreshed, matching the hybrid view's incremental behaviour. Preference
  writes from toolbar/filter clicks are coalesced on a timer instead of
  hitting the settings files on every click.
- Opt-in phase timing: enable Settings > Advanced > "Include timing
  diagnostics in the session log" (or "Include rsync debug diagnostics") to
  write per-phase millisecond timings for decisions, filters, overview, plan
  and compare into the session log. With the option off there is no overhead.
- The single-decision plan rebuild is deferred until Sync/Verify, decision
  inheritance is checked by prefix instead of allocating every ancestor, and
  the overview tree is built from stable path ids instead of parsing and
  hashing every path. On a 640k deep-path comparison, exclude dropped from
  ~500 ms to ~16 ms and a filter toggle from ~650 ms to ~30 ms.
- Ctrl+click on Show excluded now reliably selects "only excluded" from any
  other filter state, and the session log path no longer duplicates the
  application name under XDG_CACHE_HOME. A legacy "default" log path stored
  in settings is ignored so the log follows the loaded profile again.
- The overview updates on every decision again (not just its totals). Its
  folder nodes are built from stable path ids and the expanded-path scan is
  skipped for decisions, so a full overview update is ~15-35 ms at 640k rows.
- Ctrl+click on Show excluded now really shows only excluded items. The
  filter bar used 0 both for "all actions" and "no actions", so turning every
  action off left all rows visible; an explicit no-actions sentinel fixes it.
  The inverse is applied at most once per physical click, and the context
  menu offers "Show only excluded items" / "Show all decisions". Legacy
  app-default.log settings are ignored by file name so old nested paths
  cannot pin the log name.
- Fixed the drawing artefact where the Decisions column kept a framed,
  striped area below the last row after the journal was repainted: the table
  views now use NoFrame and the custom full-height border painting is gone.
- Sync and Verify start disabled until a comparison makes them actionable;
  both the toolbar buttons and the menu actions follow the plan state.
- On exit, a profile that differs from the one loaded (for example after
  "Use as Source/Destination...") prompts to save, naming the changed fields.
  The prompt offers Save, Save As... and Don't Save, so a modified profile can
  be stored under a new name.
- New "Use as Source/Destination..." context action on directory rows. It
  dives into the folder on both sides (Use as Both / Cancel), optionally
  shortens the permanent filter rules by the eliminated directory prefix, and
  marks the comparison stale with a reminder to Compare again and Save/Save As
  the profile.
- Folded full paths are computed lazily, saving a full lowercase pass over
  every path during compare. More sub-phase timings are available when timing
  diagnostics are enabled, for further performance analysis.
- External tools support qpy's placeholder expansion; the diff command uses
  the configured command with `%a`/`%A` and quoted paths stay one argument.

### Changed

- Changing the sync mode marks an existing comparison stale, disables
  Sync/Verify and orange-badges the Compare icon until a fresh comparison.
- “Apply the decision filters to directories” now follows qpy semantics: off
  by default so directory rows ignore decision filters, on makes a directory
  a faded, uncounted parent when its own change is filtered out.
- Obsolete “Actions”/“Action” labels were renamed to “Decisions”, including
  the menu, decision colours, filter tooltips and the comparison column.
- The overview rebuild keeps the chosen sort and the folded totals.

### Packaging

- The Debian package is `0.2.0-0` and installs the refreshed desktop entry,
  icon and launcher.
- The shared example profile is packaged as
  `/usr/share/doc/qt-rsync/examples/profile.json` and is also available as
  `examples/profile.json` in the checkout.

## 0.1.0 — 2026-11-24

KDE dark-theme and final UI parity pass.

### Added

- File/Edit/View/Decisions/Help menus, a mirrored application-menu button and
  window-level shortcuts for F1–F12, Ctrl+Z, Ctrl+Shift+Z and find actions.
- Full synchronization confirmation dialog: fingerprint, endpoints, backup
  state, counts grid, destructive warning, command preview and decision rules.
- qpy-style profile editor with Transfer, Transfer options, Conflict policy,
  Comparison, Preservation, Backup and Filters tabs plus a numbered filter
  editor with rsync syntax highlighting, line preview, import/export/help and
  backup browse/open buttons.
- Settings Tools tab with the external-tool table (name, command, options,
  enabled), add/edit/remove dialogs and a diff-tool command field.
- Settings Logging tab with an editable log file and browse button.
- Help/How-To and About dialogs.
- State badges on Compare, Preserve, Verify, Profile and Filters.
- Desktop integration: `qt-rsync.desktop`, scalable `qt-rsync.svg`, Qt
  resource embedding, window icon and CMake install rules.
- Debian packaging (`debian/`) derived from qpy-rsync with a 0.1.0 changelog,
  architecture-any binary package, desktop/icon install and build-only CMake
  configuration.
- Search field with next/previous match, status and keyboard shortcuts.
- Open-button context menus and local diff launching through the configured
  tool (meld by default).
- Profile editor and open-button access to the backup configuration.

### Changed

- The app inherits the platform style and palette. Under KDE it uses Breeze
  rather than a forced Fusion style, fixing wrong colours, separators and
  button frames.
- Replaced the QToolBar with qpy's plain 40×40 button row so Breeze draws
  visible button frames.
- Matched qpy tooltips for the mode selector, endpoints, toolbar, search,
  location buttons and filter checkboxes.
- Excluded count moved onto the Excluded checkbox; Undo moved to the start of
  the decisions bar with `edit-undo-symbolic`.
- The toolbar separator now sits between Inventory and Synchronize instead of
  before the mode selector, matching qpy-rsync.
- The overview now summarises only the rows currently displayed after action
  filters, unchanged gating and exclusions.
- The journal records the compare command, parse/run timing, preserved
  decisions, selected summary, inventory command, metadata count, completion
  state and total ready time, and scrolls to the newest line.
- Main-window geometry, state, layout mode and splitter positions are stored
  and restored. qpy-rsync's base64 window state is decoded directly.
- XDG migration now merges qpy-rsync settings when qt-rsync files already
  exist, including window geometry, splitter state and cached logs.
- Compare declares completion only after the destination metadata listing
  finishes; Compare/Sync/Verify stay unavailable until then.
- Sorting any comparison column sorts the shared data and updates the
  indicators on all three panes.
- Overview directories start collapsed; only the synthetic Total row is open.
- `--version` includes the checkout commit hash.
- Layout modes now work: Normal, Compare only and Compare with overview.
- Overview statistics use labels instead of painter text, so entries cannot
  be clipped and each action keeps its tooltip.
- Overview and comparison columns stretch the item column and end at the
  viewport edge.
- Verification defaults to off; its toolbar button and menu entry are hidden
  until Advanced settings enables them.
- Action rows use qpy's default colours; settings changes apply immediately.
- The synchronization filter file uses the qpy-style `qt-rsync-plan-*.rules`
  name.

### Fixed

- Unchanged files no longer appear after Compare. They are inventory-only and
  appear only while the Unchanged toggle is on. The destination listing is
  cached during Compare, so an inventory request lists only Source, intersects
  it with the cached destination, preserves source sizes/times and merges each
  unchanged path once.
- Unchecked decision boxes are now empty outlines instead of filled blue
  boxes indistinguishable from checked ones.
- The decision-column borders continue to the bottom of the view.
- File rows leave the Items column blank instead of showing `0`.
- Root, directory and unchanged-item text uses a lighter slate colour suitable
  for dark themes.
- Filter checkboxes no longer keep Breeze's focus underline after a pointer
  click.
- The progress bar is hidden when Compare, inventory, Verify or Sync
  completes, instead of remaining at 100 %.
- Removed the leftover `DELEGATE CLICK` terminal diagnostic.
- Excluded count resets on a new comparison; profile filter changes mark the
  comparison stale.
- Local diff actions report clearly when a remote endpoint or a one-sided
  file prevents opening the tool.

## 0.0.9 — 2026-11-23

Native-view parity and UI feedback.

### Added

- Ported qpy's drawn action/file icons, exact control sizes and native
  delegate/overview widgets.
- Inventory of unchanged items from both endpoints.
- Theme icon names with drawn fallbacks and the qpy verify default.

### Fixed

- Delegate decision clicks use a pane-independent decision role.
- Folder aggregates, unchecked checkbox rendering, excluded filter toggles and
  real-rsync exclusions.
- X11 keyboard decisions when Space reaches the table viewport.

## 0.0.8 — 2026-11-23

qpy layout parity.

### Added

- Real qpy layout: icon toolbar, Source/ActionFilterBar/Destination row,
  comparison above command preview, journal and overview, and a State bar.
- Full row context menu and decision reset actions.
- Header sorting in comparison lists and the overview.
- X11 keyboard folding, context-menu shortcuts and F5/F8/F9/F10 shortcuts.
- Alternating rows, tree chevrons, overview share bars and a journal filter.

## 0.0.7 — 2026-11-23

Correctness and real-sync validation.

### Added

- Real verification comparison, settings handling and process cancellation.
- Destructive-delete and mirror-mode end-to-end tests.
- Validation of real mirror sync against a disposable tree.

## 0.0.6 — 2026-11-23

Real-tree and SSH parity.

### Fixed

- Root names, totals, destination sizes and action-coloured rows.
- SSH profile endpoint preservation and auto-loaded profile endpoints.
- Column widths and icons on rows absent from one side.
- Undo visibility and folder aggregates.

### Added

- C++ action/file icons, X11 parity verification, keyboard folding and
  collapse context menu.

## 0.0.5 — 2026-11-23

Profiles and lifecycle.

### Added

- Profile menu, cached-profile startup and locked-profile dialog with retry,
  temporary copy and no-changes quit.
- Temporary-profile cleanup, log promotion and XDG path migration from
  qpy-rsync.

## 0.0.4 — 2026-11-23

Plan, verification and synchronization.

### Added

- Verify dry run, synchronization confirmation, command preview and
  decision-rule preview.
- Stale-after-sync behaviour and a journal with progress reporting and
  cancellation.

## 0.0.3 — 2026-11-23

First vertical slice.

### Added

- Lifted native `DataStore`, table/overview models, three synchronized panes
  and the first real Compare through `QProcess`.
- Decision delegate, Space handling, linked sorting/selection, statistics
  strip, context menus, header editor and initial settings pages.

## 0.0.2 — 2026-11-23

Core semantics in C++.

### Added

- Decisions, filters, execution plans, fingerprints, rsync command specs,
  profiles, XDG storage and PID locks.
- 41 QtTest core cases, with argv construction checked against qpy-rsync.

## 0.0.1 — 2026-11-23

Initial scaffold.

### Added

- CMake/Ninja C++20 build against Qt 6, separate core/model/ui libraries and a
  single `qt-rsync` GUI executable.
- Inherited qpy launcher options via `QCommandLineParser`: `--dir-pair`,
  `--backup`, `--mode`, `--compare`, `--log-file`, `--no-log-file`,
  `--version` and `--help`.
- Initial core/model/ui skeletons, C++-constructed widgets, first QtTest case,
  architecture decisions and the phased port plan.
