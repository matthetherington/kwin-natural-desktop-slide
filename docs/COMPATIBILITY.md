# Compatibility and known limitations

<!-- Intended audience: users checking whether their system is suitable. -->

Developers porting to another system should also read [portability](PORTABILITY.md) and
[upstream API notes](UPSTREAM.md).

## What you need

- **KDE Plasma on Wayland.** X11 is not supported.
- A package built for your **distribution release, processor architecture and
  exact KWin package version**. There is no universal Plasma 6 download.
- A touchpad that provides three-finger swipe events. Apple hardware is not required.
- OpenGL desktop rendering. The alternative QPainter renderer is not supported.

Not sure which package fits? Check the [installation guide](BINARY_PACKAGES.md)
before downloading. Do not force an incompatible package to install.

## What has been tested

| Level of testing | Current coverage |
| --- | --- |
| Real desktop | Fedora Asahi, aarch64, KWin 6.8.2 Wayland only. |
| Native builds and automated tests | KWin 6.7.4, 6.7.5 and 6.8.2 on ARM64. This does not establish live desktop support on every system. |
| Source review | KWin 6.7.0–6.7.6 and 6.8.0–6.8.2. Other versions are rejected; review alone is not a successful build or desktop test. |
| Multiple screens | Two/three virtual screens, including shared and independent desktops. Physical multi-monitor testing remains outstanding. |

Release build targets also include x86_64 and other distributions. A target is
not a promise that a download exists or has been tested on physical hardware.
Detailed test records and pending CI checks are in [project status](PLAN.md).

## Updates need care

This effect uses internal KWin interfaces that can change between updates.
Each binary must match the KWin build it was made for. Packages deliberately
require an exact KWin package version; source users must rebuild after KWin updates.
Distribution patches can also require further review.

If an update is blocked by this package, wait for a matching rebuild or
[disable and remove the effect](BINARY_PACKAGES.md#upgrade-or-remove) before
updating KWin. Do not bypass dependency checks. Log out and back in before using
replacement plugins, since KWin can retain the old code in memory.

## Known limitations

- **Experimental:** a crash can end your desktop session. Save work before testing.
- **Panel matching uses position and screen**, not a permanent panel identity.
  Pick panels again after changing their layout or display scaling. Panels spanning
  screens and ambiguous mirrored-screen layouts cannot be picked reliably.
- **Two-finger follow-ups can intercept horizontal scrolling** briefly while an
  animation finishes. They can be disabled in [settings](CONFIGURATION.md).
- **Other gesture tools may conflict.** InputActions is optional and the activation
  helper can hand over control without editing its bindings. Other conflicting
  gesture tools must be disabled yourself.
- **Login startup requires systemd user services.** Session-only activation also
  works without systemd.
- **GPU, HDR, rotated displays and non-Apple touchpads need broader testing.**
  Passing a software-rendered or simulated test does not validate physical hardware.

This native plugin cannot be installed through “Get New Effects” or inside a
Flatpak. Immutable distributions need their supported system-package mechanism.
See [maintainer notes](MAINTAINERS.md#distribution-constraints) for the technical reasons.
