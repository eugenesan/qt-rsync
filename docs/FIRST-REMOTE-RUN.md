# First remote-machine run (example)

This is a runbook for comparing qt-rsync with qpy-rsync on the machine that owns
the real SSH profile. The target machine has the same software, so no ABI issues
are expected; the details below are what matter.

## 0. Use the shared binaries (no clone or rebuild)

Both project directories are mounted on the target machine, so run the exact
binaries built on the dev machine — no rebuild, no copy, no sync worry.

- qt-rsync binary: `<project-root>/qt-rsync/build/bin/qt-rsync`
- qpy-rsync launcher: `<project-root>/qpy-rsync/qpy-rsync.sh`
  (it uses `/usr/bin/python3` and sets `PYTHONPATH` from its own directory; its
  native view extension is already built at
  `<project-root>/qpy-rsync/src/qpy_rsync/native_hybrid.so`)

If your home path on the target machine matches this one, the absolute paths
are:

```sh
/home/user/Develop/src/qt-rsync/build/bin/qt-rsync
/home/user/Develop/src/qpy-rsync/qpy-rsync.sh
```

Push later for history only; it is not needed to run the shared binaries.

## 1. Environment (identical for both apps)

Use the **real** XDG directories — do not override `XDG_CONFIG_HOME`,
`XDG_STATE_HOME` or `XDG_CACHE_HOME` — otherwise qt-rsync's one-time migration
cannot find the qpy-rsync profile.

```sh
export DISPLAY=:50                # the KDE session on that machine
export XDG_CURRENT_DESKTOP=KDE
unset QT_QPA_PLATFORM             # must NOT be offscreen for the visual run
export QT_XCB_GL_INTEGRATION=none
```

Also required for the SSH endpoint:

- a working SSH key/agent (or `ksshaskpass` on `PATH`);
- `rsync`, `ssh` and `setsid` on `PATH`;
- the session's `XAUTHORITY` / `DBUS_SESSION_BUS_ADDRESS` (normally inherited).

## 2. Binaries and working directory

| App | Command | cwd |
|---|---|---|
| qpy-rsync | `~/Develop/src/qpy-rsync/qpy-rsync.sh` | `~/Develop/src/qpy-rsync` |
| qt-rsync | `<build-dir>/bin/qt-rsync` | `~/Develop/src/qt-rsync` |

The qpy launcher uses `/usr/bin/python3` and sets `PYTHONPATH` itself; do not
activate a virtualenv. The Qt binary needs no Python.

## 3. Compare sequence

1. Launch qpy-rsync **without** `--dir-pair` and without `--compare`, so it
   loads the cached example profile:
   ```sh
   DISPLAY=:50 XDG_CURRENT_DESKTOP=KDE ~/Develop/src/qpy-rsync/qpy-rsync.sh
   ```
   Click **Compare** in the UI; wait for "Comparison complete" and note the
   change count, source/destination sizes and overview totals.

2. Launch qt-rsync **without arguments**:
   ```sh
   DISPLAY=:50 XDG_CURRENT_DESKTOP=KDE <build-dir>/bin/qt-rsync
   ```
   On first run it copies the qpy-rsync settings, profiles and cached profile
   path into `qt-rsync` locations. Confirm that Source and Destination show the
   same endpoints as qpy-rsync, then click **Compare**.

3. Compare the two windows: change count, source/destination item counts and
   sizes, overview totals, and the decision icons. They matched on both local
   fixtures used during development (6 and 106 changes).

4. Optional interaction checks: exclude one row with the checkbox (or Space),
   then check the selected count drops by one in both; try the context menu
   **Delete on Destination → Decision only (apply at sync)** and confirm the
   red × marker appears.

## 4. What to capture

Screenshots and logs from both apps:

```sh
import -window root /tmp/qpy-compare.png
import -window root /tmp/qt-compare.png
```

qt-rsync's journal is the bottom panel; qpy-rsync writes its session log under
`$XDG_CACHE_HOME/qpy-rsync/app-<profile>.log`. Send both screenshots plus the
change counts if anything differs.

## 5. Pitfalls

- **`--compare` needs `--dir-pair`** on both apps; for the SSH profile do *not*
  pass either flag — load the profile instead.
- SSH endpoints typed directly into the fields are parsed as
  `user@host:/path`; profile-loaded endpoints already carry the correct kind.
- The apps use separate profile copies after migration; edits made in one are
  not reflected in the other during the same session.
- Compare is a dry run and safe; **Synchronize is write-capable** — do not click
  it on the real tree while comparing.
