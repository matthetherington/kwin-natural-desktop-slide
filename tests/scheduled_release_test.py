#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise release preparation and atomic pushes using private local git repos."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nds_scheduled_release", ROOT / "tools/scheduled-release.py")
scheduled = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduled)


class ScheduledReleaseTest(unittest.TestCase):
    def git(self, root, *args):
        return scheduled.git(root, *args)

    def checkout(self, directory):
        root, remote = Path(directory) / "checkout", Path(directory) / "remote.git"
        subprocess.run(["git", "init", "--quiet", "--bare", "--initial-branch=main", str(remote)], check=True)
        subprocess.run(["git", "init", "--quiet", "--initial-branch=main", str(root)], check=True)
        self.git(root, "config", "user.name", "Release test fixture")
        self.git(root, "config", "user.email", "fixture@example.invalid")
        self.git(root, "config", "commit.gpgsign", "false")
        self.git(root, "config", "tag.gpgsign", "false")
        for name in scheduled.RELEASE_FILES:
            destination = root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        (root / "CHANGELOG.md").write_text(
            "# Changelog\n\n## " + scheduled.project_version(root) + "\n\n- Fixture release.\n")
        (root / "source.cpp").write_text("// Fixture, never a compositor plugin.\n")
        self.git(root, "add", ".")
        self.git(root, "commit", "--quiet", "-m", "Fixture base")
        self.git(root, "remote", "add", "origin", str(remote))
        self.git(root, "push", "--quiet", "origin", "HEAD:main")
        return root, remote, self.git(root, "rev-parse", "HEAD")

    def test_next_patch_uses_all_reserved_tags_not_lexicographic_order(self):
        self.assertEqual(scheduled.next_version("0.1.2", ["v0.1.9", "v0.1.10", "v0.1.11-rc1", "unrelated"]), "0.1.11")
        self.assertEqual(scheduled.next_version("0.1.12", ["v0.1.2"]), "0.1.13")
        self.assertEqual(scheduled.next_version("0.2.0", ["v0.1.99"]), "0.2.1")
        for version in ("0.1", "0.1.2-rc1", "0.01.2", "0.1.2;false"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                scheduled.next_version(version, [])
        with self.assertRaisesRegex(ValueError, "newer release line"):
            scheduled.next_version("0.1.2", ["v0.2.0"])

    def test_snapshot_patch_is_identical_in_every_consumer_and_preserves_history(self):
        with tempfile.TemporaryDirectory() as directory:
            root, remote, base = self.checkout(directory)
            previous = (root / "CHANGELOG.md").read_text()
            version, notes, patch = scheduled.prepare(root, [])
            self.assertIn("Automated native package rebuild", notes)
            self.assertIn(previous[len("# Changelog\n"):].lstrip(), (root / "CHANGELOG.md").read_text())
            for name in scheduled.PLUGIN_FILES:
                self.assertEqual(json.loads((root / name).read_text())["KPlugin"]["Version"], version)
            self.assertEqual(set(self.git(root, "diff", "--name-only").splitlines()), set(scheduled.RELEASE_FILES))
            consumer = Path(directory) / "consumer"
            subprocess.run(["git", "clone", "--quiet", str(remote), str(consumer)], check=True)
            subprocess.run(["git", "-C", str(consumer), "apply", "--check", "-"],
                           input=patch, text=True, check=True)
            subprocess.run(["git", "-C", str(consumer), "apply", "-"], input=patch, text=True, check=True)
            for name in scheduled.RELEASE_FILES:
                self.assertEqual((consumer / name).read_bytes(), (root / name).read_bytes())
            self.assertEqual(self.git(remote, "rev-parse", "main"), base)  # Preparation never publishes.

    def test_unreleased_notes_are_included_and_other_metadata_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.checkout(directory)
            changelog = root / "CHANGELOG.md"
            changelog.write_text(changelog.read_text().replace("# Changelog\n", "# Changelog\n\n## Unreleased\n\n- A pending fix.\n", 1))
            self.git(root, "add", "CHANGELOG.md")
            self.git(root, "commit", "--quiet", "-m", "Pending notes")
            metadata = json.loads((root / scheduled.PLUGIN_FILES[0]).read_text())
            version, notes, _ = scheduled.prepare(root, [])
            self.assertIn("A pending fix.", notes)
            self.assertNotIn("## Unreleased", changelog.read_text())
            actual = json.loads((root / scheduled.PLUGIN_FILES[0]).read_text())
            metadata["KPlugin"]["Version"] = version
            self.assertEqual(actual, metadata)

    def test_prepare_refuses_dirty_or_inconsistent_sources_before_editing(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.checkout(directory)
            cmake = (root / "CMakeLists.txt").read_bytes()
            (root / "source.cpp").write_text("// Unsaved fixture edit.\n")
            with self.assertRaisesRegex(ValueError, "clean"):
                scheduled.prepare(root, [])
            self.assertEqual((root / "CMakeLists.txt").read_bytes(), cmake)
            self.git(root, "add", "source.cpp")
            self.git(root, "commit", "--quiet", "-m", "Source edit")
            plugin = root / scheduled.PLUGIN_FILES[0]
            data = json.loads(plugin.read_text())
            data["KPlugin"]["Version"] = "9.9.9"
            plugin.write_text(json.dumps(data))
            self.git(root, "add", str(plugin))
            self.git(root, "commit", "--quiet", "-m", "Inconsistent fixture")
            with self.assertRaisesRegex(ValueError, "Plugin version"):
                scheduled.prepare(root, [])
            self.assertEqual((root / "CMakeLists.txt").read_bytes(), cmake)

    def test_publish_pushes_exact_snapshot_and_annotated_tag_atomically(self):
        with tempfile.TemporaryDirectory() as directory:
            root, remote, base = self.checkout(directory)
            version, _, patch = scheduled.prepare(root, [])
            tag = "v" + version
            expected = {name: (root / name).read_bytes() for name in scheduled.RELEASE_FILES}
            scheduled.publish_snapshot(root, tag, base, "main")
            commit = self.git(root, "rev-parse", "HEAD")
            self.assertNotEqual(commit, base)
            self.assertEqual(self.git(remote, "rev-parse", "main"), commit)
            self.assertEqual(self.git(remote, "rev-parse", tag + "^{}"), commit)
            self.assertEqual(self.git(remote, "cat-file", "-t", tag), "tag")
            for name, data in expected.items():
                result = subprocess.check_output(["git", "-C", str(remote), "show", f"{tag}:{name}"])
                self.assertEqual(result, data)
            # Retry after a push succeeded but GitHub publication failed.
            retry = Path(directory) / "retry"
            subprocess.run(["git", "clone", "--quiet", str(remote), str(retry)], check=True)
            self.git(retry, "checkout", "--quiet", "--detach", base)
            subprocess.run(["git", "-C", str(retry), "apply", "-"], input=patch, text=True, check=True)
            scheduled.publish_snapshot(retry, tag, base, "main")
            self.assertEqual(self.git(remote, "rev-parse", "main"), commit)

    def test_advanced_default_branch_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root, remote, base = self.checkout(directory)
            version, _, _ = scheduled.prepare(root, [])
            self.git(remote, "config", "user.name", "Concurrent fixture")
            self.git(remote, "config", "user.email", "fixture@example.invalid")
            tree = self.git(remote, "rev-parse", "main^{tree}")
            concurrent = self.git(remote, "commit-tree", tree, "-p", base, "-m", "Concurrent source")
            self.git(remote, "update-ref", "refs/heads/main", concurrent)
            with self.assertRaisesRegex(ValueError, "advanced"):
                scheduled.publish_snapshot(root, "v" + version, base, "main")
            self.assertEqual(self.git(remote, "rev-parse", "main"), concurrent)
            self.assertEqual(self.git(remote, "tag", "--list", "v" + version), "")

    def test_extra_changes_and_existing_different_tag_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root, remote, base = self.checkout(directory)
            version, _, _ = scheduled.prepare(root, [])
            (root / "source.cpp").write_text("// Untested change.\n")
            with self.assertRaisesRegex(ValueError, "only the prepared"):
                scheduled.publish_snapshot(root, "v" + version, base, "main")
            (root / "source.cpp").write_text("// Fixture, never a compositor plugin.\n")
            self.git(remote, "tag", "v" + version, base)
            with self.assertRaisesRegex(ValueError, "different snapshot"):
                scheduled.publish_snapshot(root, "v" + version, base, "main")
            self.assertEqual(self.git(remote, "rev-parse", "main"), base)
            self.assertEqual(self.git(remote, "rev-parse", "v" + version), base)

    def test_push_race_cannot_publish_a_tag_without_the_source_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root, remote, base = self.checkout(directory)
            version, _, _ = scheduled.prepare(root, [])
            original_git = scheduled.git
            original_git(remote, "config", "user.name", "Concurrent fixture")
            original_git(remote, "config", "user.email", "fixture@example.invalid")
            tree = original_git(remote, "rev-parse", "main^{tree}")
            concurrent = original_git(remote, "commit-tree", tree, "-p", base, "-m", "Racing source")

            def racing_git(checkout, *args):
                if args[0] == "push":
                    original_git(remote, "update-ref", "refs/heads/main", concurrent)
                return original_git(checkout, *args)

            with patch.object(scheduled, "git", side_effect=racing_git):
                with self.assertRaises(subprocess.CalledProcessError):
                    scheduled.publish_snapshot(root, "v" + version, base, "main")
            self.assertEqual(original_git(remote, "rev-parse", "main"), concurrent)
            self.assertEqual(original_git(remote, "tag", "--list", "v" + version), "")


if __name__ == "__main__":
    unittest.main()
