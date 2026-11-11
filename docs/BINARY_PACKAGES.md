# Install, update or remove a release package

<!-- Intended audience: users installing a prebuilt package. -->

You do not need a compiler or development tools. For building from source, see [development](DEVELOPMENT.md);
for publishing packages, see [maintaining and releasing](MAINTAINERS.md).

## Install without a compiler

Download the package for **your distribution release, CPU architecture and exact
KWin package build** from [GitHub Releases](https://github.com/matthetherington/kwin-natural-desktop-slide/releases).
Downloads appear only after release checks pass. The table below lists intended
package targets, not a promise that every package is already available or tested
on a real desktop.

Only Fedora Asahi has been tested on a real desktop. Other package checks are
build and container tests, not proof that the effect works on your desktop.
See [compatibility](COMPATIBILITY.md) for the testing boundary.

Package build targets (not a list of available downloads):

| Distribution | Architectures | Format |
| --- | --- | --- |
| Fedora 45 / compatible Fedora Asahi package set | x86_64, aarch64 | `.rpm` |
| Debian unstable (sid), not Debian 13 stable | amd64, arm64 | `.deb` |
| Ubuntu/Kubuntu 26.10 | amd64, arm64 | `.deb` |
| Arch Linux | x86_64 | `.pkg.tar.zst` |

Other derivatives are not automatically equivalent: they must have the same
runtime package name/version and matching libraries. In particular, Ubuntu users
must not install the Debian sid asset. A package cannot make an unsupported KWin
version compatible.

Each asset has a `.sha256` file and JSON manifest containing distribution target,
architecture, upstream KWin version and exact distribution package version.
The release must match all of these, not just your Plasma version. If you are
unsure, ask in the project's [issue tracker](https://github.com/matthetherington/kwin-natural-desktop-slide/issues)
before installing.

Open Konsole in your download folder. Check the download for corruption using its
checksum file (replace the example filename with the downloaded one):

```sh
sha256sum --check ./downloaded-package.rpm.sha256
```

Run **only the commands for your distribution**, replacing the example filename.
`sudo` asks for your password to install software; it may show no characters as
you type. Fedora users should read the signing-key note below before importing.

```sh
# Fedora:
sudo rpm --import ./RPM-GPG-KEY-natural-desktop-slide.asc
sudo dnf install ./downloaded-package.rpm
# Debian / Ubuntu:
sudo apt install ./downloaded-package.deb
# Arch (update the system first, then install the matching release asset):
sudo pacman -Syu
sudo pacman -U ./downloaded-package.pkg.tar.zst
```

The package manager rejects a mismatched KWin package. **Do not force dependencies**.
Fedora releases include signed RPMs and the public signing key alongside them;
verify the key fingerprint against the maintainer's published fingerprint before
importing it. If no fingerprint is published, do not bypass signature checks.
Use the matching asset, wait for a rebuild, or remove this effect package before
upgrading KWin. These strict version requirements help protect your desktop from
incompatible plugins. Frequently updated distributions may need frequent rebuilds.

## Configure and opt in

Installation does **not** enable effects, change panel/gesture settings, enable
services, or restart Plasma/KWin. No package script runs inside your desktop.

1. Launch **Natural Desktop Slide Settings** from the application menu (or run
   `natural-desktop-slide-config`), select panels and save settings.
2. Save your work. A native compositor plugin crash can end the Wayland session.
3. In a Plasma Wayland terminal, run as your normal desktop user:

```sh
# Try for this session only:
natural-desktop-slide-session start
# Restore stock Slide / recorded gesture owner:
natural-desktop-slide-session stop

# Alternatively enable the user service for login and now:
natural-desktop-slide-session enable
# Restore the baseline and disable login activation:
natural-desktop-slide-session disable
```

Do not use System Settings to enable this alongside stock Slide. The helper does
a guarded handoff, preserves settings and records recovery under
`$XDG_STATE_HOME/kwin-natural-desktop-slide` (normally `~/.local/state/...`).
InputActions is optional; leave it loaded before handoff if installed. Its bindings
are not edited. Other competing gesture owners must be disabled by the user.

`start`/`stop` also work without systemd; `enable`/`disable` require a systemd user
session. Do not mix manual and managed activation: stop the manual session before
enabling the service, or stop the service before using manual commands.

## Upgrade or remove

Stop/disable the effect before updating or removing its package. When upgrading loaded native
plugins, log out/in before using the replacement: KWin can cache the old libraries.
Installation never restarts the compositor automatically. User settings are not
owned by the package and are retained on removal.

If you enabled login startup, run `natural-desktop-slide-session disable` first.
For a session-only trial, run `natural-desktop-slide-session stop` instead. Save
your work before logging out.

Remove the package using your distribution's software manager, or use the
appropriate command:

```sh
# Fedora:
sudo dnf remove kwin-natural-desktop-slide
# Debian / Ubuntu:
sudo apt remove kwin-natural-desktop-slide
# Arch:
sudo pacman -R kwin-natural-desktop-slide
```

### Migrating from a source-installed login setup

If you previously installed a login service from source, do not activate the
package over it. Follow the [source-service migration steps](STARTUP.md#migrate-a-source-service-to-a-release-package)
first. A session-only source test should be stopped with its original helper
before switching to a package.
