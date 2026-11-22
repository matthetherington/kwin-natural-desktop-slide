# KWin Natural Desktop Slide

Bring the feel of macOS desktop switching to KDE Plasma. Swipe with three fingers on your touchpad and your desktops move according to the speed of your hand. Swipe past the first or last desktop, and they spring back into place.

You can choose which panels slide with your desktops and which stay put, adjust the spring-back speed, and customise the background revealed when you swipe past the last desktop.

## Video Demos

Credit for the touch overlay visualisation goes to https://github.com/malbiruk/touchview - this was slightly modified to add the trail effect but is otherwise original.

Placing KDE Plasma panels in the background behind windows was done using [plasma-panel-in-background](https://github.com/matthetherington/plasma-panel-in-background).

### Desktop switching demo - fast and slow swipes, and two-finger slide control after three-finger swipe (30 seconds, 7.9 MB)

https://github.com/user-attachments/assets/844b8188-d5a2-4c57-b638-229baa09d891

### Edge overscroll demo with solid black default background - resistance and spring-back (9 seconds, 2.1 MB)

https://github.com/user-attachments/assets/869a9d0c-5855-4cd0-b5fd-7f677eb1785a

### Edge overscroll demo with desktop wallpaper option (17 seconds, 3.1 MB)

https://github.com/user-attachments/assets/d03c1b94-9a10-4f06-80bf-0e6660e9ea62

## Requirements

- **KDE Plasma on Wayland** with OpenGL desktop rendering.
- A touchpad that supports three-finger swipes. Apple hardware is not required.
- A package that matches your distribution release, CPU architecture and exact
  KWin package version. This effect isn't available through “Get New Effects.”

**Experimental:** Only Fedora Asahi with KWin 6.8.2 has been tested on real
hardware. Source builds target KWin 6.7.x and 6.8.x. See [compatibility and known
limitations](docs/COMPATIBILITY.md).

## Get started

1. Follow the [installation guide](docs/BINARY_PACKAGES.md) to install a matching
   package from [GitHub Releases](https://github.com/matthetherington/kwin-natural-desktop-slide/releases).
   If there's no match, wait for one or follow the [source build guide](docs/DEVELOPMENT.md).
   Installing the package doesn't activate the effect.
2. To choose which panels slide, open **Natural Desktop Slide Settings** from
   your application menu and save your choices. Panels stay fixed by default.
   See the [settings guide](docs/CONFIGURATION.md) for more options.
3. Save your work first: a crash can end your desktop session. Run the commands
   below in **Konsole** (KDE's terminal) as your desktop user, without `sudo`.
   Use the helper to switch effects; don't enable both effects in System Settings.

### Try it for this session only

```sh
natural-desktop-slide-session start
```

Try a horizontal three-finger swipe. Run `natural-desktop-slide-session stop`
to end the trial and restore your previous setup.

### Enable or disable login startup

Login startup requires a systemd user session. Stop any session-only trial first,
then run this to enable the effect now and at future Plasma Wayland logins:

```sh
natural-desktop-slide-session enable
```

Run `natural-desktop-slide-session disable` to turn it off, restore your previous
setup and disable login startup.

## Help and more information

- [Startup and recovery](docs/STARTUP.md)
- [Updating or removing the package](docs/BINARY_PACKAGES.md#upgrade-or-remove)
- [How swipes work](docs/GESTURES.md)
- [Changelog and release notes](CHANGELOG.md)
- [All documentation](docs/README.md)
- [Report a problem](https://github.com/matthetherington/kwin-natural-desktop-slide/issues)

Copyright © 2026 Matt Hetherington. Licensed under [GPL-2.0-or-later](LICENSE),
with code adapted from KDE's Slide effect. See [credits and provenance](docs/UPSTREAM.md).
