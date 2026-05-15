# Releases

PatchMeshToBrushes uses a deliberately simple versioning model.

## Source Version

The repository root `VERSION` file stores the base public version as `X.Y.Z`.
CMake reads that file during configuration and embeds the resolved display
version in `patch-mesh-to-brushes --version`.

Local builds use the base version by default. A build can override the displayed
version without editing source files:

```powershell
cmake -S . -B build -DPMTB_VERSION_OVERRIDE=0.1.0-local
```

## Release Build Iteration

The release workflow resolves versions with `tools/version.py`:

- tag builds named `vX.Y.Z` publish under that tag and embed binary version
  `X.Y.Z+build.<github-run-number>`;
- manual release builds use `VERSION+build.<github-run-number>`.

Package filenames use a filesystem-safe spelling of that resolved version, so a
manual build for run `42` produces archives such as
`PatchMeshToBrushes-0.1.0-build.42-windows-x64.zip`.

## Push Verification

`.github/workflows/verify.yml` builds and tests every push and pull request on
Windows, macOS, and Linux. The release workflow also builds and tests before
packaging, so published archives are produced only from verified binaries.
