# Maintaining and releasing

<!-- Intended audience: project maintainers and distribution packagers. -->

For installation instructions, see [the package guide](BINARY_PACKAGES.md).

## Release workflow

Release automation checks tags, versions, changelog entries and checksums, and
builds native packages separately for every SDK.
See [release.yml](../.github/workflows/release.yml) and
[targets.json](../packaging/targets.json).

PR/main/manual/weekly runs build packages as Actions artifacts, without publishing.
Tag pushes publish only after **every declared target** passes a native full-effect
build, CTest, package generation, disposable-container installation/CLI smoke tests
and asset verification. A failed target blocks the release; it is never silently
omitted or relabelled as compatible.

1. Update CMake and all four plugin metadata versions together.
2. Add a nonempty `## <version>` section to `CHANGELOG.md`.
3. Run tests and inspect native CI artifacts/dependencies.
4. Run the tag-only metadata check locally before committing and tagging (replace
   `v0.1.1` with the intended release tag):

   ```sh
   python3 tools/prepare-release.py --tag v0.1.1 --output build/release-notes.md
   ```

5. Review the generated notes, commit changes, then push the matching tag.

For a rebuild after a distribution SDK update, bump the project/plugin patch
version and changelog before publishing a new tag. Reusing an existing tag or
native package version would make package-manager upgrades ambiguous.

The release job verifies the tag and publishes the **same tested artifacts**, their
checksums and manifests, with write permission limited to the publishing job.
RPM signing changes the package header, not the tested binary payload; final
checksums/manifests are refreshed after signing and checked again before publishing.
There are no release credentials in build jobs and no automatic commits/tags.

## Signing and publication

Before the first tag, configure a dedicated RPM CI signing key (for example an
RSA 4096-bit signing key/subkey) as the `RPM_SIGNING_KEY` GitHub Actions secret,
and its full public fingerprint as the `RPM_SIGNING_FINGERPRINT` repository variable.
The CI key must support unattended signing without a passphrase prompt. Keep its
private bytes out of the checkout; publish the public fingerprint in release/setup
documentation. Publishing fails closed if the secret/variable is missing or signing
fails. The public key is exported into the release; private material is imported
only into a temporary keyring in the tag-only publishing job.

Checksums alone detect corruption, not publisher identity. A signed COPR/OBS/apt
repository remains a future improvement for easier, authenticated automatic updates.
Unsigned-package installation overrides belong only in disposable CI containers,
never in user instructions.

Before publishing, confirm that the public documentation distinguishes package
build targets, available assets and live-tested systems. See [validation coverage](PLAN.md).

## Build a native package

Build **inside the matching disposable distro/architecture environment**:

```sh
cmake -S . -B build-package -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_KWIN_EFFECT=ON -DBUILD_DISTRIBUTION_PACKAGE=ON
cmake --build build-package --parallel 2
python3 tools/build-package.py --build-dir build-package --target fedora-45-aarch64
```

Use native SDK/runtime packages; never substitute headers from another release.
The builder rejects integration hooks/development IDs and pins the exact package
owning `kwin_wayland`. CPack generates RPM/DEB dependencies from ELF libraries as
well; Arch includes an exact KWin dependency and its runtime tool requirements.
No host installation or activation is part of package building.

CMake installation supports `DESTDIR` and components `effect`, `settings`,
`controller` and optionally `probe`. Do not bundle system Qt/KWin libraries.
Declare exact KWin/private-ABI and architecture dependencies and rebuild after
relevant KWin updates. Downstream patches can change private ABI even without
changing the upstream version. See [upstream API notes](UPSTREAM.md).

## Distribution constraints

On immutable distributions, use their supported package/layering mechanism rather
than writing a read-only `/usr`. A container/Flatpak's plugin directory is not the
host KWin's plugin directory. A custom prefix alone does not make per-user compiled
plugins discoverable; an explicitly supported Qt plugin search path is required.

This is **not a one-click KDE Store effect**. KDE's documented `KWin/Effect`
packages contain JavaScript/QML loaded through KPackage. Zipping native libraries
or adding KPackage metadata does not make this private input filter installable
through “Get New Effects”. A Store listing may link to the package guide, but
must not promise one-click installation.

A JS/QML redesign would first need to prove selective panel painting and continuous
gesture/finger-scroll ownership are possible through supported APIs. A native
companion still needs distro packaging. Upstreaming the required hooks or feature
is another route to reducing private-API dependence.

References:
- [KDE effect distribution guide](https://develop.kde.org/docs/plasma/kwineffect/)
- [Native-effect distribution constraints](https://blog.vladzahorodnii.com/2024/03/)

## Before claiming wider support

- Test x86_64 and aarch64 on at least two distributions with exact matching KWin.
- Exercise native gestures without InputActions and handoff/recovery with it.
- Check non-Apple touchpads, natural-scroll inversion, contact loss, short flicks,
  cancellation, lock/unlock, device removal and suspend/resume.
- Verify panel picking, fixed/selected panels, edge colours/wallpaper, fractional
  scaling, rotated outputs, HDR/colour management and interrupted returns.
- Reboot/login-test startup and rollback on each target distribution. The source
  service assumes systemd user sessions and `plasma-kwin_wayland.service`; other
  session managers need reviewed integration, not unconditional autostart.
- Test package installation/removal and KWin update behaviour without replacing
  stock Slide or leaving competing gesture owners active.
- Validate physical GPU/connector, colour and hotplug behaviour. Virtual-output
  tests check rendering paths, not pixel correctness or hardware behaviour.

See [development](DEVELOPMENT.md) for isolated test commands and
[portability](PORTABILITY.md) for hardware-test reporting.
