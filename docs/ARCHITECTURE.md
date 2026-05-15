# Architecture

## Module Overview

```text
CLI -> IO -> Map Document -> Patch Analysis -> Assembly Planning -> Brush Building -> Map Writer
```

### `src/cli`

Owns argument parsing and stable user-facing options. The CLI layer should stay
thin: parse options, call the pipeline, print summaries, and translate failures
to exit codes.

### `src/io`

Small file helpers. This layer exists so tests can exercise parser and converter
logic without depending on shell behavior.

### `src/map`

Responsible for preserving and understanding `.map` source. The mature parser
should retain:

- entities and key/value pairs
- existing brush blocks
- patch blocks and their source spans
- comments and unrelated source text where practical

The Phase 1 parser preserves the original source text and records parsed
entities, entity key/value pairs, existing brush spans, patch source spans,
materials, dimensions, and control points. Comments and unrelated text are not
rewritten yet; future writer work should use source spans for stable edits.

### `src/geometry`

Numeric primitives and geometry operations. This module should stay independent
from map syntax. Current contents include:

- vectors and planes
- Bezier patch evaluation
- adaptive surface sampling by chord error
- convex half-space validation
- winding validation
- plane fitting
- UV projection fitting

### `src/conversion`

The core planning and conversion logic. It owns:

- patch analysis
- patch assembly graph building
- topology classification
- segmentation planning
- brush generation
- texture projection solving
- validation diagnostics

## Data Flow

1. `MapDocument::parse` loads the source and records patch spans.
2. `ConversionPlanner::plan` turns parsed patches into patch-level analysis.
3. A future `AssemblyBuilder` groups patches into connected assemblies.
4. A future `ChartPlanner` splits assemblies into conversion charts.
5. `BrushBuilder::build` emits validated brushes grouped by source patch
   assembly.
6. A future `MapWriter` inserts each generated assembly as a `func_group` in the
   original document.

## Error Model

Use diagnostics rather than hard crashes for map-level problems. A malformed
patch should not prevent other entities from being analyzed in dry-run mode.

Diagnostic severity:

- `info`: useful conversion details
- `warning`: conversion can continue but output may need review
- `error`: one patch or assembly cannot be converted
- `fatal`: the map cannot be parsed or written

## Numeric Policy

- Use `double` for computation.
- Keep input coordinates unsnapped until a specific operation needs snapping.
- Use named tolerances in one configuration object.
- Track tolerance-driven decisions in reports.

Suggested defaults:

```text
vertex_epsilon:       0.125
coplanar_epsilon:     0.01
normal_epsilon:       0.001
min_brush_thickness:  8.0
min_face_area:        0.01
```

## Testing Shape

Start with narrow tests for:

- CLI parsing
- patch scanning
- patch grid parsing
- Bezier evaluation
- planar classification
- adjacency grouping
- texture projection fitting
- convex brush validation

Then add golden `.map` fixtures with expected report output before locking down
exact brush text output.
