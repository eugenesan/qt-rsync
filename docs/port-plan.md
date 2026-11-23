# qt-rsync port plan

This is the working plan for the pure Qt/C++ port. It converts the rules from
the project brief into phases with explicit exit criteria, and records the
corrections agreed before phase 0.

## Corrections to the brief

1. **Keep the core/UI split, but drop the dual state.** Rule 6 removes backend
   abstraction, not domain separation. Keep `src/core` (decisions, plan,
   filters, command, profile, paths) separate from `src/ui`; this is what makes
   the C++ code testable. What must *not* survive is a second UI-owned decision
   model (ADR-002).
2. **Rule 8 is decided: hardcoded C++ UI with `QAbstractItemModel` view models.**
   No QML, no `.ui` files. The native experiment is the proven implementation.
3. **Rule 9 is decided: port tests after rule 10's first vertical slice.** Core
   tests first, UI tests a phase later. Rationale in ADR-004.
4. **Rule 5 needs a boundary.** Reuse `model.hpp` and the Qt-only half of
   `hybrid.cpp`; the nanobind half is Python glue and is discarded (ADR-007).
5. **Rule 12 needs one clarification.** Keep the GUI launcher's options, but do
   not port the separate headless CLI binary (ADR-005).
6. **Add an exit criterion to every phase.** Without one, "basic structure"
   expands until it becomes the whole port.

## Source material in this tree

| File | Role |
|---|---|
| `design.md` | Authoritative product specification (copied from qpy-rsync). |
| `performance-analysis.md` | What was measured, what worked, what was rejected. |
| `plan-profiling.md` | Probe map to recreate as C++ timing instrumentation. |
| `cpp-qt-conversion-analysis.md` | Feasibility study and UI parity checklist. |
| `cpp-extension-analysis.md`, `native-qt-model-conclusion.md`, `native-Qt-model-experiment-plan.md` | Why the hybrid worked and where the boundary cost lived. |
| `python-tuning-plan.md` | Process lessons: use differential harnesses as an oracle; move a model and all its views together. |
| `hybrid-view-notes.md`, `hybrid-view-plan.md` | Behavioural spec for the native view layer and its contracts. |
| `roadmap.md` | Historical milestones; useful as a checklist, not as a plan for this project. |

## Phase 0 — scaffold (current)

Deliverables: CMake build, directory layout, placeholder app that builds and
shows an empty window, this plan and the decisions document, copied reference
docs.

Exit criteria:

```sh
./build.sh
./build/bin/qt-rsync --help
ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure
```

All three succeed. No functionality is required beyond an empty window.

## Phase 1 — core semantics in C++ (complete)

Ported in order, each with direct C++ tests:

1. `core::Change`, `core::Decisions` (INCLUDE / EXCLUDE / INHERIT /
   DELETE), reconcile, decision summary.
2. `core::Filter` — rule parsing, `phrasesFor`, `deletePhrasesFor`,
   sender-side `-s` rules, `cleanQtNoise`.
3. `core::Plan` — `buildExecutionPlan`, fingerprints, freshness,
   `decisionFilterText` (including synthetic deletes).
4. `core::Command` — compare / verify / sync specs, `--delete` handling,
   filter merge ordering.
5. `core::Profile`, `core::paths` — JSON profiles, XDG settings split, locks.

Keep the qpy-rsync Python tests as the oracle: for each ported rule, copy one
representative case into `tests/test_core.cpp` rather than attempting a bulk
translation.

Exit criterion: `ctest` green for the ported semantics and the generated
argv strings byte-identical to qpy-rsync for the same fixture profile.

Phase 1 is complete: 41 QtTest cases pass. `core::Decisions`, filters, plans,
command specs and storage are implemented in `src/core`.

## Phase 2 — first vertical slice (in progress)

- Replace the placeholder `ComparisonView` with the lifted `DataStore`,
  `TableModel`, `OverviewModel`, delegates, key/mouse filters, stats bar and
  context menus.
- Wire one real Compare: run rsync with `QProcess`, parse itemized output into
  `DataStore`, show it in the three panes and overview.
- Add the decision checkbox path with a **single** decision store.

Delivered so far: the lifted models, the parser, three synchronized panes,
the overview tree and a working Compare driven by `QProcess`. A fake-rsync
QtTest verifies the full path from CLI-style startup to rendered rows.

Delivered in the second slice: decision delegate, Space handling for all
selected rows, linked sorting, synchronized selection and scrollbars, and a
statistics strip. Decisions are owned only by `core::Decisions`.

Still to do in this phase: context menus (Exclude via Filter, Delete on
Destination), column header editor, and the settings dialog pages.

Exit criterion: a real Compare of the example-sized tree renders and decisions can
be toggled, with no mirrored state and no Python process involved.

## Phase 3 — plan, verification and sync (in progress)

Execution plan summary and validation dry run; sync confirmation dialog with
command preview and decision rules; `QProcess` sync with progress, cancel and
journal. Preserve qpy-rsync's rule that the comparison goes stale after sync
and is never refreshed automatically.

Delivered so far: plan summary wiring, verify dry run, sync confirmation
dialog with command/rules preview, and stale-after-sync behaviour, covered by
a fake-rsync UI test.

The journal widget and progress parsing are in place. Remaining in this phase:
rsync cancellation and real mirror-mode validation against a disposable tree.

Exit criterion: a verified and confirmed mirror sync against a disposable tree,
with journal continuity and stale state afterwards.

## Phase 4 — profiles, settings and multi-instance (in progress)

Profile save/load/save-as, locked-profile startup dialog, PID locks, settings
pages, diagnostics toggles, CLI option application, path rename migration
(ADR-006).

Delivered so far: profile menu, cached-profile startup, locked-profile dialog
with in-place retry and a no-changes quit path, and PID locks. The settings
pages and XDG split were ported in earlier phases.

Temporary-file cleanup, log promotion and the XDG path migration are now in
place. Phase 4 is complete.

Exit criterion: two instances can run against different profiles; a locked
profile offers temporary copy / retry / quit; no settings or profiles are
modified by the quit path.

## Phase 5 — parity hardening and test port (next)

Port the remaining qpy-rsync UI/decision differential tests to QtTest with
offscreen widgets. Retire the copied Python code from the working set only after
the full suite is green.

Exit criterion: every behaviour listed in `design.md` has a C++ test or a
documented manual smoke check.

## Out of scope for this port

- A second CLI binary.
- Alternative backends or a plugin system.
- Python, nanobind or any Qt/Python bridge.
- QML.
