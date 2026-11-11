# Development, installation and recovery

## Build options

Use the full effect build from [README](../README.md) to test compositor and KDE
integration. On Fedora, the main development packages are `cmake`, `gcc-c++`,
`qt6-qtbase-devel`, `extra-cmake-modules`, matching `kwin-devel`,
`kf6-kcoreaddons-devel` and `kf6-kcmutils-devel`. KWin's exported CMake target
requires additional Qt/KDE/Wayland/graphics development dependencies and C++23.
The effect is pinned to **KWin 6.8.2**; see [ABI notes](UPSTREAM.md).

Build just the picker, diagnostics and tests without KWin:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_KWIN_EFFECT` | OFF | Compile the pinned compositor plugin. |
| `BUILD_CONFIGURATOR` | ON | Standalone Qt Widgets panel picker. |
| `BUILD_SETTINGS_MODULE` | ON | KDE module, when effect and configurator are built. |
| `BUILD_DIAGNOSTIC` | ON | Read-only CLI diagnostics; live discovery needs `kscreen-doctor`. |
| `ENABLE_GESTURE_OBSERVER` | OFF | Build a separate passive probe plugin; no animation control. |
| `ENABLE_GESTURE_CONTROLLER` | OFF | Build the separate opt-in input owner/animation bridge. |
| `ENABLE_SETTLING_UNDO` | OFF | Continuous finger-scroll follow-ups while settling; historical option name. |
| `ENABLE_OVERSCROLL_RENDERER` | OFF | Build a separately identified wallpaper/solid-colour overscroll preview renderer. |
| `BUILD_TESTING` | ON | Qt tests and, if Python 3 is available, helper tests. |

For Core/Test-only builds, disable configurator and diagnostic. Tests requiring
widgets use Qt's offscreen backend; they do not load an effect into KWin. Build
outputs and private diagnostic/backup files stay in ignored `build*/` and `local/`.

## Layout

- `effect/`: Slide-derived rendering, transitions and D-Bus diagnostics.
- `src/`: Qt Core configuration/matching/picker model, shared widget UI and CLIs.
- `kcm/`: KDE configuration-module wrapper around the same UI.
- `tests/`: policy, parser, CLI, UI layout, metadata, ABI and helper tests.
- `tools/`: guarded session switching and atomic installation using CMake/KDE paths.

Keep gesture recognition separate from panel policy. Preserve the renderer's
`desktopChanging`, completion and cancellation paths when adding a controller.
The [gesture tracker/probe](GESTURES.md) can be installed independently of the
renderer using `tools/install-settings.py --probe-only` from `build-observer`.

## Installation and updates

For the complete build/installation use [README](../README.md). For normal-renderer
or settings-only development, build/test `build-effect`, then run from the repository root:

```sh
pkexec /usr/bin/python3 "$PWD/tools/install-settings.py"
```

Review the installer before granting privileges. It checks installed KWin's
version using `kwin_wayland --version` (no RPM dependency), reads the generated
CMake install layout and atomically installs only the selected project components.
Default normal-build paths on the original Fedora machine are:

```text
/usr/lib64/qt6/plugins/kwin/effects/plugins/kwin_natural_slide.so
/usr/lib64/qt6/plugins/kwin/effects/configs/kwin_natural_slide_config.so
```

Files are staged beside their destinations and renamed, never truncated in
place. Already running processes may retain the **old** library mapping/factory;
installation does not hot-reload code. Reopen System Settings for updated metadata.
For renderer updates, plan an approved reload/login test; never assume unloading
an effect unloads its shared library. Do not restart Wayland KWin over unsaved work.

The module is advertised only when built. The standalone picker remains usable
without it. Opening settings, installing files and saving panel rules do not
enable an effect or modify InputActions. Keep stock Slide installed.

For a settings-UI-only update after building `build-effect`, use
`pkexec /usr/bin/python3 "$PWD/tools/install-settings.py" --settings-only`.
This replaces only the project KCM, not any renderer/controller. Fully close and
reopen System Settings to avoid its cached KCM library. The new standalone editor
can instead run straight from the build directory, without installation.

Use `--build-dir` for a custom build location, `--all` for a complete full build,
`--dry-run` for a read-only preview or `--destdir` for unprivileged package staging.
See [portability and packaging](COMPATIBILITY.md). Never install into a Qt plugin
directory belonging to a container rather than the running host compositor.

## Session testing

Save your work and keep a rollback terminal ready. Do not load both Slide effects.

```sh
python3 tools/effect-session.py status
python3 tools/effect-session.py enable    # Requires stock Slide to be loaded
python3 tools/effect-session.py rollback # Unloads custom effect, restores stock
```

The helper verifies each transition and attempts rollback on load failure. It
does not write `kwinrc`. KDE's effect checkbox, unlike this helper, can change
startup preferences. Confirm matching with [diagnostics](CONFIGURATION.md), then
test transitions manually; a successful D-Bus response does not prove rendering.

For the separate overscroll preview, use the [gesture guide](GESTURES.md#overscroll-renderer-preview).
Its recovery command is `python3 tools/effect-session.py overscroll-rollback`:
this unloads the controller, restores InputActions, then restores the renderer
saved before the preview. `controller-rollback` alone intentionally leaves the
renderer unchanged and is not the complete overscroll-test rollback. Schedule
automatic recovery with `overscroll-rollback` for those tests. Emergency `rollback`
unloads all discoverable preview renderers and restores stock Slide instead.

## Recovery

For the installed persistent login setup, first use the
[service recovery instructions](STARTUP.md#restore-the-original-setup):
`systemctl --user disable --now natural-swipe-session.service` restores its saved
renderer and InputActions. Its private recovery state is separate from manual
timed-test state in this repository.

If the GUI is unusable but KWin responds, switch to a text console with
Ctrl+Alt+F3, log in as the same user, and run:

```sh
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export DBUS_SESSION_BUS_ADDRESS=unix:path=$XDG_RUNTIME_DIR/bus
python3 ~/Code/kwin-virtual-desktop-natural-swipe/tools/effect-session.py rollback
```

The helper cannot recover a crashed/unresponsive compositor. The plugin is
disabled by default, but if you enabled it persistently through KDE Settings,
disable it there or set `kwin_natural_slideEnabled=false` in the `[Plugins]`
group of `~/.config/kwinrc` before the next login. Do not repeatedly restart KWin
over unsaved work. Before uninstalling project files, confirm the custom effect
is unloaded; never remove stock KWin plugins. Actual crash recovery/uninstallation
have not been live-tested.
