# Architecture decisions (qt-rsync)

These decisions are in force. Changing one should be recorded here with its
reason. The product specification and the research that motivated the port live
in the **qpy-rsync** repository (`docs/design.md`,
`docs/performance-analysis.md`, `native-hybrid/README.md`); they are not copied
here.

## ADR-001 — Qt Widgets, UI built in C++

**Decision.** Use Qt Widgets (`QMainWindow`, `QTableView`, `QTreeView`) with
widgets constructed in C++. Do **not** use QML, Qt meta-object dynamic UI
forms, or `.ui` files.

**Why.** The qpy-rsync native experiment already delivered the full polished UI
this way: `native-hybrid/hybrid.cpp` contained the delegates, panes, overview,
statistics bar, key filters and context-menu scaffolding, and
`native-hybrid/model.hpp` contained the models that scaled to ~1M rows. Rewriting
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

**Why.** Matches the toolchain already used by the native experiment and
available on the target machines (Qt 6.10.2, CMake ≥3.20, Ninja, GCC 15).

## ADR-004 — Tests (resolved)

QtTest is the only test suite; the port is complete (53 core, 76 UI, plus the
`install` target). The qpy-rsync pytest suite remains the behavioural oracle —
see `testing.md`. The old phase plan is gone; new behaviour gets a case beside
it.

## ADR-005 — CLI options stay in the GUI process

**Decision.** No separate CLI program. Parse the qpy-rsync GUI launcher options
with `QCommandLineParser` in `src/app/StartupOptions.cpp`; `--version` prints
and exits.

**Why.** The headless `qpy_rsync/cli.py` was a second entry point with its own
semantics; qt-rsync deliberately has one entry point. The **options** are
preserved, not the second binary.

## ADR-006 — qt-rsync-native paths, no qpy import (resolved)

Paths now use `~/.config/qt-rsync/profiles`, `~/.config/qt-rsync/settingsrc`
and `$XDG_CACHE_HOME/qt-rsync/...`. There is deliberately **no** import or
migration of qpy settings or profiles, and no base64-encoded values: settings
and widget state are plain INI scalars, while profiles stay JSON.

## ADR-007 — Native experiment lifted, bindings dropped (resolved)

The pure-C++ model/view layer came from qpy-rsync's native experiment; all
nanobind glue, `uintptr_t` QWidget handoffs and binding-specific icon ingestion
were dropped. The measurements and rationale remain in the qpy-rsync repository
(`native-hybrid/README.md`, `native-qt-model/CONCLUSION.md`).

## ADR-008 — No backend abstraction layer

**Decision.** Call `rsync` directly with `QProcess`. No interfaces for swapping
backends, no plugin registry, no dependency injection framework.

**Why.** The abstraction in qpy-rsync existed to support tests and a
hypothetical alternative engine; it added indirection without changing
behaviour. Tests use a fake rsync **executable** on `PATH` instead.
