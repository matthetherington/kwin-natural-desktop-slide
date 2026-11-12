# Gesture internals and diagnostics

<!-- Intended audience: developers investigating gesture tracking, input ownership and rendering. -->

For everyday behaviour, see [How swipes work](GESTURES.md).

The renderer and input controller are separate, disabled-by-default KWin plugins.
The controller temporarily takes gesture ownership from InputActions, if installed,
without editing its bindings. With no InputActions plugin, it takes ownership from
native KWin handling until unloaded. It requires a supported renderer.
Shared desktops preview/switch all outputs together. With independent desktops,
the output active at gesture begin is controlled; follow-ups stay pinned to it.
Output identity, geometry, scale, transform or desktop-policy changes cancel and
drain the owned gesture. Translated painting is clipped per output.
See [development](DEVELOPMENT.md) for the build/install/session-test sequence.

## Tracking rules

- Exactly three fingers initiate. After 8 raw movement units, acquire horizontal
  motion when `abs(x) >= abs(y)` (inclusive ±45°); lock that axis thereafter.
- Physical right selects the right-hand desktop. Progress uses 300 input units
  per desktop by default; reversals remain continuous and partial swipes can abort.
- Slow releases commit at 30%. Acquired same-direction flicks at or above
  1 desktop/second can commit independently of distance. Pauses and movement back
  toward the origin do not qualify as flicks.
- Velocity uses up to 100 ms and at most 32 samples. Invalid data/timestamps cancel
  safely. A new gesture snapshots settings; reloads never alter its tracking midway.
- Vertical three-finger streams are consumed, not handed to another gesture owner.
  Streams started during foreign fullscreen effects are drained without switching.
- Re-entry can interrupt the controller's own settling renderer. The renderer
  captures its current spring position once as the new visual origin, including
  during partial returns and edge bounces. New cumulative finger displacement is
  applied from there without snapping to the selected desktop. Completion and
  neighbour selection still use only the new gesture's input, not inherited
  visual displacement. A new release discards stale velocity from the old spring.

## Continuous finger-scroll follow-ups

Build with `ENABLE_SETTLING_UNDO=ON` (historical option name) and enable follow-ups
in settings. A released three-finger preview opens a follow-up window for exactly
its settling animation, including after partial return. Horizontal unmodified
touchpad **Finger** axis streams then use the same tracking/release model.

Horizontal axis-stop releases the follow-up. Each switch or partial return may
reopen the window while settling; there is no follow-up count limit. An admitted
stream remains owned until its own stop. New three-finger input supersedes it;
context changes cancel and drain stale tails. Ordinary scrolling resumes once the
renderer fully settles. Vertical, wheel and modified scrolling cannot initiate it.

A recognized blocked three-finger edge swipe with no animation instead opens a
400 ms entry window. Cancelled/rejected swipes cannot open it, unrelated scrolling
cannot extend it, and admitted follow-ups use the normal animation window.

**Tradeoff:** ordinary horizontal trackpad scrolling during that window can control
desktops. High-level finger-scroll events cannot reliably prove two-finger count
or uninterrupted contact. This is not palm rejection or raw finger-drop tracking.

## Overscroll

The unified renderer admits resisted offsets approaching 10% of a
desktop at missing neighbours. Release always returns unless movement reverses
into an available neighbour. Wrapping uses real neighbours instead of overscroll.
No extra desktops, panels or panel contents are created.

- **Solid colour:** fills the exposed strip at wallpaper depth, leaving fixed
  panels/windows above it. Black is the default; the settings picker offers RGB,
  hexadecimal and HSL controls. The stored mode remains `"black"` for compatibility.
- **Adjacent wallpaper:** translates a wallpaper-only neighbouring tile, clipped
  to the gap. Its rightmost edge enters first at the left boundary, mirrored at
  the right boundary. It is neither a stretched crop nor a stationary backdrop.

Disabling overscroll restores edge clamping. Settings and defaults are documented
in the [configuration reference](CONFIGURATION_REFERENCE.md#gestures-and-animation-settings).

For manual comparisons, use `overscroll-enable --background black` or
`overscroll-mode --background wallpaper` while idle. These override the stored
mode only until renderer reload/unload; omitting the override uses saved settings.

The manual helper saves the original renderer and whether InputActions was active under ignored
`local/session/renderer-session.json` before changing effects.
`overscroll-rollback` restores **both** renderer and original gesture handling;
`controller-rollback` alone leaves the renderer unchanged. Activation failures
attempt full recovery; malformed or conflicting baselines are refused.
For timed development tests, schedule `overscroll-rollback` before activation.
For an installed login service, use [service recovery](STARTUP.md) instead.

Changed libraries need fresh renderer/controller identities in the same KWin
session. Never assume installing or unloading replaces cached code. Do not change
build manifests during a manual test or retire startup-pinned plugins.

## Read-only diagnostics

```sh
qdbus-qt6 org.kde.KWin /org/kde/KWin/Effects/NaturalDesktopSlideController \
  org.kde.KWin.NaturalDesktopSlideController.controllerStatus
qdbus-qt6 org.kde.KWin /org/kde/KWin/Effects/NaturalDesktopSlide \
  org.kde.KWin.NaturalDesktopSlide.configurationStatus
```

Status includes latest admission/release/interruption reasons, counters,
follow-up/edge-window state, progress, velocity and settings. It is not a complete
event trace and cannot establish a driver's palm classification.

An optional passive `InputEventSpy` probe helps diagnose input routing:

```sh
cmake -S . -B build-observer -DBUILD_KWIN_EFFECT=ON -DENABLE_GESTURE_OBSERVER=ON
cmake --build build-observer --parallel 2
ctest --test-dir build-observer --output-on-failure
# Only after approval to install/load diagnostic code:
pkexec /usr/bin/python3 "$PWD/tools/install-settings.py" --probe-only --build-dir "$PWD/build-observer"
qdbus-qt6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin_natural_desktop_slide_probe
qdbus-qt6 org.kde.KWin /org/kde/KWin/Effects/NaturalDesktopSlideProbe \
  org.kde.KWin.NaturalDesktopSlideProbe.gestureStatus
qdbus-qt6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin_natural_desktop_slide_probe
```

The probe observes before filters without consuming input or calling the renderer.
It keeps latest-state data only, not per-event logs, keyboard data or window titles;
locking clears/suspends it. Keep private captures under ignored `local/`.
