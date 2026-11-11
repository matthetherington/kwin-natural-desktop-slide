#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Session-only effect switching. Never writes kwinrc or restarts KWin."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

CUSTOM = "kwin_natural_desktop_slide"
STOCK = "slide"
CONTROLLER_BASE = "kwin_natural_desktop_slide_controller"
_manifest = Path(__file__).resolve().parents[1] / "build-controller/effect/controller-plugin-id.txt"
CONTROLLER = _manifest.read_text().strip() if _manifest.exists() else CONTROLLER_BASE
if not re.fullmatch(r"kwin_natural_desktop_slide_controller(?:_dev[a-zA-Z0-9]+)?", CONTROLLER):
    raise RuntimeError("Invalid controller identity in build manifest")
INPUTACTIONS = "kwin_gestures"
ROOT = Path(__file__).resolve().parents[1]
_preview_manifest = ROOT / "build-controller/effect/renderer-plugin-id.txt"
PREVIEW = _preview_manifest.read_text().strip() if _preview_manifest.exists() else CUSTOM
if not re.fullmatch(r"kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?", PREVIEW):
    raise RuntimeError("Invalid preview renderer identity in build manifest")


class SessionEffects:
    preview_state = ROOT / "local/session/renderer-session.json"
    def __init__(self, runner=subprocess.run):
        self.runner = runner
        # Debian/Ubuntu also ship the un-suffixed executable outside PATH.
        self.qdbus = next((path for name in ("qdbus-qt6", "qdbus6", "qdbus")
                           if (path := shutil.which(name))), None)
        if not self.qdbus:
            for name in ("qtpaths6", "qtpaths-qt6"):
                qtpaths = shutil.which(name)
                if not qtpaths:
                    continue
                result = runner([qtpaths, "--query", "QT_HOST_BINS"], check=True,
                                capture_output=True, text=True, timeout=15)
                candidate = Path(result.stdout.strip()) / "qdbus"
                if candidate.is_file() and os.access(candidate, os.X_OK):
                    self.qdbus = str(candidate)
                    break
        if not self.qdbus:
            raise RuntimeError("qdbus is required (qdbus-qt6, qdbus6, qdbus or Qt 6's host tools directory).")

    def call(self, method, *args):
        result = self.runner(
            [self.qdbus, "org.kde.KWin", "/Effects", "org.kde.kwin.Effects." + method, *args],
            check=True, capture_output=True, text=True, timeout=15,
        )
        return result.stdout.strip()

    def loaded(self, name):
        value = self.call("isEffectLoaded", name)
        if value not in ("true", "false"):
            raise RuntimeError("Unexpected isEffectLoaded response: " + repr(value))
        return value == "true"

    def preview_names(self):
        return sorted({PREVIEW} | {name for name in self.call("listOfEffects").splitlines()
                                   if re.fullmatch(r"kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?", name)})

    def loaded_renderers(self):
        return [name for name in dict.fromkeys([STOCK, CUSTOM, *self.preview_names()]) if self.loaded(name)]

    def preview_baseline(self):
        if self.preview_state is None:
            return None
        if self.preview_state.is_symlink():
            raise RuntimeError("Refusing symlink recovery state")
        if not self.preview_state.exists():
            return None
        baseline = json.loads(self.preview_state.read_text())
        if (not isinstance(baseline, dict) or baseline.get("renderer") not in (STOCK, CUSTOM)
                or type(baseline.get("inputActions", True)) is not bool):
            raise RuntimeError("Invalid preview restore baseline")
        return baseline

    def rollback(self):
        baseline = self.preview_baseline() # Validate before changing either layer.
        restore_inputactions = baseline.get("inputActions", True) if baseline else None
        if self.any_controller_loaded():
            self.controller_rollback(restore_inputactions=restore_inputactions)
        for name in [CUSTOM, *self.preview_names()]:
            if self.loaded(name):
                self.call("unloadEffect", name)
            if self.loaded(name):
                raise RuntimeError("Custom effect is still loaded; refusing to load stock Slide alongside it.")
        if not self.loaded(STOCK):
            if self.call("loadEffect", STOCK) != "true":
                raise RuntimeError("Could not restore stock Slide.")
        if not self.loaded(STOCK):
            raise RuntimeError("Stock Slide restoration could not be verified.")
        if self.preview_state is not None and self.preview_state.exists():
            self.controller_rollback(restore_inputactions=restore_inputactions)
            self.preview_state.unlink()

    def enable(self):
        if self.any_controller_loaded():
            raise RuntimeError("Restore the gesture controller before changing renderers.")
        if self.loaded(CUSTOM):
            raise RuntimeError("Custom effect already loaded; use status or rollback, not a second enable.")
        if self.loaded_renderers() != [STOCK]:
            raise RuntimeError("Expected stock Slide to be loaded first; no effects changed.")
        known = self.call("listOfEffects").splitlines()
        if CUSTOM not in known:
            raise RuntimeError("Custom plugin is not discoverable. Install it first; no effects changed.")
        try:
            self.call("unloadEffect", STOCK)
            if self.loaded(STOCK):
                raise RuntimeError("Stock Slide is still loaded; custom effect was not loaded.")
            if self.call("loadEffect", CUSTOM) != "true" or not self.loaded(CUSTOM):
                raise RuntimeError("Custom effect failed to load.")
            if self.loaded(STOCK):
                raise RuntimeError("Stock Slide unexpectedly reloaded.")
        except (Exception, KeyboardInterrupt) as error:
            try:
                self.rollback()
            except Exception as recovery:
                raise RuntimeError(f"Activation failed: {error}. Rollback also failed: {recovery}") from error
            raise RuntimeError(f"Activation failed; stock Slide restored: {error}") from error

    def controller_names(self):
        names = {CONTROLLER_BASE, CONTROLLER}
        names.update(name for name in self.call("listOfEffects").splitlines()
                     if re.fullmatch(r"kwin_natural_desktop_slide_controller(?:_dev[a-zA-Z0-9]+)?", name))
        return sorted(names)

    def any_controller_loaded(self):
        return any(self.loaded(name) for name in self.controller_names())

    def controller_rollback(self, restore_inputactions=None):
        for name in self.controller_names():
            if self.loaded(name):
                self.call("unloadEffect", name)
            if self.loaded(name):
                raise RuntimeError("Controller is still loaded; refusing to restore competing InputActions.")
        if restore_inputactions is None:
            restore_inputactions = INPUTACTIONS in self.call("listOfEffects").splitlines()
        if restore_inputactions and not self.loaded(INPUTACTIONS):
            if self.call("loadEffect", INPUTACTIONS) != "true" or not self.loaded(INPUTACTIONS):
                raise RuntimeError("Could not restore InputActions.")

    def controller_enable(self):
        if self.any_controller_loaded():
            raise RuntimeError("Controller already loaded; use controller-rollback first.")
        if len(self.loaded_renderers()) != 1:
            raise RuntimeError("Expected exactly one Slide renderer; nothing changed.")
        inputactions = self.loaded(INPUTACTIONS)
        known = self.call("listOfEffects").splitlines()
        if not inputactions and INPUTACTIONS in known:
            raise RuntimeError("This helper requires InputActions loaded if installed, or completely absent, so rollback is unambiguous.")
        if CONTROLLER not in known:
            raise RuntimeError("Controller plugin is not installed/discoverable; nothing changed.")
        try:
            # InputActions, if installed, initially blocks ownership. On a native
            # KDE session without that third-party plugin, loading takes ownership
            # directly; unloading the controller restores KWin's native handling.
            if self.call("loadEffect", CONTROLLER) != "true" or not self.loaded(CONTROLLER):
                raise RuntimeError("Controller could not load.")
            if inputactions:
                self.call("unloadEffect", INPUTACTIONS)
            if self.loaded(INPUTACTIONS):
                raise RuntimeError("InputActions did not unload; control was not granted.")
        except (Exception, KeyboardInterrupt) as error:
            try:
                self.controller_rollback(restore_inputactions=inputactions)
            except Exception as recovery:
                raise RuntimeError(f"Controller activation failed: {error}. Recovery failed: {recovery}") from error
            raise RuntimeError(f"Controller activation failed; InputActions restored: {error}") from error

    def preview_mode(self, mode):
        if mode not in ("black", "wallpaper"):
            raise RuntimeError("Unknown overscroll background")
        if self.loaded_renderers() != [PREVIEW]:
            raise RuntimeError("Expected the current development preview renderer alone")
        result = self.runner([self.qdbus, "org.kde.KWin", "/org/kde/KWin/Effects/NaturalDesktopSlide",
                              "org.kde.KWin.NaturalDesktopSlide.setOverscrollMode", mode],
                             check=True, capture_output=True, text=True, timeout=15)
        if result.stdout.strip() != "true":
            raise RuntimeError("Renderer refused the mode change; lift fingers and wait until settled")

    def preview_enable(self, mode=None):
        inputactions = self.loaded(INPUTACTIONS)
        if self.any_controller_loaded() or (not inputactions and INPUTACTIONS in self.call("listOfEffects").splitlines()):
            raise RuntimeError("Restore InputActions/controller before changing renderers")
        renderers = self.loaded_renderers()
        if len(renderers) != 1 or renderers[0] not in (STOCK, CUSTOM):
            raise RuntimeError("Expected exactly one original renderer; nothing changed")
        if PREVIEW not in self.call("listOfEffects").splitlines():
            raise RuntimeError("Overscroll preview is not discoverable; nothing changed")
        if mode is not None and mode not in ("black", "wallpaper"):
            raise RuntimeError("Unknown overscroll background")
        self.preview_state.parent.mkdir(parents=True, exist_ok=True)
        # Refuse stale sessions and durably save recovery before any effect change.
        with self.preview_state.open("x") as stream:
            json.dump({"renderer": renderers[0], "inputActions": inputactions}, stream)
            stream.flush()
            os.fsync(stream.fileno())
        try:
            if renderers[0] != PREVIEW:
                self.call("unloadEffect", renderers[0])
                if self.loaded(renderers[0]):
                    raise RuntimeError("Original renderer did not unload")
                if self.call("loadEffect", PREVIEW) != "true" or self.loaded_renderers() != [PREVIEW]:
                    raise RuntimeError("Renderer did not load alone")
            if mode is not None:
                self.preview_mode(mode)
            self.controller_enable()
        except (Exception, KeyboardInterrupt) as error:
            try:
                self.preview_rollback()
            except Exception as recovery:
                raise RuntimeError(f"Preview activation failed: {error}. Recovery failed: {recovery}") from error
            raise RuntimeError(f"Preview activation failed; original renderer/InputActions restored: {error}") from error

    def preview_rollback(self):
        baseline = self.preview_baseline()
        original = baseline["renderer"] if baseline else None
        restore_inputactions = baseline.get("inputActions", True) if baseline else None
        previews = [name for name in self.preview_names() if self.loaded(name)]
        if baseline is None:
            if self.any_controller_loaded() or any(name != CUSTOM for name in previews):
                raise RuntimeError("Missing preview restore baseline; use emergency rollback for stock Slide")
            return # Idempotence must not enable an unrelated, inactive plugin.
        self.controller_rollback(restore_inputactions=restore_inputactions)
        for name in previews:
            if name == original:
                continue # The renderer was already selected before controller activation.
            self.call("unloadEffect", name)
            if self.loaded(name):
                raise RuntimeError("Preview renderer is still loaded; refusing a competing renderer")
        if original is not None:
            active = self.loaded_renderers()
            if active and active != [original]:
                raise RuntimeError("A different renderer is active; refusing to overwrite the user's session choice")
            if not active:
                if self.call("loadEffect", original) != "true" or self.loaded_renderers() != [original]:
                    raise RuntimeError("Could not restore original renderer")
            self.preview_state.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["status", "start", "stop", "enable", "rollback", "controller-enable", "controller-rollback",
                                          "overscroll-enable", "overscroll-rollback", "overscroll-mode"])
    parser.add_argument("--background", choices=["black", "wallpaper"],
                        help="Temporary comparison override; enabling without this uses saved settings.")
    parser.add_argument("--build-dir", type=Path, help="Use controller/renderer identities from this CMake build.")
    args = parser.parse_args()
    try:
        if args.build_dir is not None:
            global CONTROLLER, PREVIEW
            controller = (args.build_dir / "effect/controller-plugin-id.txt").read_text().strip()
            preview = (args.build_dir / "effect/renderer-plugin-id.txt").read_text().strip()
            if not re.fullmatch(r"kwin_natural_desktop_slide_controller(?:_dev[a-zA-Z0-9]+)?", controller) or not re.fullmatch(r"kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?", preview):
                raise RuntimeError("Invalid controller/renderer build identities")
            CONTROLLER, PREVIEW = controller, preview
        effects = SessionEffects()
        if args.action == "enable":
            effects.enable()
            print("Custom effect enabled for this session only; stock Slide unloaded.")
        elif args.action == "rollback":
            effects.rollback()
            print("Stock Slide restored; custom effect unloaded. No persistent settings changed.")
        elif args.action == "controller-enable":
            effects.controller_enable()
            print("Controller enabled; InputActions handed off if present. Renderer/startup settings unchanged.")
        elif args.action == "controller-rollback":
            effects.controller_rollback()
            print("Controller unloaded; previous gesture handling restored. Renderer/startup settings unchanged.")
        elif args.action in ("start", "overscroll-enable"):
            effects.preview_enable(args.background)
            print("Natural Desktop Slide renderer/controller enabled; original renderer saved for recovery.")
        elif args.action in ("stop", "overscroll-rollback"):
            effects.preview_rollback()
            print("Original renderer and gesture handling restored; overscroll preview/controller unloaded.")
        elif args.action == "overscroll-mode":
            effects.preview_mode(args.background)
            print("Temporary overscroll background: " + args.background)
        else:
            print(f"Stock Slide: {effects.loaded(STOCK)}; custom effect: {effects.loaded(CUSTOM)}")
            print(f"Gesture controller: {effects.loaded(CONTROLLER)}; InputActions: {effects.loaded(INPUTACTIONS)}")
            print("Loaded supported renderers: " + ", ".join(effects.loaded_renderers()))
    except Exception as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
