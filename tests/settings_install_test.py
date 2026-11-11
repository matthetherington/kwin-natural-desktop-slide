# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Use temporary directories only; never install into the system."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "install_settings", Path(__file__).resolve().parents[1] / "tools/install-settings.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SettingsInstallTest(unittest.TestCase):
    def test_old_open_file_keeps_old_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "effect.so"
            destination.write_bytes(b"original loaded plugin")
            old_inode = destination.stat().st_ino
            with destination.open("rb") as loaded:
                module.atomic_write(destination, b"new plugin metadata")
                self.assertEqual(loaded.read(), b"original loaded plugin")
                self.assertNotEqual(destination.stat().st_ino, old_inode)
            self.assertEqual(destination.read_bytes(), b"new plugin metadata")

    def test_preflight_failure_leaves_files_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.write_bytes(b"new")
            destination = root / "destination"
            destination.write_bytes(b"old")
            with self.assertRaises(FileNotFoundError):
                module.install([(source, destination), (root / "missing", root / "other")])
            self.assertEqual(destination.read_bytes(), b"old")

    def test_partial_install_failure_restores_previous_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.write_bytes(b"new")
            first, second = root / "first.so", root / "second.so"
            first.write_bytes(b"original")
            first.chmod(0o640)
            write = module.atomic_write

            def fail_second(destination, data, mode=0o755):
                if destination == second:
                    raise OSError("simulated installation failure")
                write(destination, data, mode)

            with patch.object(module, "atomic_write", side_effect=fail_second):
                with self.assertRaisesRegex(OSError, "simulated"):
                    module.install([(source, first), (source, second)])
            self.assertEqual(first.read_bytes(), b"original")
            self.assertEqual(first.stat().st_mode & 0o777, 0o640)
            self.assertFalse(second.exists())

    def make_layout(self, build, plugins="/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects"):
        names = {"renderer": "kwin_natural_slide", "settings": "kwin_natural_slide_config",
                 "controller": "kwin_natural_swipe_controller_test", "overscroll": "kwin_natural_slide_preview42",
                 "probe": "kwin_natural_swipe_probe"}
        layout = {"schemaVersion": 1, "kwinVersion": "6.8.2", "pluginDirectory": plugins, "artifacts": []}
        for role, name in names.items():
            source = build / (name + ".so")
            source.write_bytes(b"mock plugin")
            layout["artifacts"].append({"role": role, "source": str(source), "name": source.name})
        path = build / "install-layout-Debug.json"
        path.write_text(json.dumps(layout))
        return path, layout

    def test_modes_are_narrowly_scoped_without_rpm(self):
        modes = {"--probe-only": ["probe"], "--settings-only": ["settings"],
                 "--controller-only": ["controller"], "--overscroll-preview": ["controller", "overscroll"],
                 "--all": ["renderer", "settings", "controller", "overscroll"]}
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            self.make_layout(build)
            for mode, roles in modes.items():
                with self.subTest(mode=mode), patch.object(module.os, "geteuid", return_value=0), \
                     patch.object(module.subprocess, "check_output", return_value="kwin 6.8.2\n") as version, \
                     patch.object(module.sys, "argv", ["installer", mode, "--build-dir", str(build)]), \
                     patch.object(module.shutil, "which", side_effect=lambda name: "/tools/kwin_wayland" if name == "kwin_wayland" else None), \
                     patch.object(module, "install") as install, patch("builtins.print"):
                    module.main()
                self.assertEqual(install.call_args.args[0], module.build_artifacts(build, roles))
                self.assertEqual(version.call_args.args[0], ["/tools/kwin_wayland", "--version"])

    def test_symlink_destination_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.write_bytes(b"new")
            link = root / "link"
            link.symlink_to(source)
            with self.assertRaisesRegex(RuntimeError, "non-regular"):
                module.install([(source, link)])

    def test_multiarch_paths_and_staging(self):
        for prefix in ("/usr/lib64/qt6/plugins", "/usr/lib/qt6/plugins", "/usr/lib/x86_64-linux-gnu/qt6/plugins"):
            with self.subTest(prefix=prefix), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                self.make_layout(root, prefix + "/kwin/effects")
                artifacts = module.build_artifacts(root, ["settings", "renderer"], destdir=root / "stage")
                self.assertEqual(artifacts[0][1], root / "stage" / prefix.lstrip("/") / "kwin/effects/configs/kwin_natural_slide_config.so")
                module.install(artifacts)
                self.assertTrue(all(destination.read_bytes() == b"mock plugin" for _, destination in artifacts))

    def test_invalid_manifest_cannot_target_stock_plugins(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, layout = self.make_layout(root)
            for identity in ("../slide.so", "slide.so", "kwin_natural_slide.so/../slide.so"):
                layout["artifacts"][0]["name"] = identity
                path.write_text(json.dumps(layout))
                with self.assertRaisesRegex(RuntimeError, "Invalid plugin identity"):
                    module.build_artifacts(root, ["renderer"])

    def test_multiconfig_requires_selection_and_missing_components_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path, layout = self.make_layout(root)
            (root / "install-layout-Release.json").write_text(path.read_text())
            with self.assertRaisesRegex(RuntimeError, "--config"):
                module.build_artifacts(root, ["renderer"])
            self.assertEqual(len(module.build_artifacts(root, ["renderer"], config="Debug")), 1)
            layout["artifacts"] = []
            path.write_text(json.dumps(layout))
            with self.assertRaisesRegex(RuntimeError, "required components"):
                module.build_artifacts(root, ["renderer"], config="Debug")

    def test_wrong_or_missing_kwin_version_is_refused(self):
        with patch.object(module.shutil, "which", return_value="/tools/kwin_wayland"):
            for output in ("kwin 6.8.3", "kwin 6.7.0", "unrecognised", "kwin 6.8.2.1"):
                with patch.object(module.subprocess, "check_output", return_value=output):
                    with self.assertRaisesRegex(RuntimeError, "pinned"):
                        module.check_installed_kwin()
        with patch.object(module.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "required"):
                module.check_installed_kwin()

    def test_dry_run_does_not_write_and_staging_does_not_need_kwin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_layout(root)
            stage = root / "stage"
            with patch.object(module.sys, "argv", ["installer", "--all", "--build-dir", str(root), "--destdir", str(stage), "--dry-run"]), \
                 patch.object(module, "check_installed_kwin") as version, patch("builtins.print"):
                module.main()
            version.assert_not_called()
            self.assertFalse(stage.exists())
            with self.assertRaisesRegex(RuntimeError, "staging root"):
                module.build_artifacts(root, ["renderer"], destdir=Path("/"))


if __name__ == "__main__":
    unittest.main()
