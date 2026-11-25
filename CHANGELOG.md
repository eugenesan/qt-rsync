# Changelog

All notable changes to qt-rsync. Versions group a few related milestones
rather than every commit; the port started at 0.0.1 and reached 0.2.0 on
2026-11-24.

## 0.2.0 — 2026-11-24

Parity follow-up: decision terminology, directory filters, overview totals,
and refreshed documentation and packaging.

### Added

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
  problems. After a sync the comparison is stale, the state and status turn
  orange, and Sync/Verify stay disabled until a new comparison.

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
