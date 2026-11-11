# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Validate release metadata without loading any plugins."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
URL = "https://github.com/matthetherington/kwin-virtual-desktop-natural-swipe"


class PackageMetadataTest(unittest.TestCase):
    def test_plugin_metadata(self):
        version = re.search(r"project\([^\n]+ VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
        identities = set()
        for path in [ROOT / "effect/metadata.json", ROOT / "effect/controller_metadata.json",
                     ROOT / "effect/probe_metadata.json", ROOT / "kcm/metadata.json"]:
            with self.subTest(path=path):
                plugin = json.loads(path.read_text())["KPlugin"]
                self.assertEqual(plugin["Authors"], [{"Name": "Matt Hetherington"}])
                self.assertEqual(plugin["License"], "GPL-2.0-or-later")
                self.assertEqual(plugin["Version"], version)
                self.assertEqual(plugin["Website"], URL)
                self.assertEqual(plugin["BugReportUrl"], URL + "/issues")
                for field in ("Id", "Name", "Description", "Icon"):
                    self.assertTrue(plugin[field])
                self.assertNotIn(plugin["Id"], identities)
                identities.add(plugin["Id"])
                if path.parent.name == "effect":
                    self.assertIs(plugin["EnabledByDefault"], False)


if __name__ == "__main__":
    unittest.main()
