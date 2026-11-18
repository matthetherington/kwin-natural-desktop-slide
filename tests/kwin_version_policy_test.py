#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the same source-branch policy in Python and real CMake script mode."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from kwin_version_policy import source_version_supported


class KWinVersionPolicyTest(unittest.TestCase):
    def test_patch_releases_not_minor_branches(self):
        for version in ("6.7.0", "6.7.99", "6.8.2", "6.8.3", "6.8.999"):
            with self.subTest(version=version):
                self.assertTrue(source_version_supported(version))
        for version in ("6.6.99", "6.9.0", "7.0.0", "6.8", "6.8.3.1",
                        "6.8.3-rc1", "6.08.3", "6.8.3\n", "", None):
            with self.subTest(version=version):
                self.assertFalse(source_version_supported(version))

    def test_review_and_validation_lists_remain_historical_not_auto_promoted(self):
        policy = json.loads((ROOT / "effect/compat/kwin-versions.json").read_text())
        for version in policy["sourceBuildVersions"]:
            self.assertTrue(source_version_supported(version))
        self.assertNotIn("6.8.999", policy["sourceBuildVersions"])
        self.assertNotIn("6.8.999", policy["compileTestedVersions"])
        self.assertNotIn("6.8.999", policy["runtimeTestedVersions"])
        with self.assertRaisesRegex(ValueError, "Exact"):
            source_version_supported("6.8.3", policy | {"requireExactRuntimeMatch": False})

    def test_cmake_and_python_policy_agree(self):
        versions = ("6.7.0", "6.7.99", "6.8.999", "6.6.99", "6.9.0",
                    "7.0.0", "6.8", "6.8.3.1", "6.8.4-rc1", "6.08.4", "6.8.3\n", "")
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "check.cmake"
            script.write_text(
                'cmake_minimum_required(VERSION 3.25)\n'
                f'include("{ROOT}/effect/compat/source_version.cmake")\n'
                'nds_kwin_source_supported("${VERSION}" accepted)\n'
                'file(WRITE "${RESULT}" "${accepted}")\n')
            result = Path(directory) / "result"
            for version in versions:
                subprocess.run(["cmake", "-DVERSION=" + version, "-DRESULT=" + str(result),
                                "-P", str(script)], check=True, capture_output=True)
                with self.subTest(version=version):
                    self.assertEqual(result.read_text() == "TRUE", source_version_supported(version))


if __name__ == "__main__":
    unittest.main()
