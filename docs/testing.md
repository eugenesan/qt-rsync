# Testing

## Run

```sh
./test.sh                       # configure, build, ctest (offscreen, no X server)
./test.sh -DQT_RSYNC_BUILD_TESTS=OFF
ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure
```

`build.sh` only builds; `test.sh` builds and runs `ctest`. With a custom build
directory set `QT_RSYNC_BUILD_DIR` for both scripts. Qt tests use `Qt6::Test`;
`test.sh` sets `QT_QPA_PLATFORM=offscreen`. There are three CTest targets:
`core` (53 cases), `ui` (76 cases) and `install` (packaging staging check).

## Oracle strategy

The qpy-rsync Python suite at `/home/user/Develop/src/qpy-rsync/tests/unit` is
the behavioural oracle. Port **one representative case per C++ rule** rather
than translating cases mechanically: assert identical outcomes for the same
fixture, not identical test structure. Fixtures use neutral names (`alice`,
`server`, `nas`, `example.test`). A ported case is complete only when it fails
on the old behaviour and passes on the fixed one.

Coverage: all core rules (change reconcile, decisions, filters, plan, argv,
profiles, storage, endpoints, external tools) and the UI (three linked panes,
overview, search, filters, dialogs, menus, shortcuts, journal, progress,
staleness, tools) have QtTest cases. Specs and edge-case research are kept in
the qpy-rsync repository, not duplicated here.

## Deliberate differences / N/A

- The qpy headless CLI has no equivalent: qt-rsync is one GUI process with
  startup options only (ADR-005). JSON compare output and rsync exit-code
  propagation are therefore N/A.
- qpy's process-runner contract, SSH askpass plumbing and Qt environment cases
  are N/A: Qt uses `QProcess` directly.
- Narrow saved-header restoration and pixel-exact profile-editor screenshots
  are covered by widget round-trips, not pixel references.
- Byte-size columns 9/10 are hidden by default in the Qt port (unlike qpy).
- Settings and widget state are plain INI values; profiles are JSON. There is
  no qpy settings/profile import and no base64 encoding anywhere.

Add a QtTest case alongside any future behaviour change; update the case count
in this file and in `docs/code-map.md` when cases are added.

## Timing diagnostics

Settings → Advanced offers "Include timing diagnostics in the session log"
and a more verbose debug toggle. Timings are written to the session log and
stderr; the log is at `$XDG_CACHE_HOME/qt-rsync/app-<profile>.log`.

## Remote smoke safety

A remote compare is a dry run and safe on real trees. **Synchronize is
write-capable**: only use it on disposable trees. For real-tree smoke scenarios
see `qpy-rsync/tools/SYNC-SMOKE-TESTS.md`.
