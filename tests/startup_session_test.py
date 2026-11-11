# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mocked startup tests; no real session bus, units, plugins or home configuration."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from session_effect_test import FakePreviewEffects, module as effects_module

spec = importlib.util.spec_from_file_location("startup_session", Path(__file__).resolve().parents[1]/"tools/startup-session.py")
startup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(startup)


class StartupSessionTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.state = Path(self.directory.name)/"renderer-session.json"
        self.config = {"schemaVersion": 1, "controllerId": effects_module.CONTROLLER,
                       "rendererId": effects_module.PREVIEW, "stateFile": str(self.state)}
        self.effects = FakePreviewEffects(self.state)
        self.now = 0
        self.session = startup.StartupSession(self.effects, self.config, lambda: self.now, self.advance)

    def advance(self, seconds):
        self.now += seconds

    def test_login_handoff_and_stop_restore_original(self):
        self.session.start()
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})
        self.assertFalse(hasattr(self.effects, "mode")) # No saved-settings override.
        self.session.stop()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM, effects_module.INPUTACTIONS})
        self.assertFalse(self.state.exists())

    def test_login_without_inputactions(self):
        self.effects.state.remove(effects_module.INPUTACTIONS)
        self.effects.inputactions_available = False
        self.session.start()
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})
        self.session.stop()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM})

    def test_can_adopt_current_test_without_toggling_effects(self):
        self.effects.preview_enable()
        before = list(self.effects.history)
        self.assertIn("adopted", self.session.start())
        self.assertEqual(before, self.effects.history[:len(before)])
        self.assertFalse(any(method in ("loadEffect", "unloadEffect") for method, _ in self.effects.history[len(before):]))

    def test_unclean_previous_logout_is_recovered(self):
        self.state.write_text(json.dumps({"renderer": effects_module.CUSTOM}))
        self.session.start()
        self.assertEqual(len(list(self.state.parent.glob("previous-session-*.json"))), 1)
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})

    def test_mismatched_baseline_changes_nothing(self):
        self.state.write_text('{"renderer":"slide"}')
        with self.assertRaisesRegex(RuntimeError, "differs"):
            self.session.start()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM, effects_module.INPUTACTIONS})

    def test_active_preview_without_managed_baseline_not_adopted_or_stopped(self):
        self.effects.state = {effects_module.PREVIEW, effects_module.CONTROLLER}
        with self.assertRaisesRegex(RuntimeError, "no managed"):
            self.session.start()
        with self.assertRaisesRegex(RuntimeError, "Missing managed"):
            self.session.stop()
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})

    def test_waits_for_inputactions_and_then_hands_off(self):
        self.effects.state.remove(effects_module.INPUTACTIONS)
        def ready_later(seconds):
            self.advance(seconds)
            self.effects.state.add(effects_module.INPUTACTIONS)
        self.session.sleep = ready_later
        self.session.start()
        self.assertEqual(self.now, 0.5)
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})

    def test_timeout_does_not_change_effects(self):
        self.effects.state.remove(effects_module.INPUTACTIONS)
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            self.session.start(wait_seconds=1)
        self.assertEqual(self.effects.state, {effects_module.CUSTOM})
        self.assertFalse(self.state.exists())

    def test_failure_restores_renderer_and_inputactions(self):
        self.effects.fail_load = True
        with self.assertRaises(RuntimeError):
            self.session.start()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM, effects_module.INPUTACTIONS})

    def test_invalid_config_and_state_are_rejected(self):
        path = Path(self.directory.name)/"startup.json"
        path.write_text(json.dumps(self.config))
        self.assertEqual(startup.read_config(path), self.config)
        path.write_text(json.dumps(dict(self.config, rendererId="slide")))
        with self.assertRaisesRegex(RuntimeError, "renderer identity"):
            startup.read_config(path)
        self.state.write_text('{"renderer":"not-ours"}')
        with self.assertRaisesRegex(RuntimeError, "original renderer"):
            self.session.start()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM, effects_module.INPUTACTIONS})

    def test_stopped_service_is_idempotent(self):
        self.session.stop()
        self.assertEqual(self.effects.state, {effects_module.CUSTOM, effects_module.INPUTACTIONS})


if __name__ == "__main__":
    unittest.main()
