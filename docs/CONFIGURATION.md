# Settings

<!-- Intended audience: users choosing panels and adjusting gestures. -->

You do not need to edit any files.

## Choose which panels slide

1. Open **Natural Desktop Slide Settings** from the application menu.
2. In **Panels**, choose **Add panel by clicking…**, then click a Plasma panel.
   Press Esc if you want to cancel.
3. Tick **Slide** for panels that should move with the desktop. Leave it unticked
   for panels that should stay in place.
4. Choose **Save all panel choices**.
5. If the effect is already running, use **Reload configuration in active effect**
   to apply your saved choices.

Panels stay fixed unless you select them. Their contents are shared across
desktops; this does not create separate panels for each desktop.

To stop a previously selected panel sliding, pick it again, untick **Slide** and
save. Removing a row from the editor does not undo a previously saved choice.
Pick panels again if you move them or change your screen layout or scaling.

On KWin 6.8, the same editor is also available in System Settings → **Animations**
→ virtual-desktop switching → **Natural Desktop Slide (Experimental)** → configuration.
Saving settings does not turn on the effect. Use the [installation guide](BINARY_PACKAGES.md)
to activate it; do not enable it alongside KDE's original Slide effect.

## Adjust the feel

Open **Gestures and animation**. Start with the defaults, then adjust as needed:

- **Movement per desktop:** lower values need less finger travel.
- **Slow-swipe completion:** how far a slow swipe must go before it switches desktops.
- **Minimum flick speed:** how fast a short swipe must be to count as a flick.
- **Animation speeds:** how quickly the animation finishes after you release.
- **Overscroll:** the gentle resistance when swiping past the first or last desktop.
  You can choose a solid background colour or moving wallpaper.
- **Two-finger follow-ups:** continue switching with horizontal two-finger movement
  while the previous animation is finishing. Turn this off if it interferes with scrolling.

Choose **Save gesture settings**, then **Reload settings in active renderer /
controller** if the effect is running. **Restore tuned defaults** only changes the
editor until you save and reload.

## If something does not look right

- Fully close and reopen System Settings after installing or updating the effect.
- Re-pick panels after changing their position, monitor or display scaling.
- If reload reports an error, the last valid settings remain active. Do not force
  a reload with mismatched versions; see [updating](BINARY_PACKAGES.md#upgrade-or-remove).
- To turn the effect off, follow [startup and recovery](STARTUP.md).

For exact defaults, file locations, manual rules and diagnostics, see the
[configuration reference](CONFIGURATION_REFERENCE.md).
