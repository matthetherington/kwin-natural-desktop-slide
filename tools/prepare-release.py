#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check tag/project/plugin versions and extract the matching release notes."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def release_notes(tag, root=ROOT):
    match = re.search(r"project\([^\n]+ VERSION (\d+\.\d+\.\d+)", (root / "CMakeLists.txt").read_text())
    if not match or tag != "v" + match[1]:
        raise ValueError("Release tag must match the CMake project version")
    version = match[1]
    for name in ("effect/metadata.json", "effect/controller_metadata.json", "effect/probe_metadata.json", "kcm/metadata.json"):
        if json.loads((root / name).read_text())["KPlugin"]["Version"] != version:
            raise ValueError("Plugin version differs from release: " + name)
    changelog = (root / "CHANGELOG.md").read_text()
    sections = re.split(r"^## ", changelog, flags=re.MULTILINE)
    for section in sections[1:]:
        heading, _, body = section.partition("\n")
        if heading.strip() == version and body.strip():
            return body.strip() + "\n"
    raise ValueError("Missing/nonempty changelog section required for " + version)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        args.output.write_text(release_notes(args.tag))
    except ValueError as error:
        parser.error(str(error))
