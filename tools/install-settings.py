#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Install this project's plugins atomically, without changing the running effect."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def atomic_write(destination, data, mode=0o755):
    """Rename a same-directory temporary file over the old path; never truncate it."""
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix="." + destination.name + "-", suffix=".tmp",
                                         dir=destination.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            os.fchmod(stream.fileno(), mode)
        os.replace(temporary, destination)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def install(artifacts):
    # Read/check everything first, before changing either file.
    pending = []
    for source, destination in artifacts:
        source, destination = Path(source), Path(destination)
        if destination.is_symlink() or (destination.exists() and not destination.is_file()):
            raise RuntimeError("Refusing non-regular plugin destination: " + str(destination))
        previous = destination.read_bytes() if destination.exists() else None
        mode = destination.stat().st_mode & 0o777 if previous is not None else 0o755
        pending.append((source.read_bytes(), destination, previous, mode))
    installed = []
    try:
        for data, destination, previous, mode in pending:
            atomic_write(destination, data)
            installed.append((destination, previous, mode))
    except Exception:
        for destination, previous, mode in reversed(installed):
            if previous is None:
                destination.unlink()
            else:
                atomic_write(destination, previous, mode)
        raise


ROLE_NAMES = {
    "renderer": r"kwin_natural_slide\.so",
    "settings": r"kwin_natural_slide_config\.so",
    "controller": r"kwin_natural_swipe_controller(?:_[a-zA-Z0-9]+)?\.so",
    "overscroll": r"kwin_natural_slide_preview[0-9]+\.so",
    "probe": r"kwin_natural_swipe_probe\.so",
}


def build_artifacts(build_dir, roles, config=None, destdir=None):
    layouts = sorted(Path(build_dir).glob("install-layout-*.json"))
    if config is not None:
        layouts = [path for path in layouts if path.name == f"install-layout-{config}.json"]
    if len(layouts) != 1:
        raise RuntimeError("Configure an effect build first; select --config if multiple install layouts exist.")
    layout = json.loads(layouts[0].read_text())
    if layout.get("schemaVersion") != 1 or layout.get("kwinVersion") != "6.8.2":
        raise RuntimeError("Invalid or unsupported build layout/version; reconfigure with this source tree.")
    plugins = Path(layout["pluginDirectory"])
    if not plugins.is_absolute() or ".." in plugins.parts or plugins.parts[-2:] != ("kwin", "effects"):
        raise RuntimeError("Invalid KDE plugin directory in build layout")
    if destdir is not None:
        staging = Path(destdir).resolve()
        if staging == Path("/"):
            raise RuntimeError("Refusing / as a staging root; use normal version-checked installation instead.")
        plugins = staging / plugins.relative_to(plugins.anchor)
    artifacts = {}
    for entry in layout["artifacts"]:
        role, name = entry["role"], entry["name"]
        if role not in ROLE_NAMES or not re.fullmatch(ROLE_NAMES[role], name) or role in artifacts:
            raise RuntimeError("Invalid plugin identity/role in build layout")
        source = Path(entry["source"])
        if not source.is_absolute() or source.name != name:
            raise RuntimeError("Invalid plugin source in build layout")
        artifacts[role] = (source, plugins / ("configs" if role == "settings" else "plugins") / name)
    missing = set(roles) - artifacts.keys()
    if missing:
        raise RuntimeError("Build does not include required components: " + ", ".join(sorted(missing)))
    return [artifacts[role] for role in roles]


def check_installed_kwin():
    executable = shutil.which("kwin_wayland")
    if not executable:
        raise RuntimeError("kwin_wayland is required to verify the installed version (no RPM dependency).")
    output = subprocess.check_output([executable, "--version"], text=True, timeout=15)
    match = re.fullmatch(r"kwin\s+(\d+\.\d+\.\d+)\s*", output.strip())
    if not match or match[1] != "6.8.2":
        raise RuntimeError("This build is pinned to KWin 6.8.2; installed version output: " + output.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--probe-only", action="store_true",
                        help="Install only the passive probe from build-observer; leave renderer/KCM unchanged.")
    mode.add_argument("--controller-only", action="store_true",
                      help="Install only the experimental owner from build-controller; leave renderer/KCM unchanged.")
    mode.add_argument("--overscroll-preview", action="store_true",
                      help="Install only the development overscroll renderer and controller; preserve normal renderer/KCM.")
    mode.add_argument("--settings-only", action="store_true",
                       help="Install only the settings module from build-effect; preserve all renderer/controller binaries.")
    mode.add_argument("--all", action="store_true", help="Install renderer, settings, controller and overscroll from one full build.")
    parser.add_argument("--build-dir", type=Path, help="CMake build directory (default follows the selected mode).")
    parser.add_argument("--config", help="Build configuration for multi-configuration generators.")
    parser.add_argument("--destdir", type=Path, help="Stage files for packaging; does not install into the live system or check its KWin version.")
    parser.add_argument("--dry-run", action="store_true", help="Validate and print installation paths without writing anything.")
    args = parser.parse_args()
    if os.geteuid() != 0 and args.destdir is None and not args.dry_run:
        raise RuntimeError("Run with pkexec or sudo after reviewing the installer.")
    if args.destdir is None:
        check_installed_kwin()
    root = Path(__file__).resolve().parents[1]
    if args.settings_only:
        roles = ["settings"]
    elif args.controller_only or args.overscroll_preview:
        roles = ["controller", "overscroll"] if args.overscroll_preview else ["controller"]
    elif args.probe_only:
        roles = ["probe"]
    elif args.all:
        roles = ["renderer", "settings", "controller", "overscroll"]
    else:
        roles = ["settings", "renderer"]
    default_build = "build-observer" if args.probe_only else "build-controller" if (args.controller_only or args.overscroll_preview or args.all) else "build-effect"
    artifacts = build_artifacts(args.build_dir or root / default_build, roles, args.config, args.destdir)
    if args.dry_run:
        for source, destination in artifacts:
            if not source.is_file():
                raise RuntimeError("Build the selected component first: " + str(source))
            print(f"{source} -> {destination}")
        return
    install(artifacts)
    restorecon = shutil.which("restorecon")
    if restorecon and args.destdir is None:
        subprocess.run([restorecon, *(str(destination) for _, destination in artifacts)], check=True)
    print("Staged project plugins for packaging." if args.destdir is not None
          else "Installed all project components from the selected build." if args.all
          else "Installed settings module only; renderer/controller binaries preserved." if args.settings_only
          else "Installed separate overscroll preview renderer and controller atomically." if args.overscroll_preview
          else "Installed experimental swipe controller atomically." if args.controller_only
          else "Installed passive swipe probe atomically." if args.probe_only
          else "Installed the configuration module and updated effect metadata atomically.")
    print("No KWin restart, effect toggles, or configuration writes performed.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
