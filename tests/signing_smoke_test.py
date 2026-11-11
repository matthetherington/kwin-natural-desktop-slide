#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Optional disposable-container test with an ephemeral key, never a release key."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def check(artifact):
    tool = Path(__file__).resolve().parents[1] / "tools/sign-release.py"
    spec = importlib.util.spec_from_file_location("nds_signing_smoke", tool)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="nds-signing-smoke-") as directory:
        root = Path(directory)
        keyring = root / "keyring"
        keyring.mkdir(mode=0o700)
        fixture = root / "assets"
        fixture.mkdir()
        target = fixture / artifact.name
        for suffix in ("", ".json", ".sha256"):
            shutil.copyfile(artifact.with_name(artifact.name + suffix), fixture / (artifact.name + suffix))
        env = dict(os.environ, GNUPGHOME=str(keyring))
        try:
            subprocess.run(["gpg", "--batch", "--pinentry-mode", "loopback", "--passphrase", "",
                            "--quick-generate-key", "Natural Desktop Slide temporary CI fixture", "rsa2048", "sign", "1d"],
                           env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            listing = subprocess.check_output(["gpg", "--batch", "--with-colons", "--list-secret-keys"], env=env, text=True)
            fingerprint = next(line.split(":")[9] for line in listing.splitlines() if line.startswith("fpr:"))
            secret = subprocess.check_output(["gpg", "--batch", "--armor", "--export-secret-keys", fingerprint], env=env, text=True)
            def payload():
                return subprocess.check_output(["rpm", "-qp", "--qf", "%{PAYLOADDIGEST}", str(target)], text=True)
            before = payload()
            module.sign(fixture, secret, fingerprint)
            assert payload() == before, "Signing changed the tested RPM payload"
            manifest = json.loads(target.with_name(target.name + ".json").read_text())
            assert manifest["rpmSigningFingerprint"] == fingerprint
            verifier = importlib.util.spec_from_file_location("nds_signed_assets_smoke", tool.with_name("verify-release-assets.py"))
            verify_module = importlib.util.module_from_spec(verifier)
            verifier.loader.exec_module(verify_module)
            verify_module.verify(fixture, manifest["projectVersion"],
                [{"target": manifest["target"], "arch": manifest["architecture"], "format": "rpm"}], True)
            print("RPM signing and signature verification pass; tested payload unchanged. Ephemeral fixture only.")
        finally:
            subprocess.run(["gpgconf", "--kill", "gpg-agent"], env=env, check=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=Path)
    args = parser.parse_args()
    check(args.artifact.resolve())
