# Architecture decisions (qt-rsync)

Status: proposed for phase 0. Each decision can be revisited, but changing one
should be recorded here with its reason.

## ADR-001 — Qt Widgets, UI built in C++

**Decision.** Use Qt Widgets (`QMainWindow`, `QTableView`, `QTreeView`) with
widgets constructed in C++. Do **not** use QML, Qt meta-object dynamic UI
forms, or `.ui` files.

**Why.** The qpy-rsync native experiment already delivered the full polished UI
this way: `native-hybrid/hybrid.cpp` contains the delegates, panes, overview,
statistics bar, key filters and context-menu scaffolding, and
`native-hybrid/model.hpp` contains the models that scaled to ~1M rows. Rewriting
the UI in QML would discard a tested asset and add a second UI paradigm.
`CMAKE_AUTOUIC` is off; `AUTOMOC` is on.

## ADR-002 — One C++ source of truth

**Decision.** `core::Decisions`, `core::ExecutionPlan` and `model::DataStore`
are the only owners of decision and comparison state. View models are
projections; they never keep a parallel decision table.

**Why.** The dominant maintenance cost in late qpy-rsync was verifying the
Python/native boundary: two decision stores, two selection states, diff
mirroring, generation guards and parity tests. A pure C++ port removes the
boundary entirely. The native `decision_log` becomes ordinary Qt signals
connected to the one store.

## ADR-003 — Build: CMake + Ninja, C++20, Qt 6

**Decision.** `CMakeLists.txt` with `find_package(Qt6 COMPONENTS Core Gui
Widgets Test)`, C++20, Ninja via `build.sh`. No qmake, no Python, no nanobind.

**Why.** Matches the toolchain already used by `native-hybrid` and available on
the target machines (Qt 6.10.2, CMake 4.2, Ninja 1.13, GCC 15).

## ADR-004 — Tests: QtTest, ported after the first vertical slice

**Decision.** Use `Qt6::Test`. Port core-semantics tests (decisions, plan,
filter rules, command argv) first, as soon as those C++ types are implemented;
port UI/parity tests later against the real widgets with
`QT_QPA_PLATFORM=offscreen`.

**Why.** A test needs a meaningful API to assert against. Writing UI tests
against placeholder widgets before the port produces tests that must be
rewritten. The old Python suite remains the behavioural oracle (see
`PORT-PLAN.md`), not a dependency.

## ADR-005 — CLI options stay in the GUI process

**Decision.** No separate CLI program. Parse the qpy-rsync GUI launcher options
with `QCommandLineParser` in `src/app/StartupOptions.cpp`; `--version` prints
and exits.

**Why.** Rule 12. The headless `qpy-rsync/cli.py` was a second entry point with
its own semantics; qt-rsync deliberately has one entry point. The **options**
are preserved, not the second binary.

## ADR-006 — Keep qpy-rsync's XDG layout initially

**Decision.** Phase 0 uses the old directory names
(`~/.config/qpy-rsync/profiles`, `$XDG_CACHE_HOME/qpy-rsync/...` and the two
settings files) so profiles and logs remain findable. Renaming to `qt-rsync`
paths happens in a later phase with a migration.

**Why.** Avoids losing user data during the first runs and keeps the copied
docs accurate. This is compatibility debt, recorded deliberately.

## ADR-007 — Lift the native experiment, drop the bindings

**Decision.**
- Lift `native-hybrid/model.hpp` essentially as-is (pure C++/Qt).
- Lift the Qt-only portion of `native-hybrid/hybrid.cpp` (approximately lines
  45–1121): session, refresh, stats widget, delegates, key/mouse filters,
  tables and overview widgets.
- Discard the nanobind module block (approximately lines 1122–1607), all
  `uintptr_t` QWidget handoffs and `nanobind` icon ingestion.

**Why.** The experiment proved the widget and model layer works; the binding
layer exists only to talk to Python and has no place here.

## ADR-008 — No backend abstraction layer

**Decision.** Call `rsync` directly with `QProcess`. No interfaces for swapping
backends, no plugin registry, no dependency injection framework.

**Why.** Rule 6. The abstraction in qpy-rsync existed to support tests and a
hypothetical alternative engine; it added indirection without changing
behaviour. Tests can use a fake rsync **executable** on `PATH` instead.
