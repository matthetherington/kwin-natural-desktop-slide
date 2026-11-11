#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stage a package into a private directory; never install into the host root."""
import argparse
import ast
import json
import os
from pathlib import Path
import subprocess
import tempfile


def check(build):
    layout = json.loads(next(build.glob("install-layout-*.json")).read_text())
    plugins = Path(layout["pluginDirectory"]).relative_to("/")
    with tempfile.TemporaryDirectory(prefix="nds-payload-") as directory:
        stage = Path(directory)
        subprocess.run(["cmake", "--install", str(build)],
                       env=dict(os.environ, DESTDIR=directory), check=True)
        helpers = list(stage.glob("usr/**/kwin-natural-desktop-slide/package_session.py"))
        assert len(helpers) == 1, helpers
        libexec = helpers[0].parent.relative_to(stage)
        expected = {
            "usr/bin/natural-desktop-slide-config", "usr/bin/natural-desktop-slide-diagnostic",
            "usr/bin/natural-desktop-slide-session",
            "usr/share/kwin-natural-desktop-slide/build-info.json",
            "usr/share/applications/natural-desktop-slide.desktop",
            "usr/lib/systemd/user/natural-desktop-slide-session.service",
            "usr/share/licenses/kwin-natural-desktop-slide/LICENSE",
            "usr/share/doc/kwin-natural-desktop-slide/README.md",
            "usr/share/doc/kwin-natural-desktop-slide/BINARY_PACKAGES.md",
            str(plugins / "plugins/kwin_natural_desktop_slide.so"),
            str(plugins / "plugins/kwin_natural_desktop_slide_controller.so"),
            str(plugins / "configs/kwin_natural_desktop_slide_config.so"),
            *(str(libexec / name) for name in ("package_session.py", "effect-session.py", "startup-session.py")),
        }
        entries = list(stage.rglob("*"))
        assert not any(p.is_symlink() for p in entries), "Unexpected package symlink"
        actual = {str(p.relative_to(stage)) for p in entries if p.is_file()}
        assert actual == expected, ("Unexpected", actual - expected, "Missing", expected - actual)
        metadata = json.loads((stage / "usr/share/kwin-natural-desktop-slide/build-info.json").read_text())
        assert metadata["kwinVersion"] == layout["kwinVersion"] and metadata["integrationDriver"] is False
        launcher = stage / "usr/bin/natural-desktop-slide-session"
        ast.parse(launcher.read_text())
        assert os.access(launcher, os.X_OK), "Session launcher is not executable"
        assert str(Path("/") / libexec) in launcher.read_text()
        service = (stage / "usr/lib/systemd/user/natural-desktop-slide-session.service").read_text()
        assert "ExecStart=/usr/bin/natural-desktop-slide-session start" in service
        assert "ExecStop=/usr/bin/natural-desktop-slide-session stop" in service
        assert "WantedBy=graphical-session.target" in service
        assert "--now" not in service and "sudo" not in service
        print("Verified exact package payload, production metadata, launcher and opt-in service.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    check(args.build_dir.resolve())
