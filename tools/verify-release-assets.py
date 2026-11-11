#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Require exactly the declared matrix's packages, with matching hashes/versions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def verify(directory, version, targets, require_rpm_signature=False):
    expected = {item["target"]: item for item in targets}
    seen = set()
    allowed = set()
    for path in directory.glob("*.json"):
        info = json.loads(path.read_text())
        name = info.get("artifact", "")
        target = info.get("target")
        if (info.get("schemaVersion") != 1 or info.get("projectVersion") != version
                or target not in expected or target in seen
                or info.get("architecture") != expected[target]["arch"]
                or not re.fullmatch(r"kwin-natural-desktop-slide-[A-Za-z0-9.+_-]+\.(rpm|deb|pkg\.tar\.zst)", name)
                or path.name != name + ".json"):
            raise ValueError("Invalid/duplicate/unexpected release manifest: " + path.name)
        suffix = {"rpm": ".rpm", "deb": ".deb", "arch": ".pkg.tar.zst"}[expected[target]["format"]]
        if not name.endswith(suffix) or not name.startswith(f"kwin-natural-desktop-slide-{version}-{target}-kwin-"):
            raise ValueError("Package format mismatch")
        policy = json.loads((ROOT / "effect/compat/kwin-versions.json").read_text())
        if (info.get("kwinVersion") not in policy["sourceBuildVersions"]
                or not re.fullmatch(r"[a-z0-9][a-z0-9+.-]*", info.get("runtimePackage", ""))
                or not re.fullmatch(r"[A-Za-z0-9.+:~_-]+", info.get("runtimePackageVersion", ""))):
            raise ValueError("Missing/invalid exact runtime dependency")
        artifact = directory / name
        digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
        checksum = artifact.with_name(name + ".sha256")
        if info.get("sha256") != digest or checksum.read_text() != f"{digest}  {name}\n":
            raise ValueError("Release checksum mismatch: " + name)
        if require_rpm_signature and name.endswith(".rpm"):
            if not re.fullmatch(r"[0-9A-F]{40}|[0-9A-F]{64}", info.get("rpmSigningFingerprint", "")):
                raise ValueError("Unsigned release RPM")
            signature = subprocess.check_output(["rpm", "-qp", "--qf",
                "%{RSAHEADER:pgpsig}\n%{DSAHEADER:pgpsig}\n%{SIGPGP:pgpsig}\n%{SIGGPG:pgpsig}\n",
                str(artifact)], text=True)
            if "Key ID" not in signature:
                raise ValueError("RPM has no embedded OpenPGP signature")
            subprocess.run(["rpmkeys", "--checksig", str(artifact)], check=True)
        seen.add(target)
        allowed.update((name, name + ".sha256", path.name))
    if seen != set(expected):
        raise ValueError("Missing package targets: " + ", ".join(sorted(set(expected) - seen)))
    public_key = directory / "RPM-GPG-KEY-natural-desktop-slide.asc"
    if public_key.exists():
        public = public_key.read_text()
        if not public.startswith("-----BEGIN PGP PUBLIC KEY BLOCK-----") or "PRIVATE KEY" in public:
            raise ValueError("Invalid release public key")
        allowed.add(public_key.name)
    elif require_rpm_signature:
        raise ValueError("Missing RPM release public key")
    if {p.name for p in directory.iterdir()} != allowed:
        raise ValueError("Unexpected files in release assets")
    return sorted(allowed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--require-rpm-signature", action="store_true")
    args = parser.parse_args()
    print("\n".join(verify(args.directory, args.version,
                         json.loads((ROOT / "packaging/targets.json").read_text()), args.require_rpm_signature)))
