# qt-rsync

A pure Qt/C++ frontend for rsync. This tree is a **phase 0 scaffold**: it
builds, parses the inherited CLI options and shows an empty main window.
No comparison, plan or sync functionality is implemented yet.

The product semantics are inherited from [qpy-rsync](https://github.com/eugenesan/qpy-rsync)
and documented in [docs/design.md](docs/design.md). The migration sequence and exit criteria are
in [docs/PORT-PLAN.md](docs/PORT-PLAN.md); the up-front architecture choices
are in [docs/DECISIONS.md](docs/DECISIONS.md). Release notes start in [CHANGELOG.md](CHANGELOG.md).

## Build

Requirements: Qt 6 (Core, Gui, Widgets, Test), CMake 3.20+, Ninja, a C++20 compiler.

```sh
./build.sh
./build/bin/qt-rsync --help
ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure
```

`build.sh` accepts extra CMake arguments, for example
`./build.sh -DQT_RSYNC_BUILD_TESTS=OFF`.

## Layout

```
src/app/    entry point, CLI parsing, XDG/Qt environment setup
src/core/   domain: changes, decisions, filters, plan, rsync command, paths
src/model/  Qt item models over one C++ DataStore (lifted from native-hybrid)
src/ui/     main window, comparison panes, overview, settings dialog
tests/      QtTest suite
docs/       port plan, decisions and the inherited qpy-rsync specification
```

## Design constraint

There is exactly one decision store and one comparison dataset, both owned in
C++. The UI models only project them. This is the central lesson from the
qpy-rsync hybrid phase and is documented in ADR-002.
