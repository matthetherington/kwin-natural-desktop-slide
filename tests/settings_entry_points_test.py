#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare source-installer and CMake/package settings payloads in private staging roots."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile


def check(build, config):
    spec = importlib.util.spec_from_file_location(
        "install_settings", Path(__file__).resolve().parents[1] / "tools/install-settings.py")
    installer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(installer)
    with tempfile.TemporaryDirectory(prefix="nds-settings-entry-points-") as directory:
        root = Path(directory)
        source_stage, cmake_stage = root / "source", root / "cmake"
        artifacts = installer.build_artifacts(build, ["settings"], config=config, destdir=source_stage)
        installer.install(artifacts)
        subprocess.run(["cmake", "--install", str(build), "--config", config],
                       env=dict(os.environ, DESTDIR=str(cmake_stage)), check=True)
        assert len(artifacts) == 3, artifacts
        for _, destination in artifacts:
            counterpart = cmake_stage / destination.relative_to(source_stage)
            assert destination.read_bytes() == counterpart.read_bytes(), counterpart
            assert destination.stat().st_mode & 0o777 == counterpart.stat().st_mode & 0o777, counterpart
        desktop = next(destination for _, destination in artifacts if destination.suffix == ".desktop")
        assert "Exec=natural-desktop-slide-config\n" in desktop.read_text()
        assert "Name=Natural Desktop Slide Settings\n" in desktop.read_text()
        print("Source and CMake/package installs provide identical settings entry points.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", required=True)
    args = parser.parse_args()
    check(args.build_dir.resolve(), args.config)
