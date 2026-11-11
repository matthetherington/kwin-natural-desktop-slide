# Upstream provenance and compatibility

<!-- Intended audience: contributors and licence reviewers tracing adapted code and private KWin dependencies. -->

For user-facing requirements, see [compatibility](COMPATIBILITY.md).

The effect is derived from **KWin v6.8.2**, GPL-2.0-or-later:

- `src/plugins/slide/slide.cpp` → `effect/natural_desktop_slide.cpp`
- `src/plugins/slide/slide.h` → `effect/natural_desktop_slide.h`
- `src/plugins/slide/main.cpp` → `effect/main.cpp`
- `src/effect/springmotion.{h,cpp}` → `src/spring_motion.h`

Source: https://invent.kde.org/plasma/kwin/-/tree/v6.8.2
(retrieved via the KDE GitHub mirror). Adaptations were made on **2026-11-11**.
Upstream copyright and license notices are retained in the derived files.
Plugin metadata and build/configuration integration are original project code.

## Changes from Slide

- Unique classes/plugin ID (`kwin_natural_desktop_slide`) and disabled-by-default metadata.
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
- Unified overscroll renderer: bounded edge previews,
  spring return without selecting a nonexistent desktop, and a temporary idle-only
  D-Bus choice of black gap versus a translated wallpaper-only adjacent tile,
   painted only in the exposed strip. Disabling overscroll restores edge clamping. The renderer preserves
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

## Dependency reduction and private ABI warning

The spring solver is now project-owned in `NaturalSwipe`, not a mirrored private
class declaration or a call to exported `KWin::SpringMotion` symbols. It preserves
upstream RK4 integration, 10 ms steps and interpolated position/velocity, with
explicit nonpositive-duration handling and an interpolation reset for immediate
settling when animations are disabled. Tests compare twelve frames against the
installed KWin 6.8.2 solver and cover settling and velocity-free gesture handover.

Painting is scoped to the current output; shared previews fan out to all outputs,
independent returns are scoped, and topology changes cancel gesture context.

`effect/compat/kwin-versions.json` centralizes reviewed source versions
**6.7.0–6.7.6 and 6.8.0–6.8.2**. Upstream 6.8 effect, output, render, input and desktop headers
used here were compared: only unused Workspace scripting additions differ.
Native builds/tests now pass against Debian sid 6.7.4, Ubuntu 26.10 6.7.5 and
Fedora 45 6.8.2 SDKs on ARM64. Only 6.8.2 is live-validated. The installer and plugin support
checks require the exact runtime version used to build each binary.

The 6.7 port uses the installed callback return type: legacy `void` paints are
treated as successful; 6.8 `bool` failures still propagate. Feature detection
handles the missing 6.7 autoscroll field and enum-based output-configuration
result. All three adapter paths have standalone tests. Frameworks/ECM minimums
are 6.26, matching 6.7's baseline; KWin's SDK supplies its own Qt/compiler needs.
Reviewed 6.7 headers include effect/handler/window/timeline, output/input device,
region/render target/viewport, input/events, workspace/window/desktops and GL
shader/vertex buffer. Patch-release changes in that set are unused workspace
shortcuts/scripting, filter-iteration fixes and a window virtual method; exact
per-release rebuilds remain essential. Core output mode internals also changed
between 6.7 and 6.8, but are not used by production code here.
KWin 6.6 and earlier remain rejected: their desktop signals and geometry APIs
need a larger separate port. Header review and synthetic tests do not establish
that an untested patch release builds or that a compiled 6.7 plugin works in a live desktop.

Private dependencies remain: input filters/events, workspace and virtual desktop
control, rendering internals, and `window.h` resource-class access. The public
`EffectWindow::windowClass()` concatenates resource name and class with a space;
it is not an unambiguous replacement for arbitrary resource names. Preserve panel
matching rather than silently changing its semantics. No generic Plasma 6 ABI is
claimed. Downstream patches still need review, even at the same version number.
