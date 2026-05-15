#!/usr/bin/env python3
"""Resolve the build version used by release packaging."""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import sys


SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")


def read_base_version(path: pathlib.Path) -> str:
    version = path.read_text(encoding="utf-8").strip()
    if not SEMVER_RE.fullmatch(version):
        raise ValueError(f"{path} must contain a simple X.Y.Z version")
    return version


def release_base_version(base_version: str) -> str:
    ref_type = os.environ.get("GITHUB_REF_TYPE", "")
    ref_name = os.environ.get("GITHUB_REF_NAME", "")
    if ref_type == "tag" and ref_name.startswith("v"):
        tag_version = ref_name[1:]
        if not SEMVER_RE.fullmatch(tag_version):
            raise ValueError("release tags must use vX.Y.Z")
        return tag_version

    return base_version


def release_version(base_version: str) -> str:
    version = release_base_version(base_version)

    run_number = os.environ.get("GITHUB_RUN_NUMBER", "0")
    return f"{version}+build.{run_number}"


def safe_filename_version(version: str) -> str:
    return re.sub(r"[^0-9A-Za-z_.-]+", "-", version)


def write_github_output(version: str, safe_version: str) -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if not output_path:
        return

    with open(output_path, "a", encoding="utf-8") as output:
        output.write(f"version={version}\n")
        output.write(f"safe_version={safe_version}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version-file", type=pathlib.Path, default=pathlib.Path("VERSION"))
    parser.add_argument("--github-output", action="store_true")
    args = parser.parse_args()

    try:
        version = release_version(read_base_version(args.version_file))
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    safe_version = safe_filename_version(version)
    if args.github_output:
        write_github_output(version, safe_version)

    print(version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
