# Status and remaining validation

## Portability review

Replaced Fedora/RPM and hard-coded lib64 installer assumptions with generated
CMake/KDE install layouts, custom-build/config selection, read-only previews and
package staging. Full installation now works from one build. Session/startup
helpers support native gesture ownership when InputActions is absent and preserve
that baseline on recovery; legacy InputActions handoffs remain covered. Added
qdbus naming/Qt-host-path discovery, preference for an installed SpringMotion
header, an OpenGL support guard and standalone Ubuntu GCC/Clang CI. Hosted CI and
cross-distribution/native-gesture live validation remain outstanding.

The exact KWin 6.8.2 and one-display guards remain. Native C++ plugins are not
one-click KPackage effects; see [compatibility and KDE Store roadmap](COMPATIBILITY.md).
No installed plugin, helper snapshot or service was updated during this review.
Validation passed: 13 headless-core, 16 standalone, 18 standard-effect and 20
full-preview suites, plus installer dry-run, local documentation links and
`git diff --check`. No cross-distribution or new live-session test was performed.

## Implemented

- Separate Slide-derived renderer, selective panel policy, multi-panel picker,
  shared standalone/KDE settings and atomic configuration saves.
- Separate three-finger controller, reversible continuous tracking, 30% slow
  completion and independent same-direction flick completion at 1 desktop/second.
- Repeatable continuous horizontal finger-scroll follow-ups during settling,
  including partial returns and the 400 ms blocked-edge entry window.
- Resisted 20% edge overscroll with spring return and moving adjacent wallpaper.
- Gesture, overscroll and independent release-animation settings. Solid background
  colour defaults to black; RGB/hex picker with explicit HSL controls.
- Guarded manual handoff/rollback, private pinned login-service helpers, atomic
  installation and KDE plugin/package metadata with repository and issue URLs.

## Validation

User-verified on Fedora Asahi, KWin 6.8.2 Wayland: selective panel sliding, native
panel picking, settings pane/reload, tracking/reversal/aborts, speed-based flicks,
30% completion, repeated two-finger follow-ups, blocked-edge grace, black overscroll
and moving adjacent-wallpaper mode.

The existing login service was enabled and adopted the active preview pair without
reloading effects; `active (exited)` and successful completion were verified.
Its private helper snapshots and pinned identities are independent of source edits.
Obsolete unloaded development plugins were moved into verified private backups;
those backups and the startup-pinned plugins must be preserved.

The new configurable solid-colour rendering is compiled but **not live-tested or
installed**. Automated tests cover colour validation/backward-compatible defaults,
RGB/HSL synchronisation, cancellation, staging/save/defaults and wallpaper retention,
alongside policy, gesture, UI layout, metadata, ABI and recovery regressions.
Repository cleanup replaces obsolete prototype diaries with current documentation;
all four tools remain because installation, diagnostics or recovery still use them.
Final validation: all 16 standalone, 18 standard-effect and 20 full-preview test
suites pass. Local documentation links resolve and `git diff --check` passes.

## Outstanding

- Approved live test of coloured gaps, especially fractional scaling, output
  transforms, fixed/selected panels, HDR colour management and return interruption.
- Full reboot/login cycle; adoption alone does not prove login ordering.
- Broader animation-speed tuning, wrapping, fullscreen/auto-hide, blur, shadows,
  popup stacking and external-effect interruption.
- Multiple displays (controller currently refuses these), display-layout changes,
  moved/new panels, embedded picker cancellation and individual-panel overrides.
- Actual compositor-crash recovery and uninstallation.

Takeover starts from the committed desktop, not the precise in-flight spring
position, so rapid re-entry can jump. High-level finger-scroll events do not
provide reliable contact count or palm rejection. Matching is geometry-based;
panels need re-picking after layout changes. KWin's private ABI is pinned to 6.8.2.

No automatic commits, installation, effect activation or startup retargeting are
part of the colour/cleanup work. See [startup recovery](STARTUP.md),
[gesture behaviour](GESTURES.md) and [provenance](UPSTREAM.md).
