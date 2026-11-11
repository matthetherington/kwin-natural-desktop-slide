# Portability and KDE Store readiness

## Current support boundary

There is no Apple-, Asahi- or ARM-specific input/rendering code. The C++ targets
compile for the host architecture; Qt/KWin supply device input and OpenGL. However,
**only Fedora Asahi, aarch64, KWin 6.8.2 Wayland has been live-tested**. A successful
build or mocked test on another platform is not compositor validation.

| Area | Current position |
| --- | --- |
| CPU | No architecture-specific code; rebuild native plugins for each architecture. x86_64 live testing remains outstanding. |
| Distribution | Installer uses CMake/KDE plugin paths, not RPM or `/usr/lib64`. `lib`, `lib64` and Debian-style multiarch layouts have synthetic tests. |
| KWin | **6.8.2 exactly**. Private input/desktop APIs and SpringMotion layout prevent a generic Plasma 6 binary. |
| Session | Plasma Wayland. X11 is not supported/tested. |
| Graphics | Overscroll renderer requires OpenGL; QPainter/software compositors are refused. Mesa software OpenGL is not a validated performance target. |
| Displays | Gesture controller requires exactly one display. The guard remains intentional; do not advertise multi-monitor gesture support. |
| Trackpad | Requires libinput/KWin swipe and finger-axis events. Sensitivity/flick settings accommodate device differences; 300 units is not a universal physical distance. |
| InputActions | Optional. If discoverable it must initially be loaded for guarded handoff. If absent, unloading our controller restores native KWin handling. |
| Other gesture owners | Not automatically detected. Conflicting third-party input filters must be disabled by the user; do not promise universal coexistence. |
| Installation | Native Qt/KWin plugin search directory must be writable (usually administrator/package-manager installation). Arbitrary `~/.local/share/kwin/effects` installation will not load C++ plugins. |

### ABI safety

Keep the exact KWin version requirement. The compatibility declaration is used
only if the distribution omits `effect/springmotion.h`; a matching installed
header is preferred. Missing exported symbols fail at link time. Downstream
patches can still change a private ABI even at the same version.

Supporting another KWin release requires reviewing its headers, signal signatures,
input-filter ordering, rendering APIs and spring layout, then compiling and
live-testing a separately built plugin. Removing `EXACT` is not a port.

## Distribution-neutral builds and packaging

Use the full build in [README](../README.md). CMake generates
`install-layout-<configuration>.json` containing actual target files and KDE's
destination paths. Reconfigure existing build trees before using the installer.

```sh
# Read-only: verify build/version and inspect destinations before authentication.
python3 tools/install-settings.py --all --build-dir build-controller --dry-run

# Stage into a packaging root, without root privileges or modifying the session.
# This deliberately does not check the host's installed KWin runtime version.
python3 tools/install-settings.py --all --build-dir build-controller --destdir "$PWD/stage"
```

Select `--config Debug`/`Release` for multi-configuration builds. The session helper
also accepts `--build-dir` to read development plugin identities from a custom
build. Recovery state remains in the same private location; do not mix manual
sessions with the installed login service.

Package maintainers can instead use normal CMake installation with `DESTDIR` and
components `effect`, `settings`, `controller`, `overscroll` and optionally `probe`.
Do not bundle system Qt/KWin libraries. Declare matching KWin/private-ABI and
architecture dependencies, and rebuild after relevant KWin updates.

On immutable distributions, use their supported package/layering mechanism rather
than trying to write a read-only `/usr`. A container/Flatpak's plugin directory is
not the host KWin's plugin directory. Per-user compiled plugin installation needs
an explicitly supported Qt plugin search path; a custom prefix alone is not enough.

## KDE Store

**This is not currently a one-click KDE Store effect.** KDE's documented
`KWin/Effect` packages contain JavaScript/QML loaded through KPackage. Our native
shared libraries and private input filter cannot be installed that way merely by
adding a `package/metadata.json` or zipping the build directory.

Recommended paths:

1. **Near term:** publish source and clear compatibility requirements; provide
   distribution packages built for each supported KWin version and architecture.
   A Store listing may point users to those installation instructions, but must
   not claim normal “Get New Effects” installation works.
2. **For one-click Store installation:** investigate a JS/QML redesign against
   the supported effects/scripting APIs. First prove those APIs can provide the
   required selective panel painting and continuous gesture/finger-scroll ownership.
   A native companion would still require separately installed distro packages.
3. **Long term:** consider upstreaming gesture/rendering hooks or the feature
   itself, reducing dependence on unstable compositor internals.

References:
- [KDE developer guide: KWin effects and Store distribution](https://develop.kde.org/docs/plasma/kwineffect/)
- [KWin developer explanation of native-effect distribution constraints](https://blog.vladzahorodnii.com/2024/03/)

## Before claiming wider support

- Test x86_64 and aarch64 on at least two distributions with exact matching KWin.
- Exercise native gestures without InputActions and handoff/recovery with it.
- Check non-Apple touchpads, natural-scroll inversion, contact loss, short flicks,
  cancellation, lock/unlock, device removal and suspend/resume.
- Verify panel picking, fixed/selected panels, edge colours/wallpaper, fractional
  scaling, rotated outputs, HDR/colour management and interrupted returns.
- Reboot/login-test startup and rollback on each target distribution. The existing
  service assumes systemd user sessions and `plasma-kwin_wayland.service`; other
  session managers need a reviewed integration, not an unconditional autostart.
- Test package installation/removal and rebuild/update behaviour without replacing
  stock Slide or leaving competing gesture owners active.

CI builds standalone Qt models/UI/tools on Ubuntu x86_64 with GCC and Clang, with
and without widget dependencies. It does **not** install a KWin plugin, validate
other KWin releases or replace live GPU/session tests. The workflow is newly added;
its hosted runs have not yet been observed.
