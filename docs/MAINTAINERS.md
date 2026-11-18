# Maintaining and releasing

<!-- Intended audience: project maintainers and distribution packagers. -->

For installation instructions, see [the package guide](BINARY_PACKAGES.md).

## Release workflow

Release automation checks tags, versions, changelog entries and checksums, and
builds native packages separately for every SDK.
See [release.yml](../.github/workflows/release.yml) and
[targets.json](../packaging/targets.json).

PR/main/manual runs build packages as Actions artifacts, without publishing.
Tag pushes and weekly scheduled runs publish only after **every declared target**
passes a native full-effect build, CTest, package generation, disposable-container
installation/CLI smoke tests and asset verification, plus the reusable portable
CI workflow. A failed target blocks the release; it is never silently omitted or
relabelled as compatible.

### Weekly SDK rebuilds

Every Monday at 08:17 UTC, the default-branch schedule installs the newest SDK
available in each configured distribution. Source builds accept stable patches
on KWin 6.7.x/6.8.x; new minor branches and prereleases fail closed. A distribution
that still ships 6.7.x is not forcibly upgraded to upstream's newest branch.

The read-only metadata job prepares the next project patch version, greater than
the current version and any existing tag on that release line. It updates CMake,
all four plugin versions and the changelog, incorporating `Unreleased` notes.
Every portable/native job applies this same source patch before building/testing.
No version commit or tag is pushed on test failure.

After all CI gates, signing and final asset verification succeed, the only
write-enabled job atomically pushes the prepared commit to the default branch and
creates its annotated tag, then publishes the tested assets. It refuses to
overwrite a changed default branch or an existing tag with different source.
If GitHub publication fails after that push, rerunning the same workflow can reuse
the identical tag/snapshot; it never moves a tag. Each successful weekly run
creates a new project patch release, even if the SDK versions are unchanged.

For a hosted dry run, manually dispatch **Native packages and releases** with
`rebuild-snapshot` enabled. It prepares the same version patch and runs portable
CI and the native matrix, but cannot push a commit/tag or publish a release.

Repository rules must allow the release job's `GITHUB_TOKEN` to push this
commit/tag. If branch protection forbids it, the workflow fails rather than
bypassing the rules. `contents: write` and signing secrets are confined to the
publishing job; ordinary PR and manual runs cannot publish. Pushes made with
`GITHUB_TOKEN` do not trigger another release workflow: the same source snapshot
has already passed the reusable CI and native matrix in this run.

### Manual source releases

1. Update CMake and all four plugin metadata versions together.
2. Add a nonempty `## <version>` section to `CHANGELOG.md`.
3. Run tests and inspect native CI artifacts/dependencies.
4. Run the tag-only metadata check locally before committing and tagging (replace
   `v0.1.2` with the intended release tag):

   ```sh
   python3 tools/prepare-release.py --tag v0.1.2 --output build/release-notes.md
   ```

5. Review the generated notes, commit changes, then push the matching tag.

For a manual rebuild after a distribution SDK update, bump the project/plugin
patch version and changelog before publishing a new tag. Scheduled builds do this
automatically; fetch their version commits/tags before selecting a manual version.
Reusing an existing tag or native package version would make upgrades ambiguous.

The release job verifies the tag and publishes the **same tested artifacts**, their
checksums and manifests, with write permission limited to the publishing job.
RPM signing changes the package header, not the tested binary payload; final
checksums/manifests are refreshed after signing and checked again before publishing.
There are no release credentials in build jobs; scheduled commits/tags are created
only after the release gates pass.

## Signing and publication

Before the first tag, configure a dedicated RPM CI signing key (for example an
RSA 4096-bit signing key/subkey) as the `RPM_SIGNING_KEY` GitHub Actions secret,
and its full public fingerprint as the `RPM_SIGNING_FINGERPRINT` repository variable.
The CI key must support unattended signing without a passphrase prompt. Keep its
private bytes out of the checkout; publish the public fingerprint in release/setup
documentation. Publishing fails closed if the secret/variable is missing or signing
fails. The public key is exported into the release; private material is imported
only into a temporary keyring in the CI-gated publishing job.

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
