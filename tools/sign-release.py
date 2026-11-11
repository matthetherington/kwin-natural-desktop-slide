#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Sign tested RPM payloads only in the privileged, tag-only release job."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


@contextmanager
def signing_environment():
    with tempfile.TemporaryDirectory(prefix="nds-signing-") as temporary:
        home = Path(temporary)
        home.chmod(0o700)
        env = dict(os.environ, GNUPGHOME=str(home))
        try:
            yield env
        finally:
            subprocess.run(["gpgconf", "--kill", "gpg-agent"], env=env, check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def sign(directory, key, fingerprint):
    if not key or not re.fullmatch(r"[0-9A-Fa-f]{40}|[0-9A-Fa-f]{64}", fingerprint):
        raise ValueError("Set the RPM_SIGNING_KEY secret and RPM_SIGNING_FINGERPRINT repository variable before publishing")
    with signing_environment() as env:
        # A dedicated, unencrypted CI signing subkey is stored encrypted at rest
        # in GitHub Secrets. Never write its bytes into the workspace/artifacts.
        subprocess.run(["gpg", "--batch", "--import"], input=key, text=True, env=env, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        public = subprocess.check_output(["gpg", "--batch", "--armor", "--export", fingerprint], env=env)
        if not public.startswith(b"-----BEGIN PGP PUBLIC KEY BLOCK-----"):
            raise ValueError("Signing key fingerprint does not identify an imported public key")
        public_path = directory / "RPM-GPG-KEY-natural-desktop-slide.asc"
        public_path.write_bytes(public)
        subprocess.run(["rpm", "--import", str(public_path)], check=True)
        for artifact in sorted(directory.glob("*.rpm")):
            subprocess.run(["rpmsign", "--define", "__gpg /usr/bin/gpg", "--define", "_gpg_name " + fingerprint,
                            "--addsign", str(artifact)], env=env, check=True, timeout=60)
            subprocess.run(["rpmkeys", "--checksig", str(artifact)], check=True)
            digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
            artifact.with_name(artifact.name + ".sha256").write_text(f"{digest}  {artifact.name}\n")
            manifest = artifact.with_name(artifact.name + ".json")
            info = json.loads(manifest.read_text())
            info.update(sha256=digest, rpmSigningFingerprint=fingerprint.upper())
            manifest.write_text(json.dumps(info, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    sign(args.directory, os.environ.get("RPM_SIGNING_KEY", ""), os.environ.get("RPM_SIGNING_FINGERPRINT", ""))
