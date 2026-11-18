#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare a tested release patch; push its commit/tag only after the CI gates."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PLUGIN_FILES = ("effect/metadata.json", "effect/controller_metadata.json",
                "effect/probe_metadata.json", "kcm/metadata.json")
RELEASE_FILES = ("CMakeLists.txt", *PLUGIN_FILES, "CHANGELOG.md")
VERSION = r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"

spec = importlib.util.spec_from_file_location("nds_prepare_release", Path(__file__).with_name("prepare-release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def version_tuple(version):
    if not re.fullmatch(VERSION, version):
        raise ValueError("Invalid release version: " + version)
    return tuple(map(int, version.split(".")))


def project_version(root):
    match = re.search(r"project\([^\n]+ VERSION (" + VERSION + r")", (root / "CMakeLists.txt").read_text())
    if not match:
        raise ValueError("Missing project version")
    return match[1]


def next_version(current, tags):
    current = version_tuple(current)
    versions = [version_tuple(tag[1:]) for tag in tags if re.fullmatch("v" + VERSION, tag)]
    if any(version[:2] > current[:2] for version in versions):
        raise ValueError("A newer release line exists; update the default branch before rebuilding")
    patch = max([current[2], *(v[2] for v in versions if v[:2] == current[:2])]) + 1
    return ".".join(map(str, (*current[:2], patch)))


def prepare(root, tags):
    if git(root, "diff", "--name-only") or git(root, "diff", "--cached", "--name-only"):
        raise ValueError("Prepare scheduled releases from a clean checkout")
    current = project_version(root)
    release.release_notes("v" + current, root)  # Check all metadata before editing.
    version = next_version(current, tags)
    replacements = {}
    cmake = root / "CMakeLists.txt"
    replacements[cmake] = cmake.read_text().replace(" VERSION " + current + " ", " VERSION " + version + " ", 1)
    for name in PLUGIN_FILES:
        path = root / name
        text, count = re.subn(r'("Version"\s*:\s*)"' + re.escape(current) + '"',
                              lambda match: match[1] + '"' + version + '"', path.read_text())
        if count != 1:
            raise ValueError("Expected one plugin version in " + name)
        replacements[path] = text
    changelog = root / "CHANGELOG.md"
    text = changelog.read_text()
    note = ("- Automated native package rebuild against the latest distribution SDKs "
            "on supported KWin source branches.\n"
            "- Binaries still require their exact build-time KWin package; "
            "CI checks are not physical desktop validation.\n")
    unreleased = re.search(r"^## Unreleased\s*\n(.*?)(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    section = "## " + version + "\n\n"
    if unreleased:
        section += unreleased[1].strip() + "\n\n" if unreleased[1].strip() else ""
        replacements[changelog] = text[:unreleased.start()] + section + note + "\n" + text[unreleased.end():]
    elif text.startswith("# Changelog\n"):
        replacements[changelog] = "# Changelog\n\n" + section + note + "\n" + text[len("# Changelog\n"):].lstrip()
    else:
        raise ValueError("Expected a Changelog heading")
    for path, text in replacements.items():
        path.write_text(text)
    notes = release.release_notes("v" + version, root)
    patch = subprocess.check_output(["git", "-C", str(root), "diff", "--binary", "--", *RELEASE_FILES], text=True)
    return version, notes, patch


def publish_snapshot(root, tag, base, branch):
    """Never overwrite a branch, existing tag, or publish untested source edits."""
    if not re.fullmatch("v" + VERSION, tag) or not re.fullmatch(r"[0-9a-f]{40}", base):
        raise ValueError("Invalid scheduled release tag/base")
    git(root, "check-ref-format", "refs/heads/" + branch)
    if git(root, "rev-parse", "HEAD") != base:
        raise ValueError("Checkout no longer matches the tested base")
    release.release_notes(tag, root)
    changed = set(git(root, "diff", "--name-only").splitlines())
    if changed != set(RELEASE_FILES) or git(root, "diff", "--cached", "--name-only"):
        raise ValueError("Scheduled snapshot must contain only the prepared release files")
    if git(root, "ls-remote", "origin", "refs/tags/" + tag):
        # A failed GitHub publication can be retried, but never replace a tag.
        git(root, "fetch", "origin", "refs/tags/" + tag)
        git(root, "add", "--", *RELEASE_FILES)
        if (git(root, "rev-parse", "FETCH_HEAD^{tree}") != git(root, "write-tree")
                or git(root, "show", "-s", "--format=%P", "FETCH_HEAD^{}") != base):
            raise ValueError("Release tag already exists with a different snapshot")
        return
    remote = git(root, "ls-remote", "origin", "refs/heads/" + branch)
    if not remote or remote.split()[0] != base:
        raise ValueError("Default branch advanced; rebuild the new source before publishing")
    git(root, "add", "--", *RELEASE_FILES)
    git(root, "-c", "user.name=github-actions[bot]",
        "-c", "user.email=41898282+github-actions[bot]@users.noreply.github.com",
        "commit", "-m", "Release " + tag[1:] + " (scheduled native rebuild)")
    git(root, "-c", "user.name=github-actions[bot]",
        "-c", "user.email=41898282+github-actions[bot]@users.noreply.github.com",
        "tag", "-a", tag, "-m", "Natural Desktop Slide " + tag[1:] + ": scheduled native rebuild")
    git(root, "push", "--atomic", "origin", "HEAD:refs/heads/" + branch, "refs/tags/" + tag)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--tags-file", type=Path, required=True)
    prepare_parser.add_argument("--notes", type=Path, required=True)
    prepare_parser.add_argument("--patch", type=Path, required=True)
    prepare_parser.add_argument("--github-output", type=Path, required=True)
    publish_parser = commands.add_parser("publish-snapshot")
    publish_parser.add_argument("--tag", required=True)
    publish_parser.add_argument("--base", required=True)
    publish_parser.add_argument("--branch", required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        version, notes, patch = prepare(ROOT, args.tags_file.read_text().splitlines())
        args.notes.write_text(notes)
        args.patch.write_text(patch)
        with args.github_output.open("a") as stream:
            stream.write(f"tag=v{version}\nversion={version}\n")
    else:
        publish_snapshot(ROOT, args.tag, args.base, args.branch)
