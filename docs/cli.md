# Command line options

qt-rsync has one executable, `qt-rsync`. These options are carried over from
the qpy-rsync GUI launcher; there is no separate headless CLI program.

| Option | Meaning |
|---|---|
| `--dir-pair SOURCE DESTINATION`, `-d` | Populate Source and Destination fields. Required with `--compare`. |
| `--backup DIRECTORY` | Populate the backup directory. |
| `--mode MODE` | `update` (default), `mirror` or `new-only`. |
| `--compare` | Start a comparison after the event loop initializes. |
| `--log-file PATH` | Override the persistent session log path. |
| `--no-log-file` | Disable persistent session logging for this launch. |
| `--version` | Print version and build, then exit. |
| `--help` | Print usage. |

`--log-file` and `--no-log-file` are mutually exclusive. Unrecognised arguments
are forwarded to Qt, matching the previous launcher's pass-through behaviour.

Qt/remote-X environment preparation (software rendering, no GLX probes,
suppressed benign Qt messages) belongs in `app::run`, not in a wrapper script.
