# MeshToBrushes

MeshToBrushes is planned as a command line converter for Quake III Arena era
`.map` files. Its job is to find every `patchDef`, `patchDef2`, and `patchDef3`
mesh in the input map and replace or accompany those curved surfaces with a
carefully planned set of convex `brushDef` brushes.

The project has a compilable CLI, a preserving map parser, patch assembly
analysis, in-memory brush planning, grouped brush emission, tests, and
documentation for the geometry strategy.

## Goals

- Convert open and closed patch meshes, including cylinders, spheres, caps,
  flat sheets, bevels, end caps, and arbitrary simple curved meshes.
- Group patches that share or overlap vertices so one surface assembly can be
  converted as a coherent shape rather than as isolated strips.
- Emit generated brushes for each patch mesh or patch assembly inside a coherent
  `func_group` so the replacement remains easy to select, inspect, and move in
  an editor.
- Preserve patch-facing materials by deriving brush face texture projection from
  the source patch texture coordinates.
- Assign non-source faces to `textures/common/caulk`.
- Use a minimum thickness of `8` units for flat sections.
- Project curved simple meshes from the backface direction.
- Avoid brittle construction patterns such as unnecessary narrow strips,
  unaligned edges, and T-junctions.

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug
build\Debug\benchmark_large_map.exe tests\map\q3dm1sample.map 1 3
```

## CLI Shape

```powershell
mesh-to-brushes input.map --dry-run
mesh-to-brushes input.map -o output.map --min-thickness 8
mesh-to-brushes input.map --report conversion-report.md --preserve-patches
```

Current behavior:

- `--dry-run` reads the map, parses entities, key/value pairs, existing brush
  blocks, and patch control grids, then prints a planned conversion summary with
  early geometry notes such as planarity, sampling counts, planar UV fit, patch
  assembly grouping, coarse topology classification, and in-memory brush planning
  counts.
- Non-dry-run conversion emits generated `brushDef` brushes inside one
  `func_group` per patch assembly. By default it preserves source patches and
  appends the groups; `--replace-patches` removes converted source patch blocks
  before appending the generated groups.
- `--report` writes patch grouping, brush strategy, brush counts, and texture
  projection fit diagnostics.

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
tests/          Small baseline tests for the scaffold
docs/dev/       Technical development notes and roadmap
examples/       Tiny hand-authored map snippets for development
```
