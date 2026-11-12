# Changelog

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
