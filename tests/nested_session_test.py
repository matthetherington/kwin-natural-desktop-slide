#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Opt-in headless KWin integration test. Never install plugins or address the host bus."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

RENDERER = "kwin_natural_desktop_slide_devintegration"
CONTROLLER = "kwin_natural_desktop_slide_controller_devintegration"


def inside(build, outputs, directory):
    if os.environ.get("NDS_ISOLATED_TEST") != "1" or os.environ.get("DBUS_SESSION_BUS_ADDRESS") == os.environ.get("NDS_HOST_BUS"):
        raise RuntimeError("A separate D-Bus session is required")
    if Path(os.environ.get("XDG_RUNTIME_DIR", "")).resolve() != directory / "runtime":
        raise RuntimeError("A private runtime directory is required")
    qdbus = shutil.which("qdbus-qt6") or shutil.which("qdbus6") or shutil.which("qdbus")
    if not qdbus:
        raise RuntimeError("qdbus is required")
    plugins = directory / "plugins/kwin/effects/plugins"
    plugins.mkdir(parents=True)
    layout = json.loads(next(build.glob("install-layout-*.json")).read_text())
    if not layout.get("integrationDriver"):
        raise RuntimeError("Build with ENABLE_INTEGRATION_DRIVER=ON and the integration development identities")
    for role, name in (("renderer", RENDERER), ("controller", CONTROLLER)):
        entry = next(item for item in layout["artifacts"] if item["role"] == role)
        if entry["name"] != name + ".so":
            raise RuntimeError("Use the integration development identities; never test the host identities")
        shutil.copy2(entry["source"], plugins / entry["name"])
    (directory / "config/kwinrc").write_text("[Plugins]\nslideEnabled=false\nfadeDesktopEnabled=false\n"
        "kwin_natural_desktop_slideEnabled=false\nkwin_natural_desktop_slide_controllerEnabled=false\n")
    env = dict(os.environ, QT_PLUGIN_PATH=str(directory / "plugins"), KWIN_COMPOSE="O2",
               LIBGL_ALWAYS_SOFTWARE="1", QT_QPA_PLATFORM="offscreen")
    controller_path = "/org/kde/KWin/Effects/NaturalDesktopSlideController"
    renderer_path = "/org/kde/KWin/Effects/NaturalDesktopSlide"
    def call(path, method, *args):
        return subprocess.check_output([qdbus, "org.kde.KWin", path, method, *map(str, args)],
            text=True, stderr=subprocess.PIPE, timeout=10).strip()
    def effect(method, *args):
        return call("/Effects", "org.kde.kwin.Effects." + method, *args)
    def driver(method, *args):
        return call(controller_path, "org.kde.KWin.NaturalDesktopSlideController." + method, *args)
    def rendered():
        return json.loads(call(renderer_path, "org.kde.KWin.NaturalDesktopSlide.configurationStatus"))
    def desktop_numbers():
        return [output["desktop"] for output in json.loads(driver("testOutputs"))]
    def wait_for(predicate, description):
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            try:
                if predicate():
                    return
            except (subprocess.SubprocessError, ValueError):
                pass
            time.sleep(0.05)
        raise RuntimeError("Timed out: " + description)
    log = (directory / "kwin.log").open("w")
    kwin = subprocess.Popen(["kwin_wayland", "--virtual", "--output-count", str(outputs),
        "--width", "1280", "--height", "720", "--socket", "nds-test-wayland", "--no-lockscreen",
        "--no-global-shortcuts", "--no-kactivities"], env=env, stdout=log, stderr=subprocess.STDOUT)
    client = None
    try:
        wait_for(lambda: kwin.poll() is not None or RENDERER in effect("listOfEffects"), "nested compositor/plugin discovery")
        if kwin.poll() is not None:
            raise RuntimeError("Nested KWin exited: " + (directory / "kwin.log").read_text()[-6000:])
        assert effect("loadEffect", RENDERER) == "true"
        assert effect("loadEffect", CONTROLLER) == "true"
        assert len(json.loads(driver("testOutputs"))) == outputs
        client_env = dict(env, QT_QPA_PLATFORM="wayland", WAYLAND_DISPLAY="nds-test-wayland")
        client = subprocess.Popen([str(build / "tests/nested_test_client")], env=client_env,
            stdout=subprocess.DEVNULL, stderr=log)
        time.sleep(1)
        assert client.poll() is None, "Nested test client exited before rendering"
        timestamp = 0
        def gesture(index, delta, cancel=False, before_end=None):
            nonlocal timestamp
            timestamp += 1000000
            assert driver("testGesture", index, 0, 0, timestamp) == "true"
            assert driver("testGesture", index, 1, delta, timestamp + 500000) == "true"
            time.sleep(0.1)
            active = rendered()["outputs"]
            if before_end:
                before_end(active)
            assert driver("testGesture", index, 3 if cancel else 2, 0, timestamp + 700000) == "true"
            wait_for(lambda: not any(item["active"] for item in rendered()["outputs"]), "animation settlement")
        # Shared desktops preview and commit on all outputs.
        assert driver("testConfigure", "false") == "true"
        wait_for(lambda: not any(item["active"] for item in rendered()["outputs"]), "shared setup settlement")
        def shared_preview(states):
            assert len(states) == outputs and all(item["gesture"] for item in states), states
            assert all(abs(item["position"][0] - 0.4) < 1e-8 for item in states), states
            assert all(item["windowPaints"] > 0 for item in states), states
        gesture(0, 120, before_end=shared_preview)
        assert desktop_numbers() == [2] * outputs
        gesture(outputs - 1, -30) # Slow partial return, no desktop change.
        assert desktop_numbers() == [2] * outputs
        gesture(0, -120, cancel=True)
        assert desktop_numbers() == [2] * outputs
        # Independent desktops affect only the output selected at begin.
        assert driver("testConfigure", "true") == "true"
        wait_for(lambda: not any(item["active"] for item in rendered()["outputs"]), "independent setup settlement")
        def independent_preview(states):
            active = [item for item in states if item["active"]]
            assert len(active) == 1 and active[0]["gesture"], states
            assert active[0]["windowPaints"] > 0, states
        gesture(outputs - 1, 120, before_end=independent_preview)
        assert desktop_numbers() == [1] * (outputs - 1) + [2]
        gesture(0, 30, before_end=independent_preview)
        assert desktop_numbers() == [1] * (outputs - 1) + [2]
        # Mixed scale/geometry and layout changes during an owned stream.
        assert driver("testOutputLayout", 1, 1280, -100, 1.5) == "true"
        wait_for(lambda: any(output["scale"] == 1.5 for output in json.loads(driver("testOutputs"))), "fractional output scale")
        timestamp += 1000000
        assert driver("testGesture", 0, 0, 0, timestamp) == "true"
        assert driver("testGesture", 0, 1, 60, timestamp + 500000) == "true"
        assert driver("testOutputLayout", 1, 1300, -100, 1.5) == "true"
        time.sleep(0.2)
        assert driver("testGesture", 0, 2, 0, timestamp + 700000) == "true"
        assert desktop_numbers() == [1] * (outputs - 1) + [2]
        print(f"PASS: {outputs} virtual outputs; shared/independent previews, commits, aborts, fractional scale and layout-change cancellation")
    finally:
        for process in (client, kwin):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--outputs", type=int, choices=(2, 3), default=3)
    parser.add_argument("--inside", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if args.inside:
        inside(build, args.outputs, args.inside.resolve())
        return 0
    with tempfile.TemporaryDirectory(prefix="nds-nested-", dir="/tmp/opencode") as temporary:
        directory = Path(temporary)
        env = dict(os.environ, NDS_ISOLATED_TEST="1", NDS_HOST_BUS=os.environ.get("DBUS_SESSION_BUS_ADDRESS", ""),
                   WAYLAND_DISPLAY="nds-test-wayland", QT_QPA_PLATFORM="offscreen",
                   QT_QPA_PLATFORMTHEME="", QT_NO_XDG_DESKTOP_PORTAL="1", QT_ACCESSIBILITY="0",
                   XDG_CURRENT_DESKTOP="NDS-TEST")
        for variable, name in (("XDG_CONFIG_HOME", "config"), ("XDG_CONFIG_DIRS", "config-system"),
                               ("XDG_DATA_HOME", "data"), ("XDG_DATA_DIRS", "data-system"), ("XDG_CACHE_HOME", "cache"),
                               ("XDG_STATE_HOME", "state"), ("XDG_RUNTIME_DIR", "runtime")):
            path = directory / name
            path.mkdir(mode=0o700)
            env[variable] = str(path)
        process = subprocess.Popen(["dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()),
            "--inside", str(directory), "--build-dir", str(build), "--outputs", str(args.outputs)],
            env=env, start_new_session=True)
        try:
            return process.wait(timeout=120)
        finally:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass


if __name__ == "__main__":
    sys.exit(main())
