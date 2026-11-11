#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit user activation for native packages; never called by package scripts."""
import argparse
import importlib.util
import json
import os
import re
from pathlib import Path
import subprocess
import sys

SERVICE = "natural-desktop-slide-session.service"


def configuration(metadata, home=None, environment=None):
    env = os.environ if environment is None else environment
    home = Path.home() if home is None else Path(home)
    if (metadata.get("schemaVersion") != 1 or metadata.get("integrationDriver") is not False
            or metadata.get("rendererId") != "kwin_natural_desktop_slide"
            or metadata.get("controllerId") != "kwin_natural_desktop_slide_controller"
            or not re.fullmatch(r"\d+\.\d+\.\d+", metadata.get("kwinVersion", ""))):
        raise RuntimeError("Invalid installed production build metadata")
    state = Path(env.get("XDG_STATE_HOME") or home / ".local/state")
    if not state.is_absolute():
        raise RuntimeError("XDG_STATE_HOME must be absolute")
    return {"schemaVersion": 1, "rendererId": metadata["rendererId"],
            "controllerId": metadata["controllerId"],
            "stateFile": str(state / "kwin-natural-desktop-slide/renderer-session.json")}


def check_version(metadata, runner=subprocess.check_output):
    actual = runner(["kwin_wayland", "--version"], text=True, timeout=15).strip()
    if actual != "kwin " + metadata["kwinVersion"]:
        raise RuntimeError("Installed package requires KWin " + metadata["kwinVersion"]
                           + "; found " + actual + ". Install a matching package; do not bypass dependencies.")


def check_service_override(home=None, environment=None):
    env = os.environ if environment is None else environment
    home = Path.home() if home is None else Path(home)
    config = Path(env.get("XDG_CONFIG_HOME") or home / ".config")
    if not config.is_absolute():
        raise RuntimeError("XDG_CONFIG_HOME must be absolute")
    unit = config / "systemd/user" / SERVICE
    if unit.exists() or unit.is_symlink():
        raise RuntimeError("A source-installed user service overrides the package: " + str(unit)
                           + ". Stop it and migrate using docs/BINARY_PACKAGES.md first.")


def installed_session(config):
    path = Path(__file__).with_name("startup-session.py")
    spec = importlib.util.spec_from_file_location("nds_installed_startup", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.StartupSession(module.create_effects(config), config)


def main(metadata_path):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "stop", "status", "enable", "disable"))
    args = parser.parse_args()
    try:
        if os.geteuid() == 0:
            raise RuntimeError("Run as your desktop user, not with sudo")
        metadata = json.loads(Path(metadata_path).read_text())
        config = configuration(metadata)
        if args.action in ("start", "enable"):
            check_version(metadata)
            if os.environ.get("XDG_SESSION_TYPE") != "wayland":
                raise RuntimeError("Activation requires a Plasma Wayland session")
        if args.action in ("enable", "disable"):
            check_service_override()
            if args.action == "enable":
                subprocess.run(["systemctl", "--user", "daemon-reload"], check=True, timeout=30)
            subprocess.run(["systemctl", "--user", args.action, "--now", SERVICE], check=True, timeout=120)
            if args.action == "enable":
                subprocess.run(["systemctl", "--user", "is-active", "--quiet", SERVICE], check=True, timeout=15)
            if args.action == "disable":
                # Also recover a session started manually while the unit was
                # inactive; systemctl alone would not run ExecStop in that case.
                print(installed_session(config).stop())
        else:
            print(getattr(installed_session(config), args.action)())
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("Natural Desktop Slide: " + str(error), file=sys.stderr)
        return 1
