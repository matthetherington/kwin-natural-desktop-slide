# KWin Natural Desktop Slide

Switch between KDE Plasma desktops with a three-finger touchpad swipe. The
animation follows your fingers, and you choose which panels slide with your
desktop and which stay in place.

It brings macOS (OS X)-like desktop switching to KDE Plasma, with touchpad
behaviour close to native Mac swipes: direct finger tracking, quick flicks and
spring-back overscroll at the desktop edges.

You can also customise the animation speed and the background shown when you
swipe past the last desktop. Panels keep the same contents on every desktop.

## Demos

### Desktop switching demo - fast and slow swipes (27 seconds, 4.5 MB)

https://github.com/user-attachments/assets/8ca0e43f-6843-4bbf-bb82-18d7ace9e889

### Edge overscroll demo - resistance and spring-back (7 seconds, 0.9 MB)

https://github.com/user-attachments/assets/56e87ffb-8fc6-4faf-9516-d3742101d9f3

**This is experimental software:** broad hardware and software compatibility has not been verified.

- Requires **KDE Plasma on Wayland**, a compatible KWin version and a touchpad
  with three-finger gesture support. KWin is the part of KDE that draws your desktop. Apple hardware is not required, but other systems still need testing.
- Only **Fedora Asahi with KWin 6.8.2** has been tested on real hardware so far, but other KDE-based systems should be compatible too.
- This is not a “Get New Effects” download. You'll need to download a package built specifically
  for your system, or build one yourself. See [compatibility](docs/COMPATIBILITY.md).

## Get started

1. Follow the [installation guide](docs/BINARY_PACKAGES.md) to find and install a
   matching package from [GitHub Releases](https://github.com/matthetherington/kwin-natural-desktop-slide/releases).
   Packages are only available after their release checks pass. If there is no
   match for your system, do not install a different one.
2. Open **Natural Desktop Slide Settings** from your application menu. On KWin 6.8,
   you can also open the same editor through **System Settings → Animations →
   virtual-desktop switching → Natural Desktop Slide (Experimental) → configuration**.
   Choose which panels should slide, then save. See the [settings guide](docs/CONFIGURATION.md)
    for help with the controls and larger-text layouts. **Reload saved settings**
    discards unsaved gesture edits after confirmation.
   If the effect is already running, choose **Apply saved settings** after saving.
3. Save your work, open **Konsole** (KDE's terminal), and run:

   ```sh
   natural-desktop-slide-session start
   ```

Try a horizontal three-finger swipe. To turn it off and restore your previous setup:

```sh
natural-desktop-slide-session stop
```

Installing the package does not turn it on or restart your desktop. KDE's original
Slide effect is kept. For automatic startup, updates or removal, follow the
[installation guide](docs/BINARY_PACKAGES.md).
Source and package installs use the same file permissions: plugins and menu
launchers are non-executable; the settings application remains executable.

## Help and more information

- [Changelog and release notes](CHANGELOG.md)
- [Settings and panel selection](docs/CONFIGURATION.md)
- [How swipes work](docs/GESTURES.md)
- [Compatibility and known limitations](docs/COMPATIBILITY.md)
- [Feature scope and validation coverage](docs/PLAN.md)
- [Startup and recovery](docs/STARTUP.md)
- Source-installed login services can resume pinned plugins without loading stock
  Slide first when a valid saved recovery baseline is available.
- [All documentation](docs/README.md), including source builds and contributor guides
- [Report a problem](https://github.com/matthetherington/kwin-natural-desktop-slide/issues)

Copyright © 2026 Matt Hetherington. Licensed under [GPL-2.0-or-later](LICENSE),
with code adapted from KDE's Slide effect. See [credits and provenance](docs/UPSTREAM.md).
