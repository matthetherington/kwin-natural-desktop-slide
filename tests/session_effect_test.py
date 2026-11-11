# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mocked rollback tests; never contact the real session bus."""
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "effect_session", Path(__file__).resolve().parents[1] / "tools/effect-session.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
# Exercise separate development identities as well as the canonical unified one.
module.PREVIEW = "kwin_natural_desktop_slide_devtest"


class FakeEffects(module.SessionEffects):
    def __init__(self, discoverable=True, load_failure=False, unload_failure=False):
        self.state = {module.STOCK}
        self.discoverable = discoverable
        self.load_failure = load_failure
        self.unload_failure = unload_failure
        self.history = []
        self.preview_state = None # Never access real private session state in mocks.

    def call(self, method, *args):
        self.history.append((method, args))
        name = args[0] if args else None
        if method == "isEffectLoaded":
            return "true" if name in self.state else "false"
        if method == "listOfEffects":
            return module.CUSTOM if self.discoverable else module.STOCK
        if method == "unloadEffect":
            if not (self.unload_failure and name == module.STOCK):
                self.state.discard(name)
            return ""
        if method == "loadEffect":
            if self.load_failure and name == module.CUSTOM:
                return "false"
            if name == module.CUSTOM:
                assert module.STOCK not in self.state
            if name == module.STOCK:
                assert module.CUSTOM not in self.state
            self.state.add(name)
            return "true"
        raise AssertionError(method)


class FakeControllerEffects(FakeEffects):
    def __init__(self, fail_load=False, fail_unload=False):
        super().__init__()
        self.state = {module.CUSTOM, module.INPUTACTIONS}
        self.fail_load = fail_load
        self.fail_unload = fail_unload
        self.inputactions_available = True

    def call(self, method, *args):
        if method == "listOfEffects":
            return '\n'.join([module.CUSTOM, module.CONTROLLER] + ([module.INPUTACTIONS] if self.inputactions_available else []))
        if method == "loadEffect" and args[0] == module.CONTROLLER and self.fail_load:
            return "false"
        if method == "unloadEffect" and args[0] == module.INPUTACTIONS and self.fail_unload:
            return ""
        if method == "loadEffect" and args[0] == module.INPUTACTIONS:
            assert module.CONTROLLER not in self.state
        return super().call(method, *args)


class FakePreviewEffects(FakeControllerEffects):
    def __init__(self, state_path, fail_renderer=False, fail_mode=False, fail_controller=False, fail_unload=False):
        super().__init__(fail_load=fail_controller)
        self.preview_state = state_path
        self.fail_renderer = fail_renderer
        self.fail_mode = fail_mode
        self.fail_preview_unload = fail_unload

    def preview_mode(self, mode):
        if self.fail_mode:
            raise RuntimeError("simulated mode failure")
        self.mode = mode

    def call(self, method, *args):
        if method == "listOfEffects":
            return '\n'.join([module.STOCK, module.CUSTOM, module.CONTROLLER, module.PREVIEW,
                              "kwin_natural_desktop_slide_dev999"] + ([module.INPUTACTIONS] if self.inputactions_available else []))
        if method == "loadEffect" and args[0] in (module.STOCK, module.CUSTOM, module.PREVIEW):
            assert not self.loaded_renderers(), 'never load competing renderers'
            if args[0] == module.PREVIEW and self.fail_renderer:
                return "false"
        if method == "unloadEffect" and args[0] == module.PREVIEW and self.fail_preview_unload:
            return ""
        return super().call(method, *args)


class SessionEffectTest(unittest.TestCase):
    def test_canonical_renderer_is_counted_once_and_restores_stock(self):
        with patch.object(module, "PREVIEW", module.CUSTOM), tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory) / "session.json")
            effects.state = {module.STOCK, module.INPUTACTIONS}
            effects.preview_enable()
            self.assertEqual(effects.loaded_renderers(), [module.CUSTOM])
            effects.preview_rollback()
            self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})

    def test_canonical_renderer_already_selected_is_not_reloaded(self):
        with patch.object(module, "PREVIEW", module.CUSTOM), tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory) / "session.json")
            effects.preview_enable()
            effects.preview_rollback()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})
            self.assertFalse(any(args == (module.CUSTOM,) and method in ("loadEffect", "unloadEffect") for method, args in effects.history))

    def test_qdbus_names_and_qt_host_directory(self):
        for name in ("qdbus-qt6", "qdbus6", "qdbus"):
            with patch.object(module.shutil, "which", side_effect=lambda value: "/tools/" + value if value == name else None):
                self.assertEqual(module.SessionEffects().qdbus, "/tools/" + name)
        with tempfile.TemporaryDirectory() as directory:
            candidate = Path(directory) / "qdbus"
            candidate.write_text("mock")
            candidate.chmod(0o755)
            with patch.object(module.shutil, "which", side_effect=lambda value: "/tools/qtpaths6" if value == "qtpaths6" else None):
                runner = lambda *args, **kwargs: SimpleNamespace(stdout=directory + "\n")
                self.assertEqual(module.SessionEffects(runner=runner).qdbus, str(candidate))

    def test_without_inputactions_restores_native_gestures(self):
        for failure in (False, True):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                effects = FakePreviewEffects(Path(directory) / "session.json", fail_controller=failure)
                effects.state.remove(module.INPUTACTIONS)
                effects.inputactions_available = False
                if failure:
                    with self.assertRaises(RuntimeError):
                        effects.preview_enable()
                else:
                    effects.preview_enable()
                    self.assertIs(json.loads(effects.preview_state.read_text())["inputActions"], False)
                    effects.preview_rollback()
                self.assertEqual(effects.state, {module.CUSTOM})
                self.assertFalse(any(args == (module.INPUTACTIONS,) and method.endswith("loadEffect") for method, args in effects.history))

    def test_invalid_input_owner_does_not_change_any_effects(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory) / "session.json")
            effects.preview_state.write_text(json.dumps({"renderer": module.CUSTOM, "inputActions": "false"}))
            with self.assertRaisesRegex(RuntimeError, "Invalid"):
                effects.preview_rollback()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})
            self.assertEqual(effects.history, [])

    def test_native_baseline_does_not_enable_later_installed_inputactions(self):
        for emergency in (False, True):
            with self.subTest(emergency=emergency), tempfile.TemporaryDirectory() as directory:
                effects = FakePreviewEffects(Path(directory) / "session.json")
                effects.state.remove(module.INPUTACTIONS)
                effects.inputactions_available = False
                effects.preview_enable()
                effects.inputactions_available = True # Installed during the test, but not enabled.
                if emergency:
                    effects.rollback()
                    self.assertEqual(effects.state, {module.STOCK})
                else:
                    effects.preview_rollback()
                    effects.preview_rollback()
                    self.assertEqual(effects.state, {module.CUSTOM})
                self.assertNotIn(module.INPUTACTIONS, effects.state)

    def test_enable_and_rollback(self):
        effects = FakeEffects()
        effects.enable()
        self.assertEqual(effects.state, {module.CUSTOM})
        effects.rollback()
        effects.rollback()  # Recovery is idempotent.
        self.assertEqual(effects.state, {module.STOCK})

    def test_failed_load_restores_stock(self):
        effects = FakeEffects(load_failure=True)
        with self.assertRaisesRegex(RuntimeError, "stock Slide restored"):
            effects.enable()
        self.assertEqual(effects.state, {module.STOCK})

    def test_failed_unload_does_not_load_custom(self):
        effects = FakeEffects(unload_failure=True)
        with self.assertRaises(RuntimeError):
            effects.enable()
        self.assertEqual(effects.state, {module.STOCK})
        self.assertNotIn(("loadEffect", (module.CUSTOM,)), effects.history)

    def test_missing_plugin_changes_nothing(self):
        effects = FakeEffects(discoverable=False)
        with self.assertRaisesRegex(RuntimeError, "not discoverable"):
            effects.enable()
        self.assertFalse(any(method.endswith("loadEffect") for method, _ in effects.history))

    def test_unexpected_initial_state_changes_nothing(self):
        effects = FakeEffects()
        effects.state.clear()
        with self.assertRaisesRegex(RuntimeError, "Expected stock"):
            effects.enable()
        self.assertEqual(effects.state, set())

    def test_controller_handoff_and_restore_leave_renderer(self):
        effects = FakeControllerEffects()
        effects.controller_enable()
        self.assertEqual(effects.state, {module.CUSTOM, module.CONTROLLER})
        effects.controller_rollback()
        effects.controller_rollback()
        self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})

    def test_controller_failures_restore_inputactions(self):
        for effects in [FakeControllerEffects(fail_load=True), FakeControllerEffects(fail_unload=True)]:
            with self.assertRaisesRegex(RuntimeError, "InputActions restored"):
                effects.controller_enable()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})

    def test_controller_requires_known_initial_owner(self):
        effects = FakeControllerEffects()
        effects.state.remove(module.INPUTACTIONS)
        with self.assertRaisesRegex(RuntimeError, "requires InputActions"):
            effects.controller_enable()
        self.assertEqual(effects.state, {module.CUSTOM})

    def test_emergency_rollback_restores_both_layers(self):
        effects = FakeControllerEffects()
        effects.controller_enable()
        effects.rollback()
        self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})

    def test_preview_handoff_and_idempotent_restore(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_enable("wallpaper")
            self.assertEqual(effects.state, {module.PREVIEW, module.CONTROLLER})
            self.assertEqual(effects.mode, "wallpaper")
            effects.preview_rollback()
            effects.preview_rollback()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})
            self.assertFalse(effects.preview_state.exists())

    def test_preview_failures_restore_both_original_layers(self):
        for failure in ("fail_renderer", "fail_mode", "fail_controller"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                effects = FakePreviewEffects(Path(directory)/"session.json", **{failure: True})
                with self.assertRaisesRegex(RuntimeError, "original renderer/InputActions restored"):
                    effects.preview_enable("black")
                self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})

    def test_preview_unload_failure_never_loads_competitor(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_enable()
            effects.fail_preview_unload = True
            with self.assertRaisesRegex(RuntimeError, "still loaded"):
                effects.preview_rollback()
            self.assertEqual(effects.state, {module.PREVIEW, module.INPUTACTIONS})
            self.assertTrue(effects.preview_state.exists())

    def test_preview_requires_known_baseline_and_original_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_state.write_text('{"renderer": "slide"}')
            with self.assertRaises(FileExistsError):
                effects.preview_enable()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})
            effects.preview_state.unlink()
            effects.state = {module.PREVIEW, module.INPUTACTIONS}
            with self.assertRaisesRegex(RuntimeError, "Missing preview restore baseline"):
                effects.preview_rollback()

    def test_preview_rollback_handles_old_generations(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_state.write_text('{"renderer": "kwin_natural_desktop_slide"}')
            effects.state = {"kwin_natural_desktop_slide_dev999", module.INPUTACTIONS}
            effects.preview_rollback()
            self.assertEqual(effects.state, {module.CUSTOM, module.INPUTACTIONS})

    def test_emergency_stock_rollback_handles_preview(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_enable()
            effects.rollback()
            self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})
            self.assertFalse(effects.preview_state.exists())

    def test_mode_request_and_busy_refusal_do_not_toggle_effects(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.state = {module.PREVIEW, module.INPUTACTIONS}
            effects.qdbus = "mock-qdbus"
            calls = []
            def runner(command, **kwargs):
                calls.append(command)
                return SimpleNamespace(stdout="false")
            effects.runner = runner
            with self.assertRaisesRegex(RuntimeError, "wait until settled"):
                module.SessionEffects.preview_mode(effects, "wallpaper")
            self.assertEqual(calls[0][-2:], ["org.kde.KWin.NaturalDesktopSlide.setOverscrollMode", "wallpaper"])
            self.assertEqual(effects.state, {module.PREVIEW, module.INPUTACTIONS})

    def test_preview_can_restore_stock_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.state = {module.STOCK, module.INPUTACTIONS}
            effects.preview_enable()
            effects.preview_rollback()
            self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})

    def test_default_preview_does_not_override_saved_background(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_enable()
            self.assertFalse(hasattr(effects, "mode"))
            effects.preview_rollback()

    def test_failed_original_unload_does_not_load_preview(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.state = {module.STOCK, module.INPUTACTIONS}
            effects.unload_failure = True
            with self.assertRaisesRegex(RuntimeError, "original renderer/InputActions restored"):
                effects.preview_enable()
            self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})
            self.assertNotIn(("loadEffect", (module.PREVIEW,)), effects.history)

    def test_preview_recovery_respects_unexpected_renderer_choice(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = FakePreviewEffects(Path(directory)/"session.json")
            effects.preview_enable()
            effects.state.add(module.STOCK) # Simulate an external renderer change.
            with self.assertRaisesRegex(RuntimeError, "different renderer"):
                effects.preview_rollback()
            self.assertEqual(effects.state, {module.STOCK, module.INPUTACTIONS})
            self.assertTrue(effects.preview_state.exists())


if __name__ == "__main__":
    unittest.main()
