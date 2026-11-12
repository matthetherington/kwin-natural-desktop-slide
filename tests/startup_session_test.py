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

    def empty_login(self, managed=True):
        self.effects.state = {effects_module.INPUTACTIONS}
        if managed:
            self.state.write_text('{"renderer":"slide","inputActions":true}')

    def test_empty_login_resumes_directly_and_preserves_baseline(self):
        self.empty_login()
        baseline = self.state.read_bytes()
        self.assertIn("directly", self.session.start())
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})
        self.assertEqual(self.state.read_bytes(), baseline)
        self.assertNotIn(("loadEffect", (effects_module.STOCK,)), self.effects.history)
        self.assertFalse(hasattr(self.effects, "mode"))

    def test_empty_login_without_baseline_changes_nothing(self):
        self.empty_login(managed=False)
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            self.session.start(wait_seconds=1)
        self.assertEqual(self.effects.state, {effects_module.INPUTACTIONS})
        self.assertFalse(self.state.exists())

    def test_clean_shutdown_keeps_separate_baseline_for_next_login(self):
        self.empty_login()
        self.session.start()
        self.session.stop()
        self.assertFalse(self.state.exists())
        self.assertTrue(self.session.startup_baseline.exists())
        self.effects.state = {effects_module.INPUTACTIONS} # New KWin, stock Slide disabled.
        self.effects.history.clear()
        self.session.start()
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})
        self.assertNotIn(("loadEffect", (effects_module.STOCK,)), self.effects.history)
        self.session.stop()
        self.assertEqual(self.effects.state, {effects_module.STOCK, effects_module.INPUTACTIONS})

    def test_empty_login_without_inputactions(self):
        self.empty_login()
        self.effects.state.clear()
        self.effects.inputactions_available = False
        self.state.write_text('{"renderer":"slide","inputActions":false}')
        self.session.start()
        self.assertEqual(self.effects.state, {effects_module.PREVIEW, effects_module.CONTROLLER})

    def test_empty_login_failures_restore_empty_renderer_state(self):
        for failure in ("fail_renderer", "fail_load", "fail_unload"):
            with self.subTest(failure=failure):
                self.empty_login()
                setattr(self.effects, failure, True)
                with self.assertRaisesRegex(RuntimeError, "login effect state restored"):
                    self.session.start()
                setattr(self.effects, failure, False)
                self.assertEqual(self.effects.state, {effects_module.INPUTACTIONS})
                self.assertTrue(self.state.exists())

    def test_empty_login_conflicting_controller_is_not_adopted(self):
        self.empty_login()
        self.effects.state.add(effects_module.CONTROLLER)
        before = set(self.effects.state)
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            self.session.start(wait_seconds=1)
        self.assertEqual(self.effects.state, before)

    def test_empty_login_waits_for_gesture_owner(self):
        self.empty_login()
        self.effects.state.clear()
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            self.session.start(wait_seconds=1)
        self.assertFalse(self.effects.state)

    def test_empty_login_missing_pinned_plugin_changes_nothing(self):
        self.empty_login()
        call = self.effects.call
        def missing_renderer(method, *args):
            result = call(method, *args)
            if method == "listOfEffects":
                return '\n'.join(name for name in result.splitlines() if name != effects_module.PREVIEW)
            return result
        self.effects.call = missing_renderer
        with self.assertRaisesRegex(RuntimeError, "not discoverable"):
            self.session.start(wait_seconds=1)
        self.assertEqual(self.effects.state, {effects_module.INPUTACTIONS})

    def test_direct_start_rechecks_for_new_renderer(self):
        self.empty_login()
        probe = self.session.probe
        def changed_effects():
            self.effects.state.add(effects_module.STOCK)
            return probe()
        self.session.probe = changed_effects
        with self.assertRaisesRegex(RuntimeError, "Effects changed"):
            self.session.resume()
        self.assertEqual(self.effects.state, {effects_module.STOCK, effects_module.INPUTACTIONS})
        self.assertFalse(any(method in ("loadEffect", "unloadEffect") for method, _ in self.effects.history))

    def test_direct_start_cleanup_failure_keeps_recovery_record(self):
        self.empty_login()
        self.effects.fail_load = True
        self.effects.fail_preview_unload = True
        with self.assertRaisesRegex(RuntimeError, "Recovery failed"):
            self.session.start()
        self.assertTrue(self.state.exists())
        self.assertNotIn(effects_module.STOCK, self.effects.state)

    def test_invalid_saved_baseline_is_rejected_without_effect_changes(self):
        self.empty_login(managed=False)
        for value in ('[]', '{"renderer":"not-ours"}', '{"renderer":"slide","inputActions":1}'):
            with self.subTest(value=value):
                self.session.startup_baseline.write_text(value)
                with self.assertRaises(RuntimeError):
                    self.session.start(wait_seconds=1)
                self.assertEqual(self.effects.state, {effects_module.INPUTACTIONS})
                self.assertFalse(self.state.exists())

    def test_symlink_baseline_is_rejected(self):
        self.empty_login(managed=False)
        target = self.state.with_name("target.json")
        target.write_text('{"renderer":"slide"}')
        self.session.startup_baseline.symlink_to(target)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            self.session.start()
        self.assertEqual(self.effects.state, {effects_module.INPUTACTIONS})

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
