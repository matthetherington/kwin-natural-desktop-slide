# Project instructions

## Goal and scope

Build a separately named KWin desktop-switching effect with selectively sliding
Plasma panels. See `docs/PLAN.md` for status and the continuous-gesture milestone.
Panel contents are shared across desktops; only their rendering changes.

## Safety

- Never replace Fedora's stock KWin libraries or Slide plugin.
- Do not install, enable effects, restart KWin/Plasma, or change desktop/panel
  settings without explicit user approval. A Wayland compositor crash can end
  the session and lose unsaved work.
- Keep the original Slide effect available as the rollback path; never enable
  both desktop-switching effects at once.
- Do not change InputActions configuration as part of the initial effect work.
- Keep private window dumps and diagnostics under ignored `local/`.

## Implementation

- Use C++20, Qt 6, CMake, and four-space indentation. The KWin plugin target
  inherits C++23 from the installed KWin 6.8 development target.
- Keep panel policy independent of KWin so it can be tested without a compositor.
- Match geometry in logical output-local coordinates after conversion by the
  KWin adapter. Never store transient window IDs as persistent panel identity.
- Default to fixed panels. Rules are first-match-wins; only Plasma dock windows
  are eligible. Other all-desktop windows must remain unaffected.
- Preserve Slide's interactive progress/cancellation paths for milestone 2.
- Build against the installed KWin development package; do not assume plugin ABI
  compatibility across updates.

## Licensing

Use GPL-2.0-or-later throughout the project, matching KWin's Slide effect.
Include SPDX license and copyright headers in source/build files. When adapting
upstream code, retain its copyright and license notices and document its origin
and modifications. The full GPL version 2 text is in `LICENSE`.

## Validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Add regression tests for policy changes. Report what was actually tested and
distinguish standalone tests from live compositor testing. Work in small,
reviewable chunks; update the plan and README as each chunk lands.

For effect/KCM changes, also configure `build-effect` with
`-DBUILD_KWIN_EFFECT=ON`, build it and run its tests. The default build does not
compile compositor code. See `docs/DEVELOPMENT.md` for options and recovery.
