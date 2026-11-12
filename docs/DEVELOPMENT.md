# Development and source builds

<!-- Intended audience: contributors and experienced users building from source. -->

For a prebuilt package, use the [installation guide](BINARY_PACKAGES.md) instead.
Release engineering is documented in [MAINTAINERS.md](MAINTAINERS.md).

## Requirements

- Plasma Wayland with KWin **6.7.0–6.7.6 or 6.8.0–6.8.2**, matching development
  headers and OpenGL rendering. X11 is unsupported. Rebuild against the exact
  runtime's development packages after each KWin update.
- CMake 3.25+, a C++23 compiler and Make or Ninja. Core/tools code uses C++20;
  the installed KWin SDK determines the native plugin's compiler requirements.
- Qt 6.5+ development packages (Core, DBus, Quick and Widgets), Extra CMake
  Modules 6.26+, and KDE Frameworks 6.26+ CoreAddons and KCMUtils development packages.
  The KWin SDK may require newer versions.
- Python 3 for install/session helpers; administrator access for plugin installation.
- A libinput/KWin-compatible touchpad for live gestures.

See [distribution dependency examples](PORTABILITY.md#distribution-dependencies).
Native builds/tests pass on KWin 6.7.4, 6.7.5 and 6.8.2; only 6.8.2 is
live-validated. Other allowed versions are source-reviewed only. See
[private ABI notes](UPSTREAM.md) and [validation status](PLAN.md).

## Build and test

Run these commands from the repository root. Standalone validation does **not**
compile the compositor plugins:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For a full effect/KCM build, also run:

```sh
cmake -S . -B build-effect -DCMAKE_BUILD_TYPE=Debug -DBUILD_KWIN_EFFECT=ON
cmake --build build-effect --parallel 2
ctest --test-dir build-effect --output-on-failure
```

Neither build installs plugins or changes your running desktop. Report standalone,
full-effect and live compositor testing separately.

`BUILD_KWIN_EFFECT` defaults OFF. Configurator, diagnostic and settings-module
options default ON, as do `ENABLE_GESTURE_CONTROLLER` and `ENABLE_SETTLING_UNDO`
(the historical name for continuous follow-ups). `ENABLE_GESTURE_OBSERVER` is OFF.
Overscroll is included in the single renderer; there is no separate preview target.

For same-session development, use fresh `_dev1`/`_dev2` suffixes through
`DESKTOP_SLIDE_RENDERER_ID` and `DESKTOP_SLIDE_CONTROLLER_ID`. Never assume file
replacement/unloading discards cached libraries. Do not install multiple
development renderers into a normal user setup.

## Isolated multi-output integration tests (approval required)

These launch separate headless KWin processes with software OpenGL, private XDG
directories and a separate D-Bus session. They never install plugins or address
the host compositor. Test hooks are compiled out by default; the installer refuses
integration-driver builds. Do not load these plugins in a normal session.

```sh
cmake -S . -B build-integration -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_KWIN_EFFECT=ON -DENABLE_INTEGRATION_DRIVER=ON \
  -DDESKTOP_SLIDE_RENDERER_ID=kwin_natural_desktop_slide_devintegration \
  -DDESKTOP_SLIDE_CONTROLLER_ID=kwin_natural_desktop_slide_controller_devintegration
cmake --build build-integration --parallel 2
ctest --test-dir build-integration --output-on-failure
python3 tests/nested_session_test.py --build-dir build-integration --outputs 2
python3 tests/nested_session_test.py --build-dir build-integration --outputs 3
```

The harness requires `kwin_wayland`, `dbus-run-session`, `qdbus-qt6` (or `qdbus6`/
`qdbus`) and writable `/tmp/opencode`. Window-paint counters check rendering paths,
not pixel correctness. Physical display/GPU/HDR and real-device testing remain needed.

## Install a source build

Inspect destinations before authorising installation:

```sh
python3 tools/install-settings.py --all --build-dir build-effect --dry-run
pkexec /usr/bin/python3 "$PWD/tools/install-settings.py" --all --build-dir "$PWD/build-effect"
```

The installer uses CMake/KDE paths and verifies `kwin_wayland --version`. Other
modes: `--settings-only`, `--runtime-only`, `--controller-only`, `--probe-only`;
select the correct `--build-dir`. `--destdir` stages for packaging without root.
It never restarts KWin or activates effects. If `pkexec` is unavailable, use `sudo`.
Select `--config Debug`/`Release` for multi-configuration builds. CMake generates
`install-layout-<configuration>.json` with actual target files and KDE paths;
reconfigure old build trees before using the installer.

Like release packages, the default source install, `--all` and `--settings-only`
install both settings entry points: the System Settings module and **Natural
Desktop Slide Settings** in the application menu (the `natural-desktop-slide-config`
command). `--settings-only` updates these without replacing renderer/controller
binaries, including any development identities pinned by your startup service.
Fully close and reopen System Settings after updating the module.

For staging without root or host runtime-version checks:

```sh
python3 tools/install-settings.py --all --build-dir build-effect --destdir "$PWD/stage"
```

Use distribution packages/layering on immutable systems. A custom prefix or a
Flatpak's plugin directory is not automatically a host KWin plugin path. See
[packaging constraints](MAINTAINERS.md#distribution-constraints).

Normal plugin IDs are `kwin_natural_desktop_slide`,
`kwin_natural_desktop_slide_config` and `kwin_natural_desktop_slide_controller`.
The optional diagnostic probe is `kwin_natural_desktop_slide_probe`.

## Configure and try a source build

1. Open `build-effect/natural-desktop-slide-config`, or fully reopen System Settings
   → **Animations** → virtual-desktop switching → **Natural Desktop Slide
   (Experimental)** → configuration on KWin 6.8.
2. Follow the [settings guide](CONFIGURATION.md) to pick panels and save.
3. **Save your work.** A compositor crash can end the Wayland session.
4. Start from stock Slide or the project renderer. If InputActions is installed,
   leave it loaded for the guarded handoff. Disable other conflicting gesture tools.
5. Run as your normal desktop user, **not** with `sudo`:

   ```sh
   python3 tools/effect-session.py start --build-dir build-effect
   ```

Try a horizontal three-finger swipe. To restore the recorded renderer and input owner:

```sh
python3 tools/effect-session.py stop
```

Use the editor's reload controls after saving later changes. Saving does not enable
effects. This manual activation is session-only; for an existing source-installed
login service, use [startup and recovery](STARTUP.md) instead. Do not mix its
private baseline with source-tree test state or enable competing desktop-switching
effects together.

### Updating code in the same session

KWin can cache loaded libraries even after unloading an effect. Use fresh renderer
and controller identities for same-session development, or log out/in before
trying replacements. Do not change build manifests during an active manual test,
overwrite startup-pinned identities or install multiple development renderers in
a normal setup. See [gesture internals](GESTURE_INTERNALS.md) for comparison modes
and input diagnostics.

## Recovery

```sh
# Managed login setup:
systemctl --user disable --now natural-desktop-slide-session.service
# Manual source-tree test only:
python3 tools/effect-session.py stop     # Restore recorded renderer/input owner
python3 tools/effect-session.py rollback # Emergency: restore stock Slide
```

Do not mix the installed service's private baseline with source-tree test state.
Never run competing animations/controllers. Helpers cannot recover a crashed or
unresponsive compositor. Save work before live tests. See [startup/migration](STARTUP.md).
Private captures/backups belong under ignored `local/`, builds under `build*/`.
