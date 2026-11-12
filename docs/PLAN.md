# Features, validation and remaining work

<!-- Intended audience: contributors and testers checking feature scope and validation limits. -->

This page describes implemented features and the limits of their validation, not
an installation or support promise. For system requirements, start with
[compatibility](COMPATIBILITY.md).

## Feature scope

- Selective Plasma panel movement, with shared panel contents across desktops.
- A shared standalone/System Settings editor for panel choices, gestures and
  animation settings, with explicit saving and applying, atomic saves and backups.
- Reversible three-finger tracking, slow-swipe completion, velocity-based flicks
  and horizontal finger-scroll follow-ups while the animation settles.
- Position-continuous handover when a gesture interrupts a switch, partial return
  or edge bounce; release decisions use the new gesture's input.
- Resisted edge overscroll with spring return, a configurable solid colour or
  moving adjacent wallpaper, and independent animation-speed controls.
- Shared multi-output switching and independent-output targeting, with follow-ups
  pinned to their original output and cancellation when the output context changes.
- Guarded activation, login startup and recovery that preserve stock Slide and
  do not edit InputActions bindings.
- Native RPM, DEB and Arch package generation, exact KWin build/runtime guards,
  signing/checksum checks and release gates covering every declared package target.

See [gestures](GESTURES.md), [settings](CONFIGURATION.md) and
[release packaging](MAINTAINERS.md) for behaviour and usage details.

## Validation coverage

### Real desktop

Physical desktop validation is limited to **Fedora Asahi, aarch64, KWin 6.8.2
Wayland**. It covers panel selection and sliding, tracking and reversal, aborted
swipes, flick completion, continuous follow-ups, blocked-edge grace, solid black
overscroll and moving adjacent wallpaper. Reboot/login and suspend/resume have
also been reported working on that system. These results do not establish
cross-system lifecycle or hardware support.

### Native builds and standalone tests

Native ARM64 SDK builds and automated tests cover Fedora 45 / KWin 6.8.2,
Debian sid / KWin 6.7.4 and Ubuntu 26.10 / KWin 6.7.5. Standalone core, UI and
tool tests also run without compiling or loading compositor plugins, including
Debian 13 / Qt 6.8.2 with GCC 14 and Clang 19.

Automated coverage includes:

- Panel policy, configuration validation, backups and atomic saves.
- Input tracking, velocity, release decisions, gesture handover and output-context
  invalidation, using synthetic event streams rather than physical touchpads.
- Spring trajectories compared with KWin's solver, settling and gesture handover.
- Overscroll geometry, colour selection and legacy/modern KWin API adapters.
- Settings feedback, unsaved-edit guards, dependent controls and layouts at
  narrow window sizes and larger fonts.
- Narrow gesture-editor layout with 18pt DejaVu Sans (installed explicitly in
  CI); the reload action uses a compact caption to avoid horizontal overflow.
- Plugin metadata, ABI guards, source/package settings-install parity and mocked
  activation/recovery failures.
- Explicit plugin installation permissions (`0644`) shared by source and CMake
  installs, with CI exercising both `CMAKE_INSTALL_SO_NO_EXE` defaults.
- Source-installed login startup from an empty renderer state using a validated
  recovery baseline, including clean-stop/next-login, gesture-owner conflicts and
  failed direct activation. These are mocked tests; the reboot fix still needs
  a real reboot/login check.

Standalone tests do not compile compositor code. Full effect/KCM builds and
their tests are a separate validation step; see [build commands](DEVELOPMENT.md#build-and-test).

### Isolated compositor tests

Two- and three-output KWin 6.8.2 tests cover shared and independent previews,
commits, aborts, fractional scaling, layout-change cancellation and per-output
window-paint calls. They use software OpenGL and synthetic input. They check
rendering paths, not pixel correctness, physical monitors or device behaviour.
See [isolated integration tests](DEVELOPMENT.md#isolated-multi-output-integration-tests-approval-required).

### Packaging and version review

Release 0.1.1 synchronizes the CMake and four plugin versions with its changelog;
the maintainer checklist includes a local run of the tag-only metadata gate.

ARM64 Debian/Ubuntu and Fedora packages have container-only installation and CLI
smoke-test coverage. RPM signing and final asset verification have fixture-key
coverage; a production release requires a dedicated signing key and successful
checks for every declared target. Local package tests do not establish that
published assets exist or that all hosted CI targets pass. Consult
[GitHub Releases](https://github.com/matthetherington/kwin-natural-desktop-slide/releases)
for available downloads and their per-asset manifests.

The reviewed source-version range is **KWin 6.7.0–6.7.6 and 6.8.0–6.8.2**.
Versions without native build or desktop testing remain source-reviewed only.
Every binary requires its exact build-time KWin runtime; distribution patches
can require additional review. See [upstream dependencies](UPSTREAM.md).

## Remaining validation and work

- Stress testing of rapid gesture handovers, reversals and aborts.
- Broader visual testing of configurable colours, animation speeds, wrapping,
  interrupted returns, blur, shadows and popups.
- Physical multi-display/GPU/connector testing, including fractional scaling,
  rotated/HDR outputs, colour management and hotplug timing.
- Actual compositor-crash recovery and lifecycle testing on other systems.
- Live desktops on other distributions, x86_64 hardware and non-Apple touchpads.
- Live KWin 6.7 validation and native build/runtime testing of the remaining
  source-reviewed versions.
- A separate port for KWin 6.6 or earlier, and further reduction of private-API
  dependencies.

Finger-scroll events cannot prove contact count or provide palm rejection.
Geometry-based panel matching requires re-picking after layout changes. These
are current limitations, not guarantees that broader testing will remove them.
See [portability](PORTABILITY.md) for hardware-test reporting and
[maintainer checks](MAINTAINERS.md#before-claiming-wider-support) before claiming
support for another system.
