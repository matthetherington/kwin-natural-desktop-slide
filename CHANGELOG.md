# Changelog

## Unreleased

- Allow source builds on KWin 6.7.x and 6.8.x patch releases, warning when a patch
  has not been individually reviewed. Keep exact build/runtime binary matching.
- Test the newest distribution SDKs weekly and publish a new patch release only
  after portable CI, the entire native package matrix and signing checks pass.
- Prepare one version-bumped source snapshot for every release test. Push its
  commit/tag only after validation, without overwriting a newer default branch.

## 0.1.2

- Support source builds and native package rebuilds against reviewed KWin 6.8.3,
  fixing the Fedora/Arch SDK update that blocked CI.
- Retain exact build/runtime matching and reject unreviewed versions, with
  regression tests preventing interchangeable use of 6.8.2/6.8.3 binaries.
- Validate full-effect builds, tests and RPM generation on Fedora 45 ARM64 with
  KWin 6.8.3. Physical desktop validation remains limited to KWin 6.8.2.

## 0.1.1

- Fix source-installed login startup when stock Slide is disabled and a valid
  saved recovery baseline is available.
- Keep the gesture editor's reload control compact at narrow widths and larger
  font sizes, with explicit font dependencies in CI.
- Make source and CMake installs use consistent non-executable plugin permissions,
  with CI checking both shared-library permission defaults.
- Refresh user and contributor documentation and add desktop-switching and
  overscroll demo videos.

## 0.1.0

- Initial experimental release: continuous desktop swipes, selective panel
  movement, reversible gestures, flicks, follow-ups and configurable overscroll.
- Shared/independent multi-output policies and exact KWin build/runtime guards.
- Native distribution packages with explicit user activation and stock Slide recovery.
- Consistent application-menu and System Settings access for source and package
  settings installs.
- Beginner-friendly settings feedback, optional technical details, clearer Save/Apply
  controls and inline help, with layout fixes for narrow windows and larger fonts.
- Only Fedora Asahi/aarch64/KWin 6.8.2 has physical desktop validation so far.
  Package build/test coverage is listed in the per-asset JSON manifests.
