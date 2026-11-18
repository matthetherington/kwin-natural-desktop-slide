# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Source compatibility is branch-based; binary compatibility remains exact."""
import json
from pathlib import Path
import re

POLICY_PATH = Path(__file__).resolve().parents[1] / "effect/compat/kwin-versions.json"


def source_version_supported(version, policy=None):
    if not isinstance(version, str) or not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        return False
    policy = json.loads(POLICY_PATH.read_text()) if policy is None else policy
    if policy.get("requireExactRuntimeMatch") is not True:
        raise ValueError("Exact KWin build/runtime matching must remain enabled")
    return version.rsplit(".", 1)[0] in policy["sourceBuildBranches"]
