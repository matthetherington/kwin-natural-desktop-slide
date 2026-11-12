# Configuration reference and diagnostics

<!-- Intended audience: advanced users editing configuration files and developers diagnosing panel matching or settings reloads. -->

For everyday use, see the [settings guide](CONFIGURATION.md).

The effect reads `~/.config/kwin-natural-desktop-slide/panels.json`, respecting
`XDG_CONFIG_HOME`. Rules reload when the effect starts or receives a reload
request, not automatically on file changes. Invalid reloads preserve the last
valid rules; without a valid config all panels remain fixed.

## Panel picker

Open **Natural Desktop Slide Settings** from the application menu. On KWin 6.8,
you can also use **Animations → virtual-desktop switching** in System Settings, or launch
`build-effect/natural-desktop-slide-config`. The standalone build also provides
`build/natural-desktop-slide-config`. Both use the same editor and configuration file.

- **Add panel by clicking…** uses KWin's native window picker; Esc cancels. It
  works with stock Slide running. Only Plasma dock windows are accepted.
- Each pick adds a Slide checkbox row. Re-picking selects its existing row without
  resetting staged choices. Nothing changes until **Save panel choices**.
- Saving prepends explicit slide/fixed overrides and preserves other rules.
  Smaller selected bounds take priority over enclosing bounds.
- **Remove from this list** affects only the edit list, not previously saved rules.
  To keep a panel fixed, untick Slide and save it.
- **Discard edits and reload saved choices** discards staged choices after confirmation and re-reads
  external edits. **Apply saved settings** applies saved rules
  only if the custom effect is already loaded and reads the same file.
- Applying refuses unsaved choices. Feedback uses plain-language summaries;
  raw diagnostic replies and file/backup paths are available under **Technical details**.

Existing files get sibling `.backup-…` copies before atomic saves. Invalid JSON,
symlink files and externally modified files are not silently overwritten. Backups
are not automatically pruned. `--config /path/to/file.json` edits another file.

### Matching limitations

The picker saves an output name and containment rectangle, not a permanent panel
ID. Re-pick after panel/layout/scaling changes; obsolete overrides remain in JSON
until removed. Another panel entirely within the same rectangle can also match.
Ambiguous mirrored outputs and panels spanning outputs are rejected. Picker
rows describe this editing session; they are not a persistent inventory of panels.

## JSON format (version 1)

See [the example](../examples/panels.json). Rules are **first-match-wins**;
unmatched panels stay fixed. Only all-desktop Plasma docks slide in the effect.

| Root property | Meaning |
| --- | --- |
| `schemaVersion` | Required number `1`. |
| `defaultAction` | Optional; only `"fixed"` is supported. |
| `rules` | Required array; `[]` leaves every panel fixed. |

| Rule property | Meaning |
| --- | --- |
| `name` | Optional non-empty diagnostic label. |
| `output` | Optional exact output name; omitted means any output. |
| `edge` | Optional `left`, `right`, `top`, or `bottom`. |
| `maxInset` | Maximum distance from that edge, inclusive; default 32. Requires `edge` when explicitly supplied. |
| `geometryBounds` | Optional `[x,y,width,height]` containing the entire panel. |
| `action` | Required `slide` or `fixed`. |

All geometry uses **logical, output-local pixels**, excluding shadows. Rectangles
need positive sizes; values must be finite. Side-edge rules require panels taller
than wide; top/bottom rules require at least as much width as height. Panels
outside their output fail closed to fixed. Unknown keys/types are rejected. Avoid
duplicate JSON keys: Qt retains only their last occurrence.

## Gestures and animation settings

The shared standalone/KDE editor now has **Panels** and **Gestures and animation**
tabs. Gesture settings live separately in
`~/.config/kwin-natural-desktop-slide/gestures.json` (respecting `XDG_CONFIG_HOME`), so panel
rules and their schema are unchanged. With `--config`, this tab edits `gestures.json`
beside the chosen panel file; live reload refuses a different runtime path.

Do not look for the renderer in Desktop Effects on KWin 6.8: that page explicitly
excludes `desktop-animations`, which is handled by Animations. The exclusive group
must remain intact to prevent running stock Slide and this renderer together.
Development controller entries are input helpers, not the settings entry; do not
enable them individually through KDE while InputActions is in use. Historical
fresh plugin identities remain installed until explicitly retired; rollback unloads
effects but never uninstalls their files. Fully reopen System Settings after KCM
updates or plugin-file cleanup to refresh its metadata cache.

All tuned values remain defaults. A missing file means defaults and is not created
automatically. Saving is explicit and atomic, with backup/conflict/symlink checks.
Invalid reloads retain the entire previous valid configuration. **Restore
defaults** only stages values; it does not save or change active effects.

| Control / JSON key | Default | Range / meaning |
| --- | --- | --- |
| Slow-swipe completion / `slowSwipeThreshold` | 30% / `0.3` | 5–95%; slower partial swipes below this return. |
| Movement per desktop / `distancePerDesktop` | `300` | 50–1500 raw input units; lower is more sensitive. |
| Minimum flick speed / `flickVelocity` | `1` | 0.1–20 desktops/second; recognition only, not animation speed. |
| Overscroll / `overscrollEnabled` | `true` | Enable resisted edge previews in a supporting renderer. |
| Maximum overscroll / `overscrollLimit` | 10% / `0.1` | 1–50%; resistance approaches this cap. |
| Background / `overscrollBackground` | `"black"` | Solid colour (`"black"`) or moving adjacent `"wallpaper"`. |
| Solid colour / `overscrollColor` | `"#000000"` | Opaque `#RRGGBB`; RGB/hex and HSL picker. |
| Normal switch speed / `settleSpeed` | `1` | 0.1–4×; higher settles faster. |
| Fast-flick speed / `flickSettleSpeed` | `1` | Independent 0.1–4× for fast same-direction releases. |
| Abort/bounce speed / `returnSpeed` | `1` | Independent 0.1–4× for returning previews. |
| Two-finger follow-ups / `followups` | `true` | Three fingers still initiate; horizontal scrolling is intercepted only in the follow-up window. |
| No-animation edge window / `edgeGraceMs` | `400` | 0–1500 ms; zero disables this exception. |

JSON requires `"schemaVersion": 1`; other keys are optional and default as above.
The solid background has an **RGB/hex colour picker with additional HSL controls**.
Choose **Solid colour**, select a colour, then save and reload explicitly. Cancel
discards the selection. Wallpaper mode retains the solid colour for later use.
`overscrollColor` is an opaque `#RRGGBB` string, defaulting to `#000000` when absent
from older files. The historical `overscrollBackground: "black"` value now means
solid colour; `"wallpaper"` retains the adjacent-wallpaper mode. Alpha is not supported.

New colour settings require matching renderer **and** controller builds. Older
strict schema-1 parsers reject the added property; the editor refuses to reload
either component if colour support is missing. Do not save with the new editor
until those builds are installed if the older setup must survive a restart.

Unknown keys, wrong types, non-finite or out-of-range values reject the whole
reload. Three fingers, ±45° acquisition, axis locking and the velocity estimator
are intentionally not exposed as tuning controls in this slice.

Settings do not change tracking halfway through a gesture or settling chain.
Controller contexts snapshot them; renderer changes defer until idle. Speed
multipliers scale spring time, not finger displacement, with fractional-time carry
for slow speeds/high refresh. 1× preserves the tuned animation and KDE's global
animation factor. Fast-switch speed and flick recognition are independent.

Use **Save gesture settings**, then **Apply saved settings**. Applying preflights
component support and paths, never enables plugins,
and refuses older cached libraries without this feature. Overscroll is included
in the one project renderer; input settings need the controller. Stock Slide and
InputActions bindings are never configured here.

Temporary developer comparison commands override the saved background only until
renderer reload/unload; enabling without `--background` uses saved settings.
Saved gesture files persist after rollback; effect recovery never discards the
user's saved choices. See [validation status](PLAN.md) for test coverage.

When [login startup](STARTUP.md) is installed, stop/disable its user service for
recovery instead of using the timed manual-test helper's separate baseline file.

## Read-only diagnostics

```sh
build-effect/natural-desktop-slide-diagnostic --config examples/panels.json
build-effect/natural-desktop-slide-diagnostic --config examples/panels.json \
  --snapshot examples/panel-snapshot.json
build-effect/natural-desktop-slide-diagnostic --discover
```

The example snapshot is synthetic. `--discover` reads Plasma layout metadata and
`kscreen-doctor -j`; layout dimensions are not exact KWin frame geometry. It does
not require the custom effect. Exit codes: 0 success, 1 data/read error, 2 usage.

For exact frames and matching decisions, query the **loaded** custom effect:

```sh
qdbus-qt6 org.kde.KWin /org/kde/KWin/Effects/NaturalDesktopSlide \
  org.kde.KWin.NaturalDesktopSlide.panelSnapshot
qdbus-qt6 org.kde.KWin /org/kde/KWin/Effects/NaturalDesktopSlide \
  org.kde.KWin.NaturalDesktopSlide.configurationStatus
```

Snapshots contain `label`, `isPlasmaPanel`, `output`, `geometry`, and
`outputGeometry: [0,0,width,height]`, plus live diagnostic fields. UUID labels are
not persistent matching IDs. Snapshot mode trusts supplied identity and does not
model desktop membership; a non-sticky dock can therefore differ from the live
decision. During animation the renderer uses frozen decisions, not reloaded rules.
Store private captures under ignored `local/`.
