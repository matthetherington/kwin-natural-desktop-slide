# KWin Natural Desktop Slide

Switch between KDE Plasma desktops with a three-finger touchpad swipe. The
animation follows your fingers, and you choose which panels slide with your
desktop and which stay in place.

You can also customise the animation speed and the background shown when you
swipe past the last desktop. Panels keep the same contents on every desktop.

## Before you try it

**This is experimental software. Save your work before turning it on:** a crash
could close your desktop session and unsaved applications.

- Requires **KDE Plasma on Wayland**, a compatible KWin version and a touchpad
  with three-finger gesture support. KWin is the part of KDE that draws your desktop.
- Only **Fedora Asahi with KWin 6.8.2** has been tested on a real desktop so far.
  Apple hardware is not required, but other systems still need testing.
- This is not a “Get New Effects” download. It needs a package built specifically
  for your system. See [compatibility](docs/COMPATIBILITY.md) if you are unsure.

## Get started

1. Follow the [installation guide](docs/BINARY_PACKAGES.md) to find and install a
   matching package from [GitHub Releases](https://github.com/matthetherington/kwin-natural-desktop-slide/releases).
   Packages are only available after their release checks pass. If there is no
   match for your system, do not install a different one.
2. Open **Natural Desktop Slide Settings** from your application menu. On KWin 6.8,
   you can also open the same editor through **System Settings → Animations →
   virtual-desktop switching → Natural Desktop Slide (Experimental) → configuration**.
   Choose which panels should slide, then save. See the [settings guide](docs/CONFIGURATION.md)
   for help with the controls and larger-text layouts.
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

## Help and more information

- [Settings and panel selection](docs/CONFIGURATION.md)
- [How swipes work](docs/GESTURES.md)
- [Compatibility and known limitations](docs/COMPATIBILITY.md)
- [Startup and recovery](docs/STARTUP.md)
- [All documentation](docs/README.md), including source builds and contributor guides
- [Report a problem](https://github.com/matthetherington/kwin-natural-desktop-slide/issues)

Copyright © 2026 Matt Hetherington. Licensed under [GPL-2.0-or-later](LICENSE),
with code adapted from KDE's Slide effect. See [credits and provenance](docs/UPSTREAM.md).
