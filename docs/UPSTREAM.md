# Upstream provenance and compatibility

The effect is derived from **KWin v6.8.2**, GPL-2.0-or-later:

- `src/plugins/slide/slide.cpp` → `effect/natural_slide.cpp`
- `src/plugins/slide/slide.h` → `effect/natural_slide.h`
- `src/plugins/slide/main.cpp` → `effect/main.cpp`
- `src/effect/springmotion.h` → `effect/compat/springmotion.h`

Source: https://invent.kde.org/plasma/kwin/-/tree/v6.8.2
(retrieved via the KDE GitHub mirror). Adaptations were made on **2026-11-11**.
Upstream copyright and license notices are retained in the derived files.
Plugin metadata and build/configuration integration are original project code.

## Changes from Slide

- Unique classes/plugin ID (`kwin_natural_slide`) and disabled-by-default metadata.
- Independent JSON configuration, not the stock Slide KConfig group.
- Wallpaper sliding enabled and desktop gaps fixed at zero for this prototype.
- Only selected, all-desktop Plasma dock windows slide; other sticky windows
  stay fixed. Selected panels are rendered once per visible desktop, clipped to
  their own output, without creating actual Plasma panel instances.
- Fixed docks retain upstream elevation behaviour; selected docks do not get
  artificially raised above fullscreen windows.
- Panel decisions freeze for each transition so config/geometry changes do not
  change translation halfway through. New windows are classified when added.
- Screen state uses unique ownership rather than implicitly shared QHash values.
- Visibility tests reset per desktop instead of carrying booleans across the loop.
- Paint data is copied for each draw, avoiding a leaked transform on early return.
- Read-only D-Bus diagnostics; no titles or persistent matching by transient UUID.
- Spring settling, wrap-path optimisation, interactive desktop progress and
  cancellation pathways are retained. A separate GPL input controller now drives
  those notifications; no InputActions implementation code was copied.
- Optional separately identified overscroll renderer: bounded edge previews,
  spring return without selecting a nonexistent desktop, and a temporary idle-only
  D-Bus choice of black gap versus a translated wallpaper-only adjacent tile,
  painted only in the exposed strip. Default renderer keeps its original clamping. The preview preserves
  panel policy, disables non-wrapping translation wraparound at edges, and caps
   rendered spring positions at the overscroll limit. Solid gaps now use an
   opaque configurable colour, painted with colour-managed OpenGL shaders at
   wallpaper depth. The overscroll renderer refuses non-OpenGL compositors.
- Development preview libraries use hidden internal symbols and fresh plugin
  identities to avoid reusing cached renderer/controller code in a running KWin.

## Fedora packaging check

Initial build environment: `kwin` and `kwin-devel` **6.8.2-3.fc45.aarch64**.
The corresponding `kwin-6.8.2-3.fc45.src.rpm` was retrieved from:

https://kojipkgs.fedoraproject.org/packages/kwin/6.8.2/3.fc45/src/

Its sole patch, `c9d720793bbcef9483ab290673032d39441c0e6f.patch`, changes
`src/compositor.cpp` to handle a missing scanout render device; it does not alter
Slide or SpringMotion. Source archives are kept outside the repository, not
vendored wholesale. Installed headers and exported symbols were checked while
building/linking this prototype.

## Private ABI warning

Fedora's development package does not install `effect/springmotion.h` even though
the class is exported by `libkwin.so.6`. A matching installed header is preferred
on distributions that provide it. Otherwise the compatibility header preserves the
v6.8.2 class layout and declarations and links to the system implementation; it
does **not** define replacement KWin symbols. A standalone spring smoke test
checks basic construction, advancement, and settling without starting an effect.

The build requires **exactly KWin 6.8.2**. Do not relax that check after an update
without reviewing upstream headers/layout and rebuilding. Even an unchanged
upstream version with new downstream patches may need review. Compiling and
linking are not proof that an in-process compositor plugin is safe to activate.
