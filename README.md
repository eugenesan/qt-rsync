# qt-rsync

A pure Qt/C++ frontend for rsync. It is a direct port of the behaviour of
[qpy-rsync](https://github.com/eugenesan/qpy-rsync): the same decision
semantics, execution plan, filter rules, rsync command construction, profiles
and UI model, but with **no Python layer and no second internal state**.

The product specification is [docs/design.md](docs/design.md); the migration
plan and exit criteria are in [docs/port-plan.md](docs/port-plan.md); the
architecture choices in [docs/decisions.md](docs/decisions.md). Release notes
start in [CHANGELOG.md](CHANGELOG.md).

## Status

Functional parity: Compare, decisions (including exclude, delete-on-destination,
undo and redo), unchanged inventory, overview, sorting/folding, profiles with
multi-instance locks, Verify, Synchronize with confirmation, cancellation,
pause/resume, journal and progress all work. Side-by-side X11 runs against
qpy-rsync matched on small fixtures, a 620-file tree and the real SSH profile;
mirror-mode sync and filtered transfers are validated against disposable trees.

## Build

Requirements: Qt 6 (Core, Gui, Widgets, Test), CMake 3.20+, Ninja, a C++20
compiler. No Python is required.

```sh
./build.sh                       # builds into ./build (or $QT_RSYNC_BUILD_DIR)
./build/bin/qt-rsync --help
./test.sh                        # configure, build and run all QtTest cases
```

`build.sh` accepts extra CMake arguments, for example
`./build.sh -DQT_RSYNC_BUILD_TESTS=OFF`.

Install the binary, desktop entry and icon with:

```sh
cmake --install ./build --prefix /usr/local
```

The repository includes `resources/qt-rsync.desktop` and
`resources/qt-rsync.svg`; the icon is also embedded as a Qt resource so the
window icon works without an installed theme.

## Layout

```
src/app/    entry point, CLI parsing, XDG/Qt environment setup
src/core/   domain: changes, decisions, filters, plan, command, parser, storage
src/model/  Qt item models over one C++ DataStore (lifted from native-hybrid)
src/ui/     main window, comparison panes, overview, settings, dialogs
tests/      QtTest suite (core + UI)
docs/       port plan, decisions and the inherited qpy-rsync specification
```

## Design constraint

There is exactly one decision store and one comparison dataset, both owned in
C++. The UI models only project them. This is the central lesson from the
qpy-rsync hybrid phase and is documented in ADR-002.
