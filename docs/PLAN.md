# Status and remaining validation

<!-- Intended audience: contributors and maintainers tracking implementation and recorded validation. -->

This is a development record, not an installation or support promise.
For user-facing guidance, start with [compatibility](COMPATIBILITY.md).

## Implemented

- One Slide-derived renderer with selective Plasma panel movement and a shared
  standalone/KDE settings editor.
- Source settings installs include the standalone app and application-menu launcher,
  using the same entry points and CMake/KDE destinations as release packages.
  Runtime-only installs still leave settings components untouched.
- Beginner-facing settings feedback uses short summaries with optional technical
  details rather than raw JSON. Both tabs explain Save/Apply, block applying
  unsaved edits, and use simpler labels; gesture help is visible inline and
  dependent controls are disabled without losing their values.
- Reversible three-finger tracking, 30% slow completion, velocity-based flicks and
  repeatable horizontal finger-scroll follow-ups until settling finishes.
- Position-continuous visual handover when a new gesture interrupts a settling
  switch, partial return or edge bounce; release decisions remain input-based.
- Resisted edge overscroll, defaulting to **10%**, with spring return and a solid
  colour or moving adjacent wallpaper. Explicitly saved limits are preserved.
- Independent animation-speed settings, atomic configuration saves, diagnostics,
  guarded session handoff/recovery and a pinned systemd login service.
- Audience-based documentation: a short public README, separate user guides and
  technical references, a complete source-build guide and a dedicated maintainer
  release guide. [Documentation index](README.md) maps each guide to its audience.
- Multi-output shared preview/switching and independent-output targeting, with
  pinned follow-ups, scoped cancellation and topology/scale/transform invalidation.
- Output-local painting, including windows intersecting more than one output.
- Source-reviewed KWin 6.7.0–6.7.6 / 6.8.0–6.8.2 allowlist, legacy/modern
  paint/input/output-result adapters and exact build/runtime guards.
- Project-owned spring solver removes the private SpringMotion layout/export
  dependency while preserving the native trajectory and handover physics.
- Native RPM/DEB/Arch package generation, disabled-by-default user integration,
  tag/version/changelog release checks, checksums and complete-matrix release gates.
  Target matrix: Fedora 45, Debian sid, Ubuntu 26.10 (x86_64/ARM64), Arch (x86_64).
  Package publishing has not been run on GitHub yet; see [BINARY_PACKAGES.md](BINARY_PACKAGES.md).

## Validation boundary

CI layout portability follow-up: reproduced Debian 13 / Qt 6.8.2 ARM64's exact
10-pixel horizontal overflow at 540 × 360 with 18-point text. The overflow checkbox
now uses a shorter label while preserving its full explanation. Wrapped labels
also enforce their height-for-width minimum after width/font/style changes to
avoid Qt 6.8 form-row underallocation. Coverage retains the zero-horizontal-scroll
and unclipped-text assertions and adds DejaVu Sans and live font-change cases.
All 23 standalone suites pass in an isolated Debian 13 ARM64 / Qt 6.8.2 container
with GCC 14 and Clang 19. Host Qt 6.11.2 builds pass 23 standalone and 27 effect
suites. Hosted x86_64/Ubuntu Qt 6.8.3 reruns remain pending; no host installation
or live compositor changes were made.

Gesture-help layout follow-up: explanations use full-width form rows with explicit
height-for-width sizing, rather than nested spin-box-width fields. The edge section
is titled **Overflow settings**. Regression coverage checks full-width, unclipped
help and non-overlapping rows at multiple window widths and normal/large fonts.
All 23 standalone and 27 effect-build suites pass, including narrow large-font
layouts without horizontal scrolling. No host installation or live compositor
testing was performed for this follow-up.

Settings UX: 23 standalone and 27 effect-build suites pass locally, including
success/error/deferred/malformed diagnostic summaries, collapsed technical details,
unsaved-apply guards, dependent controls and small/normal/tall window layout checks.
Both standalone and KCM binaries were rebuilt. No host installation or live
compositor reload was performed for this change.

Settings-entry-point parity: 22 standalone and 26 effect-build suites pass locally.
Private staging verifies matching settings-module, standalone-app and menu-launcher
bytes/permissions between the source installer and CMake/package install rules.
No host installation, effect activation or live compositor testing was performed
for this change.

Live-tested on Fedora Asahi, aarch64, KWin 6.8.2 Wayland: panel selection/sliding,
tracking/reversal/aborts, flick completion, continuous follow-ups, blocked-edge
grace, black overscroll and moving adjacent wallpaper. Runtime rename/activation
and migrated settings paths were verified without restarting KWin.
The user subsequently reported successful reboot/login and suspend/resume on that
same system. Cross-system lifecycle validation remains outstanding.

Automated coverage includes policy, configuration, input models, overscroll
geometry, colour selection, UI layout, metadata, ABI and mocked install/recovery.
See [compatibility](COMPATIBILITY.md) and [publication constraints](MAINTAINERS.md).
New work is built in external temporary directories. The user accepted the live
`devlive20261111p1` renderer/controller test and approved making it the login default.
The installed service now pins those tested binaries and adopted the running pair
without unloading/reloading effects. Its managed recovery baseline restores stock
Slide and the prior gesture owner. Saved panel/gesture settings and kwinrc were
verified unchanged; this build's subsequent reboot/login validation remains pending.
The reported panel-edge line also occurs with stock Slide, disappears with Blur
disabled, and is avoided by the user's change from 170% to 175% scaling; no renderer
workaround was introduced. Standalone coverage includes output-context invalidation
and native-reference spring trajectories. Isolated two-/three-output KWin 6.8.2
tests cover shared/independent previews, commits, aborts, fractional scaling,
layout-change cancellation and per-output window-paint calls. They use software
OpenGL and synthetic input, not physical monitor/device validation. Host loaded
effects and saved settings are unchanged. Native ARM64 container builds now also
pass against Debian sid KWin 6.7.4 and Ubuntu 26.10 KWin 6.7.5 SDKs; no live desktop
validation has been done on those releases. Other allowlisted versions remain
source-reviewed only.

Latest packaging validation: **22 standalone suites** and **26 full package-build
suites** pass. Debian/Ubuntu ARM64 packages also install and pass CLI smoke tests
in isolated containers; Fedora ARM64 RPM generation and unsigned container-only
installation/CLI smoke tests pass. RPM publishing requires a dedicated signing key.
RPM signing and final signature/asset verification were tested in an Ubuntu 24.04
container with an ephemeral fixture key; the tested binary payload was unchanged.
Release infrastructure remains unpublished; x86_64/Arch jobs await hosted CI.
The first hosted standalone GCC jobs passed; Clang jobs exposed alias-template
deduction in a panel-policy test. Using explicit `QVector<PanelRule>` fixes it.
Local Clang 19.1.7 / Qt 6.8.2 ARM64 builds now pass with widgets ON (22 suites)
and OFF (19 suites); GCC passes both configurations too. Hosted Clang reruns remain
pending after pushing the fix.
The previous **24 integration-build suites** and both virtual-output integration
runs passed before packaging-only changes; no compositor painting/input code changed.
Production and integration renderers have no dynamic `KWin::SpringMotion`
references. Exact runtime mismatch refusal is covered by automated tests.
Portability tests exercise legacy/modern adapter contracts and synthetic touchpad
event rates, coalescing, sensitivity and direction. CI now defines Debian/Fedora
core jobs on native x86_64/ARM64 runners and a Fedora x86_64 effect build; hosted
results are pending. Distribution dependencies/touchpad tuning are documented in
[PORTABILITY.md](PORTABILITY.md).
Debian 13 ARM64 container validation passes all **21** standalone suites using
GCC 14.2 / Qt 6.8.2. A non-UTF-8 minimal-container locale exposed a diagnostic
stderr-test assumption; tests now set a deterministic UTF-8 locale. The
no-widgets/no-diagnostic build also passes all **18** applicable suites locally.

## Outstanding

- Broader stress testing of gesture handover, including rapid reversals/aborts;
  initial live gesture/visual testing was accepted by the user.
- Visual testing of configurable colours, HDR, scaling, transforms and interrupted
  return animations; broader speed tuning, wrapping, blur, shadows and popups.
- Actual compositor-crash recovery; reboot/login and suspend/resume on other systems.
- Live desktops on other distributions, x86_64 hardware and non-Apple touchpads.
- Physical multi-display/GPU/connector validation, rotated/HDR outputs and real
  hotplug timing; broader gesture follow-up/handover integration scenarios.
- Live runtime validation on KWin 6.7; build/runtime validation on remaining
  source-reviewed versions including 6.8.0/1; a larger port for 6.6 or
  earlier, and further private-API reduction.

Finger-scroll events cannot prove contact count or provide palm rejection.
Panel matching is geometry-based and needs
re-picking after layout changes. Each binary remains tied to its exact KWin build.
