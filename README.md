# KWin Virtual Desktop Natural Swipe

Continuous desktop swipes for KDE Plasma Wayland, with selected panels sliding
alongside desktops and other panels staying fixed.

- Three fingers initiate; horizontal two-finger follow-ups work until settling ends.
- Reversible tracking, partial-swipe cancellation and velocity-based flick completion.
- Resisted edge overscroll with a selectable solid colour or adjacent wallpaper.
- Shared KDE/standalone settings, including an RGB/hex and HSL colour picker.

**Experimental: KWin 6.8.2 only; gesture control currently requires one display.**
InputActions is optional. Save your work before live testing: a compositor crash can end the
Wayland session. Stock Slide is never replaced.
Not Mac-specific, but only Fedora Asahi has been live-tested. See
[portability and KDE Store readiness](docs/COMPATIBILITY.md).

## Build and install

Requires CMake 3.25+, a C++23 compiler, Qt 6 and matching KWin/KDE development
packages. See [dependencies and build options](docs/DEVELOPMENT.md).

```sh
# Full build, including settings, controller and both renderers
cmake -S . -B build-controller -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_KWIN_EFFECT=ON -DENABLE_GESTURE_CONTROLLER=ON \
  -DENABLE_SETTLING_UNDO=ON -DENABLE_OVERSCROLL_RENDERER=ON \
  -DSWIPE_CONTROLLER_ID=kwin_natural_swipe_controller_preview17 \
  -DOVERSCROLL_RENDERER_ID=kwin_natural_slide_preview4
cmake --build build-controller --parallel 2
ctest --test-dir build-controller --output-on-failure

# Inspect destinations, then install (sudo may replace pkexec); no activation
python3 tools/install-settings.py --all --build-dir build-controller --dry-run
pkexec /usr/bin/python3 "$PWD/tools/install-settings.py" --all --build-dir "$PWD/build-controller"
```

For updates within the same KWin session, use **fresh plugin identities**; KWin can
cache old libraries. Do not change manifests during an active manual test or
overwrite identities pinned by an installed startup service.

## Configure and try

1. Fully reopen System Settings → **Animations** → virtual-desktop switching →
   **Selective Panel Slide (Experimental)** → configuration. Alternatively, run
   `build-controller/natural-swipe-config`.
2. Add panels by clicking them, tick those that should slide, and save.
3. In **Gestures and animation**, choose your background colour and other options,
   then save. Black and the tuned gesture defaults remain the defaults.

From stock Slide or the normal project renderer, with InputActions loaded **if
installed** (systems without it use native KWin gesture handling):

```sh
python3 tools/effect-session.py overscroll-enable
# Restore the original renderer and InputActions:
python3 tools/effect-session.py overscroll-rollback
```

Use **Reload** after later settings changes. Picking/saving does not enable effects.
The above activation is **session-only**. For an existing managed login setup, use
[startup and service recovery](docs/STARTUP.md), not the manual activation commands.
Never enable competing desktop-switching effects together.

## More information

- [Settings, defaults and diagnostics](docs/CONFIGURATION.md)
- [Gesture behaviour and limitations](docs/GESTURES.md)
- [Development and recovery](docs/DEVELOPMENT.md) · [Validation status](docs/PLAN.md)

Copyright © 2026 Matt Hetherington. **GPL-2.0-or-later**; see [LICENSE](LICENSE).
Compiled plugins carry KDE `KPlugin` metadata in `effect/*metadata.json` and
`kcm/metadata.json`, including author, version, licence, repository and issue URLs.
This is a native KWin plugin, not a Plasma applet/KPackage.
See [upstream provenance](docs/UPSTREAM.md).
