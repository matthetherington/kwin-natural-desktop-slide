#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build native release assets in a matching disposable distribution environment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
NAME = "kwin-natural-desktop-slide"


def output(*args):
    return subprocess.check_output(args, text=True, timeout=30).strip()


def target_profile(name, distro=None, machine=None):
    targets = json.loads((ROOT / "packaging/targets.json").read_text())
    target = next((item for item in targets if item["target"] == name), None)
    if target is None:
        raise ValueError("Unknown package target")
    distro = platform.freedesktop_os_release() if distro is None else distro
    machine = platform.machine() if machine is None else machine
    if (target["distro"] != distro.get("ID") or target["arch"] != machine
            or (target["release"] and target["release"] != distro.get("VERSION_ID"))):
        raise ValueError("Package target does not match the native distribution/architecture")
    return target


def checked_build(build):
    cache = (build / "CMakeCache.txt").read_text()
    if "BUILD_TESTING:BOOL=ON" not in cache or "BUILD_DISTRIBUTION_PACKAGE:BOOL=ON" not in cache:
        raise ValueError("Packaging requires tests and distribution integration enabled")
    info = json.loads((build / "packaging/build-info.json").read_text())
    layout = json.loads((build / "install-layout-Release.json").read_text())
    policy = json.loads((ROOT / "effect/compat/kwin-versions.json").read_text())
    if (info.get("schemaVersion") != 1 or info.get("integrationDriver") is not False
            or layout.get("integrationDriver") is not False
            or info.get("kwinVersion") not in policy["sourceBuildVersions"]
            or info.get("kwinVersion") != layout.get("kwinVersion")
            or info.get("rendererId") != "kwin_natural_desktop_slide"
            or info.get("controllerId") != "kwin_natural_desktop_slide_controller"
            or not re.fullmatch(r"\d+\.\d+\.\d+", info.get("version", ""))):
        raise ValueError("Only a reviewed full production Release build may be packaged")
    expected = {"renderer": "kwin_natural_desktop_slide.so", "settings": "kwin_natural_desktop_slide_config.so",
                "controller": "kwin_natural_desktop_slide_controller.so"}
    if len(layout["artifacts"]) != 3 or {item["role"]: item["name"] for item in layout["artifacts"]} != expected:
        raise ValueError("Unexpected/missing package plugin identities")
    if output("kwin_wayland", "--version") != "kwin " + info["kwinVersion"]:
        raise ValueError("Build/runtime KWin mismatch in packaging environment")
    return info


def runtime_dependency(kind):
    # Depend on the exact distro package release, not just upstream's ABI label.
    if kind == "rpm":
        name, version = output("rpm", "-qf", "--qf", "%{NAME} %{EPOCHNUM}:%{VERSION}-%{RELEASE}",
                               "/usr/bin/kwin_wayland").split()
    elif kind == "deb":
        owner = output("dpkg-query", "-S", "/usr/bin/kwin_wayland").split(": /", 1)[0]
        name, version = output("dpkg-query", "-W", "-f=${Package} ${Version}", owner).split()
    else:
        name, version = output("pacman", "-Q", output("pacman", "-Qoq", "/usr/bin/kwin_wayland")).split()
    if not re.fullmatch(r"[a-z0-9][a-z0-9+.-]*", name) or not re.fullmatch(r"[A-Za-z0-9.+:~_-]+", version):
        raise ValueError("Invalid runtime package identity")
    return name, version


def arch_metadata(info, dependency, size, epoch):
    name, version = dependency
    return (f"pkgname = {NAME}\npkgbase = {NAME}\npkgver = {info['version']}-1\n"
            f"pkgdesc = Continuous desktop swipes with selectively sliding Plasma panels\n"
            f"url = https://github.com/matthetherington/{NAME}\nbuilddate = {epoch}\n"
            f"packager = Matt Hetherington\nsize = {size}\narch = x86_64\n"
            f"license = GPL-2.0-or-later\ndepend = {name}={version}\n"
            "depend = python\ndepend = qt6-tools\n")


def build_package(build, destination, target):
    info = checked_build(build)
    subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"], check=True)
    dependency = runtime_dependency(target["format"])
    # Include the distro's package release in the asset name, avoiding collisions
    # when downstream patches change without changing upstream's version string.
    runtime_label = re.sub(r"[^A-Za-z0-9.+_-]", "_", dependency[1])
    stem = f"{NAME}-{info['version']}-{target['target']}-kwin-{runtime_label}"
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nds-package-") as temporary:
        work = Path(temporary)
        kind = target["format"]
        if kind in ("rpm", "deb"):
            name, version = dependency
            requires = f"{name} = {version}, python3, qt6-qttools" if kind == "rpm" else f"{name} (= {version}), python3, qdbus-qt6"
            variable = "CPACK_RPM_PACKAGE_REQUIRES" if kind == "rpm" else "CPACK_DEBIAN_PACKAGE_DEPENDS"
            command = ["cpack", "--config", str(build / "CPackConfig.cmake"), "-G", kind.upper(),
                       "-B", str(work), "-D", f"{variable}={requires}",
                       "-D", f"CPACK_PACKAGE_FILE_NAME={stem}"]
            # DEB-DEFAULT otherwise hides the target/KWin build from the filename.
            if kind == "deb":
                command += ["-D", f"CPACK_DEBIAN_FILE_NAME={stem}.deb"]
            else:
                command += ["-D", f"CPACK_RPM_FILE_NAME={stem}.rpm"]
            subprocess.run(command, check=True)
            files = list(work.glob("*." + kind))
            if len(files) != 1:
                raise ValueError("Expected exactly one native package")
            artifact = destination / files[0].name
            artifact.write_bytes(files[0].read_bytes())
        else:
            stage = work / "stage"
            subprocess.run(["cmake", "--install", str(build)], env=dict(os.environ, DESTDIR=str(stage)), check=True)
            files = list(stage.rglob("*"))
            if any(item.is_symlink() for item in files):
                raise ValueError("Unexpected symlink in project package payload")
            size = sum(item.stat().st_size for item in files if item.is_file())
            epoch = int(os.environ.get("SOURCE_DATE_EPOCH", time.time()))
            (stage / ".PKGINFO").write_text(arch_metadata(info, dependency, size, epoch))
            artifact = destination / (stem + ".pkg.tar.zst")
            subprocess.run(["bsdtar", "--zstd", "-cf", str(artifact), "-C", str(stage), ".PKGINFO", "usr"], check=True)
            subprocess.run(["pacman", "-Qip", str(artifact)], check=True)
    digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
    artifact.with_name(artifact.name + ".sha256").write_text(f"{digest}  {artifact.name}\n")
    manifest = {"schemaVersion": 1, "artifact": artifact.name, "sha256": digest,
                "projectVersion": info["version"], "target": target["target"],
                "kwinVersion": info["kwinVersion"], "runtimePackage": dependency[0],
                "runtimePackageVersion": dependency[1], "architecture": target["arch"],
                "validation": "native SDK build and CTest; not physical desktop validation"}
    artifact.with_name(artifact.name + ".json").write_text(json.dumps(manifest, indent=2) + "\n")
    return artifact


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    parser.add_argument("--target", required=True)
    args = parser.parse_args()
    print(build_package(args.build_dir.resolve(), args.output_dir.resolve(), target_profile(args.target)))
