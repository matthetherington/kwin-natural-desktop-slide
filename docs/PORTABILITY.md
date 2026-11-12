# Distribution, architecture and touchpad portability

<!-- Intended audience: developers porting the effect and testers checking new hardware. -->

For an end-user support summary, see [compatibility](COMPATIBILITY.md).
See [validation coverage](PLAN.md) for test scope and remaining work.

## What is supported versus validated

The implementation uses Qt/KWin/libinput capabilities, not Apple vendor IDs,
Asahi APIs, RPM commands or architecture-specific instructions. Build natively
for x86_64 or aarch64; never copy an ARM plugin to an x86_64 installation.

Full live validation remains Fedora Asahi/aarch64/KWin 6.8.2 only. Other
distributions and non-Apple touchpads are intended targets, not hardware-tested
claims. CI defines Debian 13/Fedora 45 core builds on both architectures and a
Fedora 45 x86_64 effect build. A configured CI target is not evidence of a passing
build or hardware support. Debian's core tests do not mean its stock KWin version
can load this effect.

Standalone core/UI/tools validation includes Debian 13 ARM64 containers with
Qt 6.8.2, GCC 14 and Clang 19. These tests do not load the effect or access the
host compositor/session.

KWin source-reviewed ports: **6.7.0–6.7.6 and 6.8.0–6.8.2**. Native ARM64 SDK
build/test coverage includes Debian sid 6.7.4, Ubuntu 26.10 6.7.5 and Fedora 45 6.8.2.
Only 6.8.2 is live-validated. KWin 6.6 and earlier are not yet
ported. An older distribution needs a supported Plasma/KWin repository, not a
forced version override. Install matching development packages and rebuild after
every KWin update. Downstream private-ABI changes can require further review.

## Distribution dependencies

These commands install dependencies, not the effect. Review them before running.
Package availability and KWin versions depend on the enabled repositories.

Fedora (full effect):

```sh
sudo dnf install cmake make gcc-c++ python3 kwin-devel extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kcmutils-devel kf6-kcoreaddons-devel
```

Debian 13 (standalone core/UI/tools only):

```sh
sudo apt install cmake make g++ python3 qt6-base-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_KWIN_EFFECT=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For a full build on Debian/Ubuntu, obtain a supported KWin version and its
matching SDK (`kwin-dev` where provided), Qt Declarative development files,
ECM and KF6 KCMUtils/CoreAddons development packages meeting the
[source-build requirements](DEVELOPMENT.md#requirements).
Some distributions omit private headers or CMake targets; do not substitute
headers or libraries from a different KWin build. Arch/openSUSE packages likewise
need a matching SDK before they can be advertised as validated targets.

Follow [development](DEVELOPMENT.md) for building/testing and installation. The
installer takes KDE's plugin directory from CMake, including `lib`, `lib64` and
multiarch layouts. It does not assume RPM or change the desktop automatically.
If `pkexec` is unavailable, the documented installer also works under `sudo`.
Immutable systems need native packaging/layering, not a Flatpak-contained plugin.
Login automation currently requires systemd user services; use manual session
activation/recovery instead on other session managers.

## Non-Apple touchpads

A touchpad must expose libinput three-finger swipe events for initiation.
Horizontal finger-axis events enable follow-ups while the animation is settling.
Touchpads that expose only pointer movement/two contacts cannot gain three-finger
recognition from this plugin. Mouse wheels, synthesized autoscroll and non-finger
axis sources are not follow-up gestures. No vendor whitelist is used.

Keep the tuned defaults initially. If a different pad feels too slow or fast,
adjust **distance per desktop** in the gesture settings: lower means less travel
to switch; higher means more travel. Flick speed is measured in desktops/second,
so calibrate distance first, then the flick threshold. Animation speed changes
settling only, not finger displacement.

Synthetic regression streams cover 1/4/8/10/20 ms event intervals, three distance
scales, both directions, diagonal noise, coalesced timestamps, flick/slow release
and contact-loss cancellation. These are not recordings of specific touchpads
and do not validate firmware, palm rejection or natural-scroll behaviour.

## Before reporting a successful hardware test

Record distribution, architecture, exact KWin/Qt versions, touchpad model/driver,
display layout and whether InputActions is installed. Save work before testing.
Check three-finger acquisition, reversal, partial abort, short flick, repeated
finger-scroll follow-ups, both scroll directions, lock/unlock and suspend/resume.
Also verify fixed/selected panels and rollback to stock Slide. Do not publish
raw input recordings, window titles or device serials; keep private diagnostics
under ignored `local/`. See [compatibility limits](COMPATIBILITY.md).
