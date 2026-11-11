#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


package = load("build_package", "build-package.py")
release = load("prepare_release", "prepare-release.py")
assets = load("verify_assets", "verify-release-assets.py")
session = load("package_session", "package_session.py")
signing = load("sign_release", "sign-release.py")


def info():
    return {"schemaVersion": 1, "version": "0.1.0", "kwinVersion": "6.8.2",
            "rendererId": "kwin_natural_desktop_slide",
            "controllerId": "kwin_natural_desktop_slide_controller", "integrationDriver": False}


class ReleaseToolsTest(unittest.TestCase):
    def test_matrix_and_validation_policy(self):
        targets = json.loads((ROOT / "packaging/targets.json").read_text())
        self.assertEqual(len({x["target"] for x in targets}), len(targets))
        self.assertEqual({x["format"] for x in targets}, {"rpm", "deb", "arch"})
        policy = json.loads((ROOT / "effect/compat/kwin-versions.json").read_text())
        self.assertLessEqual(set(policy["runtimeTestedVersions"]), set(policy["compileTestedVersions"]))
        self.assertLessEqual(set(policy["compileTestedVersions"]), set(policy["sourceBuildVersions"]))

    def test_signing_requires_explicit_key_and_fingerprint(self):
        with tempfile.TemporaryDirectory() as d:
            for key, fingerprint in (("", "A" * 40), ("secret", "invalid")):
                with self.assertRaises(ValueError):
                    signing.sign(Path(d), key, fingerprint)

    def test_signing_refreshes_only_rpm_manifest_and_exports_public_key(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            p = root / "test.rpm"
            p.write_bytes(b"fixture")
            manifest = root / "test.rpm.json"
            manifest.write_text(json.dumps({"artifact": p.name}))
            public = b"-----BEGIN PGP PUBLIC KEY BLOCK-----\nfixture\n"
            with patch.object(signing.subprocess, "run") as run, \
                 patch.object(signing.subprocess, "check_output", return_value=public):
                signing.sign(root, "secret CI fixture", "A" * 40)
            self.assertEqual((root / "RPM-GPG-KEY-natural-desktop-slide.asc").read_bytes(), public)
            self.assertEqual(json.loads(manifest.read_text())["rpmSigningFingerprint"], "A" * 40)
            self.assertNotIn("secret CI fixture", "".join(x.read_text() for x in root.iterdir()))
            self.assertTrue(any(call.args[0][0] == "rpmsign" for call in run.call_args_list))
    def test_versions_and_changelog(self):
        version = re.search(r"project\([^\n]+ VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
        self.assertTrue(release.release_notes("v" + version).strip())
        for tag in (version, "v999.999.999", "v" + version + "-rc1", "v" + version + ";echo surprise"):
            with self.assertRaises(ValueError):
                release.release_notes(tag)

    def test_native_target_not_just_filename_label(self):
        self.assertEqual(package.target_profile("fedora-45-x86_64", {"ID": "fedora", "VERSION_ID": "45"}, "x86_64")["format"], "rpm")
        for distro, arch in (({"ID": "ubuntu", "VERSION_ID": "45"}, "x86_64"),
                             ({"ID": "fedora", "VERSION_ID": "44"}, "x86_64"),
                             ({"ID": "fedora", "VERSION_ID": "45"}, "aarch64")):
            with self.assertRaises(ValueError):
                package.target_profile("fedora-45-x86_64", distro, arch)

    def test_exact_distribution_package_dependency(self):
        for kind, responses, expected in (
            ("rpm", ["kwin 0:6.8.2-3.fc45"], ("kwin", "0:6.8.2-3.fc45")),
            ("deb", ["kwin-wayland: /usr/bin/kwin_wayland", "kwin-wayland 4:6.7.4-2+b1"], ("kwin-wayland", "4:6.7.4-2+b1")),
            ("arch", ["kwin", "kwin 6.8.2-2"], ("kwin", "6.8.2-2"))):
            with patch.object(package, "output", side_effect=responses):
                self.assertEqual(package.runtime_dependency(kind), expected)

    def test_malformed_native_dependency_rejected(self):
        with patch.object(package, "output", return_value="kwin bad%macro"):
            with self.assertRaises(ValueError):
                package.runtime_dependency("rpm")

    def test_arch_requires_exact_kwin_and_no_install_hook(self):
        text = package.arch_metadata(info(), ("kwin", "6.8.2-2"), 100, 1)
        self.assertIn("depend = kwin=6.8.2-2\n", text)
        self.assertIn("license = GPL-2.0-or-later", text)
        self.assertNotIn("install =", text)

    def test_only_production_release_builds(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "packaging").mkdir()
            (root / "CMakeCache.txt").write_text("BUILD_TESTING:BOOL=ON\nBUILD_DISTRIBUTION_PACKAGE:BOOL=ON\n")
            metadata = root / "packaging/build-info.json"
            metadata.write_text(json.dumps(info()))
            layout = {"integrationDriver": False, "kwinVersion": "6.8.2", "artifacts": [
                {"role": "renderer", "name": "kwin_natural_desktop_slide.so"},
                {"role": "controller", "name": "kwin_natural_desktop_slide_controller.so"},
                {"role": "settings", "name": "kwin_natural_desktop_slide_config.so"}]}
            (root / "install-layout-Release.json").write_text(json.dumps(layout))
            with patch.object(package, "output", return_value="kwin 6.8.2"):
                self.assertEqual(package.checked_build(root), info())
                for change in ({"integrationDriver": True}, {"rendererId": "kwin_natural_desktop_slide_devtest"}, {"kwinVersion": "6.6.0"}):
                    metadata.write_text(json.dumps(info() | change))
                    with self.assertRaises(ValueError):
                        package.checked_build(root)
                metadata.write_text(json.dumps(info()))
            with patch.object(package, "output", return_value="kwin 6.8.1"):
                with self.assertRaises(ValueError):
                    package.checked_build(root)

    def test_user_state_never_belongs_to_package(self):
        config = session.configuration(info(), "/home/test", {})
        self.assertEqual(config["stateFile"], "/home/test/.local/state/kwin-natural-desktop-slide/renderer-session.json")
        self.assertEqual(session.configuration(info(), "/home/test", {"XDG_STATE_HOME": "/state"})["stateFile"], "/state/kwin-natural-desktop-slide/renderer-session.json")
        with self.assertRaises(RuntimeError):
            session.configuration(info(), "/home/test", {"XDG_STATE_HOME": "relative"})

    def test_test_driver_and_foreign_ids_refused(self):
        for change in ({"integrationDriver": True}, {"controllerId": "foreign"}, {"kwinVersion": "unknown"}):
            with self.assertRaises(RuntimeError):
                session.configuration(info() | change, "/home/test", {})

    def test_activation_version_guard_and_recovery_after_update(self):
        def matching(*args, **kwargs): return "kwin 6.8.2\n"
        session.check_version(info(), matching)
        with self.assertRaises(RuntimeError):
            session.check_version(info(), lambda *a, **k: "kwin 6.8.3")
        # Version verification is deliberately only on start/enable, not stop.
        with patch.object(session.os, "geteuid", return_value=1000), \
             patch.object(session, "check_version", side_effect=RuntimeError("mismatch")), \
             patch.object(session.sys, "argv", ["session", "start"]), \
             tempfile.TemporaryDirectory() as d:
            p = Path(d) / "build-info.json"
            p.write_text(json.dumps(info()))
            self.assertEqual(session.main(p), 1)

    def test_old_source_service_requires_explicit_migration(self):
        with tempfile.TemporaryDirectory() as d:
            session.check_service_override(d, {})
            unit = Path(d) / ".config/systemd/user" / session.SERVICE
            unit.parent.mkdir(parents=True)
            unit.write_text("old source service")
            with self.assertRaisesRegex(RuntimeError, "overrides"):
                session.check_service_override(d, {})

    def test_recovery_does_not_require_matching_runtime(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "build-info.json"
            p.write_text(json.dumps(info()))
            for action in ("stop", "disable"):
                fake = Mock()
                fake.stop.return_value = "restored"
                with patch.object(session.os, "geteuid", return_value=1000), \
                     patch.object(session, "check_version", side_effect=AssertionError("must not check on recovery")), \
                     patch.object(session, "check_service_override"), \
                     patch.object(session, "installed_session", return_value=fake), \
                     patch.object(session.subprocess, "run") as run, \
                     patch.object(session.sys, "argv", ["session", action]):
                    self.assertEqual(session.main(p), 0)
                    fake.stop.assert_called_once()
                    self.assertEqual(run.call_count, int(action == "disable"))

    def test_explicit_enable_checks_runtime_and_service_activation(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "build-info.json"
            p.write_text(json.dumps(info()))
            with patch.object(session.os, "geteuid", return_value=1000), \
                 patch.dict(session.os.environ, {"XDG_SESSION_TYPE": "wayland"}), \
                 patch.object(session, "check_version") as version, \
                 patch.object(session, "check_service_override"), \
                 patch.object(session.subprocess, "run") as run, \
                 patch.object(session.sys, "argv", ["session", "enable"]):
                self.assertEqual(session.main(p), 0)
                version.assert_called_once()
                self.assertEqual([c.args[0][2] for c in run.call_args_list], ["daemon-reload", "enable", "is-active"])

    def test_complete_release_assets(self):
        target = {"target": "fedora-45-x86_64", "arch": "x86_64", "format": "rpm"}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            name = "kwin-natural-desktop-slide-0.1.0-fedora-45-x86_64-kwin-0_6.8.2-3.fc45.rpm"
            payload = b"test package fixture"
            digest = hashlib.sha256(payload).hexdigest()
            (root / name).write_bytes(payload)
            (root / (name + ".sha256")).write_text(f"{digest}  {name}\n")
            manifest = {"schemaVersion": 1, "projectVersion": "0.1.0", "target": target["target"],
                        "architecture": "x86_64", "artifact": name, "sha256": digest,
                        "kwinVersion": "6.8.2", "runtimePackage": "kwin", "runtimePackageVersion": "0:6.8.2-3.fc45"}
            (root / (name + ".json")).write_text(json.dumps(manifest))
            self.assertEqual(len(assets.verify(root, "0.1.0", [target])), 3)
            with self.assertRaisesRegex(ValueError, "Unsigned"):
                assets.verify(root, "0.1.0", [target], require_rpm_signature=True)
            for version, targets in (("0.2.0", [target]), ("0.1.0", [target, target | {"target": "missing"}])):
                with self.assertRaises(ValueError): assets.verify(root, version, targets)
            (root / name).write_bytes(b"corrupted")
            with self.assertRaisesRegex(ValueError, "checksum"):
                assets.verify(root, "0.1.0", [target])


if __name__ == "__main__":
    unittest.main()
