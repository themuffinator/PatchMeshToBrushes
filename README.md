# PatchMeshToBrushes

PatchMeshToBrushes is a command line tool for Quake III Arena era `.map`
files. It finds curved patch meshes and creates practical brush versions of
them, grouped neatly so the result is easier to select, inspect, and edit in a
map editor.

It is designed for mapper-friendly output:

- curved patch meshes become convex `brushDef` brushes
- each converted patch or patch set is placed in a `func_group`
- visible faces keep the patch material where possible
- hidden support faces use `textures/common/caulk`
- flat sections keep at least `8` units of thickness
- generated output can be kept beside the original patches or replace them in a
  copy of the map

## Getting Started

1. Download the build for your computer from the
   [latest release](https://github.com/themuffinator/PatchMeshToBrushes/releases/latest).
2. Unzip the download somewhere convenient, such as your desktop or a tools
   folder.
3. Open a terminal in that folder.
4. Try a safe preview first:

```powershell
patch-mesh-to-brushes path\to\your.map --dry-run
```

If the preview looks good, write a new map file:

```powershell
patch-mesh-to-brushes path\to\your.map -o path\to\your_brushes.map
```

Your original map is left alone when you use `-o`. Open the new map in your
editor and inspect the generated groups.

Release downloads also include a styled `README.html` for offline reading. For
bugs, ideas, or rough edges, use the
[issue tracker](https://github.com/themuffinator/PatchMeshToBrushes/issues).

## Usage

### Preview A Map

Use this when you only want to see what the tool finds:

```powershell
patch-mesh-to-brushes my_map.map --dry-run
```

This does not write a map file.

### Create A Converted Copy

Use this for everyday conversion:

```powershell
patch-mesh-to-brushes my_map.map -o my_map_brushes.map
```

The converted brushes are added in new `func_group` objects, and the original
patch meshes are kept.

### Replace Patch Meshes In The Output Copy

Use this when you want the new file to contain brushes instead of the original
patch meshes:

```powershell
patch-mesh-to-brushes my_map.map -o my_map_brushes.map --replace-patches
```

This changes only the output file you choose with `-o`.

### Write A Report

Use a report when you want a readable summary of what happened:

```powershell
patch-mesh-to-brushes my_map.map -o my_map_brushes.map --report conversion-report.md
```

The report lists how many patch meshes were found, how they were grouped, and
how many brushes were created.

### Common Options

| Option | What it does |
| --- | --- |
| `--dry-run` | Shows what would happen without writing a map. |
| `-o, --output <path>` | Writes the converted map to this path. |
| `--preserve-patches` | Keeps original patch meshes and adds brush groups. This is the default. |
| `--replace-patches` | Removes converted patch meshes from the output file. |
| `--report <path>` | Writes a conversion report. |
| `--min-thickness <number>` | Sets the minimum thickness for flat brush sections. The default is `8`. |
| `--version` | Prints the tool version. |
| `--help` | Shows the built-in help. |

## Build From Source

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug
build\Debug\benchmark_large_map.exe tests\map\q3dm1sample.map 1 3
```

## Test Fixtures

`tests/map/q3dm1sample.map` is a real-world sample fixture. The parser expects
to discover `188` entities, `987` existing brush blocks, and `113` valid
`patchDef2` blocks in that map, which gives later phases a practical regression
check as the project moves beyond the tiny example map.

## Documentation

- [Project Plan](docs/dev/PROJECT_PLAN.md)
- [Architecture](docs/dev/ARCHITECTURE.md)
- [Brush Construction](docs/dev/BRUSH_CONSTRUCTION.md)
- [Hardening](docs/dev/HARDENING.md)
- [Map Format Notes](docs/dev/MAP_FORMAT.md)
- [Releases](docs/dev/RELEASES.md)
- [Roadmap](docs/dev/ROADMAP.md)
- [Credits](CREDITS.md)

## Repository Layout

```text
src/
  cli/          Argument parsing and user-facing options
  conversion/   Patch analysis, grouping, brush planning, brush output
  geometry/     Numeric geometry primitives
  io/           File loading and saving helpers
  map/          Map parser and document model
tests/          Small baseline tests and real map fixtures
docs/dev/       Technical development notes and roadmap
examples/       Tiny hand-authored map snippets for development
benchmarks/     Optional benchmark tools
tools/          Release packaging helpers
```
