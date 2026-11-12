#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Guarded Plasma login handoff; uses pinned installed plugins, never edits kwinrc."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import uuid


def read_config(path):
    config = json.loads(Path(path).read_text())
    if set(config) != {"schemaVersion", "controllerId", "rendererId", "stateFile"} or config["schemaVersion"] != 1:
        raise RuntimeError("Invalid startup configuration schema")
    if not isinstance(config["controllerId"], str) or not re.fullmatch(r"kwin_natural_desktop_slide_controller(?:_dev[a-zA-Z0-9]+)?", config["controllerId"]):
        raise RuntimeError("Invalid pinned controller identity")
    if not isinstance(config["rendererId"], str) or not re.fullmatch(r"kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?", config["rendererId"]):
        raise RuntimeError("Invalid pinned renderer identity")
    state = Path(config["stateFile"])
    if not state.is_absolute() or state.name != "renderer-session.json":
        raise RuntimeError("Invalid startup recovery state path")
    return config


def create_effects(config):
    # Install a private snapshot beside this script, avoiding a dependency on the
    # working tree or its changing CMake manifests at the next login.
    helper = Path(__file__).with_name("effect-session.py")
    spec = importlib.util.spec_from_file_location("natural_desktop_slide_startup_effects", helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.CONTROLLER = config["controllerId"]
    module.PREVIEW = config["rendererId"]
    effects = module.SessionEffects()
    effects.preview_state = Path(config["stateFile"])
    return effects


class StartupSession:
    def __init__(self, effects, config, clock=time.monotonic, sleep=time.sleep):
        self.effects = effects
        self.config = config
        self.clock = clock
        self.sleep = sleep

    def baseline(self):
        baseline = self.read_baseline(self.effects.preview_state)
        return baseline["renderer"] if baseline is not None else None

    def read_baseline(self, state):
        if state.is_symlink():
            raise RuntimeError("Refusing symlink recovery state")
        if not state.exists():
            return None
        baseline = json.loads(state.read_text())
        if not isinstance(baseline, dict):
            raise RuntimeError("Invalid startup recovery state")
        original = baseline.get("renderer")
        if original not in ("slide", "kwin_natural_desktop_slide"):
            raise RuntimeError("Invalid original renderer in startup recovery state")
        if type(baseline.get("inputActions", True)) is not bool:
            raise RuntimeError("Invalid input owner in startup recovery state")
        return baseline

    @property
    def startup_baseline(self):
        # Separate from the live record consumed by preview_rollback at logout.
        return self.effects.preview_state.with_name("startup-baseline.json")

    def write_baseline(self, path, baseline):
        with open(path, "x", opener=lambda name, flags: os.open(name, flags, 0o600)) as stream:
            json.dump(baseline, stream)
            stream.flush()
            os.fsync(stream.fileno())

    def resume(self):
        baseline = self.read_baseline(self.effects.preview_state)
        if baseline is None:
            baseline = self.read_baseline(self.startup_baseline)
            if baseline is None:
                return False
            self.write_baseline(self.effects.preview_state, baseline)
        # Recheck after creating the recovery record, before touching effects.
        if self.probe()[0] != "empty":
            raise RuntimeError("Effects changed during startup; refusing direct activation")
        inputactions = self.effects.loaded("kwin_gestures")
        try:
            renderer = self.config["rendererId"]
            if (self.effects.call("loadEffect", renderer) != "true"
                    or self.effects.loaded_renderers() != [renderer]):
                raise RuntimeError("Pinned renderer did not load alone")
            self.effects.controller_enable()
        except (Exception, KeyboardInterrupt) as error:
            # Restore the actual pre-start state, not a renderer absent at login.
            # Keep the recovery record, including if cleanup itself fails.
            try:
                self.effects.controller_rollback(restore_inputactions=inputactions)
                if self.effects.loaded(renderer):
                    self.effects.call("unloadEffect", renderer)
                if self.effects.loaded(renderer):
                    raise RuntimeError("Pinned renderer did not unload")
            except Exception as recovery:
                raise RuntimeError(f"Direct activation failed: {error}. Recovery failed: {recovery}") from error
            raise RuntimeError(f"Direct activation failed; login effect state restored: {error}") from error
        return True

    def probe(self):
        known = set(self.effects.call("listOfEffects").splitlines())
        required = {self.config["rendererId"], self.config["controllerId"]}
        if not required <= known:
            return "waiting", "Pinned plugins are not discoverable yet"
        renderers = self.effects.loaded_renderers()
        controllers = [name for name in self.effects.controller_names() if self.effects.loaded(name)]
        inputactions = self.effects.loaded("kwin_gestures")
        if renderers == [self.config["rendererId"]] and controllers == [self.config["controllerId"]] and not inputactions:
            return "active", "Pinned renderer/controller already active"
        if (len(renderers) == 1 and renderers[0] in ("slide", "kwin_natural_desktop_slide") and not controllers
                and (inputactions or "kwin_gestures" not in known)):
            return "original", renderers[0]
        if not renderers and not controllers and (inputactions or "kwin_gestures" not in known):
            return "empty", "No renderer loaded; a saved startup baseline is required"
        return "waiting", "Waiting for one original renderer and the installed gesture owner (InputActions, if present)"

    def start(self, wait_seconds=45):
        original = self.baseline() # Reject malformed state before any effect change.
        deadline = self.clock() + wait_seconds
        reason = "KWin is not ready"
        while True:
            try:
                state, reason = self.probe()
            except (subprocess.SubprocessError, RuntimeError) as error:
                state, reason = "waiting", str(error)
            if state == "active":
                if original is None:
                    raise RuntimeError("Active preview has no managed recovery baseline; refusing to adopt it")
                return "Existing preview adopted without unloading/reloading any effects"
            if state == "empty":
                if self.resume():
                    return "Pinned renderer/controller enabled directly using saved recovery baseline"
            if state == "original":
                if original is not None:
                    if original != reason:
                        raise RuntimeError("Recovery baseline differs from the active renderer; refusing to guess")
                    # Logout/crash may prevent ExecStop. Preserve the stale record
                    # but allow a fresh, known-state handoff at the next login.
                    stale = self.effects.preview_state.with_name("previous-session-" + uuid.uuid4().hex + ".json")
                    self.effects.preview_state.rename(stale)
                self.effects.preview_enable() # Uses saved gesture settings; no overrides.
                return "Pinned renderer/controller enabled; existing gesture owner handed off"
            if self.clock() >= deadline:
                raise RuntimeError("Startup handoff timed out without changing effects: " + reason)
            self.sleep(0.5)

    def stop(self):
        if self.baseline() is None:
            state, _ = self.probe()
            if state == "active":
                raise RuntimeError("Missing managed baseline; refusing to change an unmanaged active preview")
            return "No managed preview to restore"
        baseline = self.read_baseline(self.effects.preview_state)
        temporary = self.startup_baseline.with_name("startup-baseline-" + uuid.uuid4().hex + ".tmp")
        try:
            self.write_baseline(temporary, baseline)
            temporary.replace(self.startup_baseline)
        finally:
            temporary.unlink(missing_ok=True)
        self.effects.preview_rollback()
        return "Original renderer and gesture ownership restored; saved gesture settings preserved"

    def status(self):
        return json.dumps({"renderers": self.effects.loaded_renderers(),
                           "controller": self.effects.loaded(self.config["controllerId"]),
                           "inputActions": self.effects.loaded("kwin_gestures"),
                           "originalRenderer": self.baseline()}, indent=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["start", "stop", "status"])
    parser.add_argument("--config", type=Path, default=Path(__file__).with_name("startup.json"))
    args = parser.parse_args()
    def interrupted(signum, frame):
        raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM, interrupted)
    try:
        config = read_config(args.config)
        session = StartupSession(create_effects(config), config)
        print(getattr(session, args.action)(), flush=True)
        return 0
    except (Exception, KeyboardInterrupt) as error:
        print("Natural Desktop Slide startup: " + (str(error) or "interrupted; recovery attempted"), file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
