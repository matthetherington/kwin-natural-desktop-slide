# How swipes work

<!-- Intended audience: users learning the gestures and their limitations. -->

## Switch desktops

Swipe horizontally with **three fingers** on your touchpad. The desktop follows
your fingers, so you can reverse direction or return to where you started to cancel.

- A slow swipe switches desktops after about **30%** of a desktop by default.
- A quick flick can switch even if you have not moved that far.
- If you start another swipe before the animation finishes, it continues from
  the current position rather than snapping back first.
- At the first or last desktop, the animation gently resists further movement
  and returns when you let go. If desktop wrapping is enabled, it uses the next
  desktop instead.

## Continue with two fingers

After releasing a three-finger swipe, you can continue with horizontal two-finger
movement **while the animation is still finishing**. Once it stops, normal scrolling
resumes and a new desktop switch needs three fingers again.

This can briefly intercept horizontal scrolling in applications. You can turn off
**Two-finger follow-ups** in [settings](CONFIGURATION.md). The touchpad events do
not reliably reveal finger count during these follow-ups; this is not palm rejection.

## Panels and multiple screens

Only panels you select slide. Other panels remain fixed, and panel contents do
not change between desktops.

With shared desktops, all screens switch together. With independent desktops,
the gesture controls the screen active when it begins. Screen layout changes
cancel a gesture. Multi-screen behaviour has been tested with virtual screens,
not yet physical multi-monitor hardware.

## Limitations

- Vertical three-finger gestures are consumed while this controller is active;
  they are not passed to another gesture tool.
- Other desktop gesture tools can conflict. InputActions is optional and its
  settings are not edited, but other conflicting tools must be disabled yourself.
- A mouse wheel cannot start a follow-up, and a touchpad without three-finger
  gesture support cannot start a swipe.

See [compatibility](COMPATIBILITY.md) for system requirements, or
[gesture internals](GESTURE_INTERNALS.md) for exact thresholds and diagnostics.
