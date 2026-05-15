#!/usr/bin/env python3
"""Create a release zip for one PatchMeshToBrushes platform build."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys


def find_binary(build_dir: pathlib.Path) -> pathlib.Path:
    candidates = [
        build_dir / "Release" / "patch-mesh-to-brushes.exe",
        build_dir / "Debug" / "patch-mesh-to-brushes.exe",
        build_dir / "patch-mesh-to-brushes.exe",
        build_dir / "Release" / "patch-mesh-to-brushes",
        build_dir / "Debug" / "patch-mesh-to-brushes",
        build_dir / "patch-mesh-to-brushes",
    ]
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate
    raise FileNotFoundError("could not find patch-mesh-to-brushes binary")


def copy_if_exists(source: pathlib.Path, destination: pathlib.Path) -> None:
    if source.exists():
        shutil.copy2(source, destination / source.name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True)
    parser.add_argument("--version")
    parser.add_argument("--build-dir", type=pathlib.Path, default=pathlib.Path("build"))
    parser.add_argument("--readme-html", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, default=pathlib.Path("dist"))
    args = parser.parse_args()

    binary = find_binary(args.build_dir)
    version_part = f"-{args.version}" if args.version else ""
    package_name = f"PatchMeshToBrushes{version_part}-{args.platform}"
    staging_dir = args.output_dir / package_name
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True)

    shutil.copy2(binary, staging_dir / binary.name)
    for filename in ["README.md", "LICENSE", "CREDITS.md"]:
        copy_if_exists(pathlib.Path(filename), staging_dir)
    shutil.copy2(args.readme_html, staging_dir / "README.html")

    archive_base = args.output_dir / package_name
    archive_path = shutil.make_archive(str(archive_base), "zip", args.output_dir, package_name)
    print(archive_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
