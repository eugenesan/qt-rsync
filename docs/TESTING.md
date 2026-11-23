# Testing

## Run

```sh
./test.sh                       # configure, build, ctest (offscreen)
./test.sh -DQT_RSYNC_BUILD_TESTS=OFF
ctest --test-dir /tmp/opencode/qt-rsync-build --output-on-failure
```

Qt tests use `Qt6::Test`. `test.sh` sets `QT_QPA_PLATFORM=offscreen`, so no X
server is needed.

## Oracle strategy

The qpy-rsync Python suite is the behavioural oracle for the port. For each
ported C++ rule, copy one representative Python test case into the QtTest
suite rather than translating it mechanically. The goal is identical outcomes
for the same fixture, not identical test structure.

Phase 1 (core) tests cover, in order: decisions and reconcile, filter rule
parsing and patterns, plan construction/fingerprints/freshness, generated rsync
argv, then profiles and XDG paths.

Phase 5 adds UI tests against real widgets with offscreen Qt. A fake rsync
executable on `PATH` stands in for the backend; no backend abstraction layer is
introduced for testing (ADR-008).
