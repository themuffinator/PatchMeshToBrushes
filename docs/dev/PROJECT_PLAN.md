# Project Plan

## Problem Statement

Quake III Arena era patches are curved surface descriptions. They are useful for
visual mesh detail, but many downstream workflows need convex brushes. This tool
will inspect each patch in a `.map`, infer how patches relate to each other, and
build brush sets that cover the same playable or blocking volume as elegantly as
possible.

The converter must not simply drop a thin vertical brush under each sampled edge.
That produces excessive strips, T-junctions, poor editor ergonomics, and fragile
compile results. The target construction should use shared segment boundaries,
coherent caps, and broad convex pieces where the topology allows it.

## Definitions

- **Patch**: A parsed `patchDef`, `patchDef2`, or `patchDef3` block and its
  texture, dimensions, control points, and patch-local UV data.
- **Patch assembly**: A connected group of patches that share quantized vertices,
  overlapping boundary spans, or intentionally touching seams.
- **Output group**: A generated `func_group` entity that contains every brush
  covering one patch or one patch assembly.
- **Chart**: A mostly continuous region of a patch assembly that can be sampled
  and converted using one brush generation strategy.
- **Source face**: A brush face that lies on, or approximates, the visible patch
  surface. It inherits the patch texture.
- **Support face**: Any face that exists only to close the convex brush. It uses
  `textures/common/caulk`.

## End-to-End Pipeline

1. Parse the `.map` into an entity preserving document model.
2. Discover patch definitions and retain exact source spans for future rewriting.
3. Decode patch metadata, dimensions, control grids, and UV coordinates.
4. Build an adjacency graph over patches using quantized positions and boundary
   overlap tests.
5. Group patches into assemblies and classify each assembly:
   - planar or nearly planar sheet
   - open curved sheet
   - cylinder or partial cylinder
   - sphere, dome, or closed shell
   - cap or bevel
   - mixed arbitrary patch assembly
6. Split each assembly into charts using curvature, texture discontinuity, seam
   constraints, and convexity limits.
7. Generate a shared segmentation lattice for each chart so neighboring brushes
   meet on common edges and avoid T-junctions.
8. Build convex brushes:
   - planar charts extrude by at least `8` units
   - simple curved charts project from the backface direction
   - closed shells build inward or outward shell sectors depending on intent
   - cylinder-like shapes use radial wedges and aligned caps
   - arbitrary assemblies use fallback subdivision and merge planar runs where
     possible
9. Assign materials:
   - source faces inherit the patch material and a solved texture projection
   - support faces use `textures/common/caulk`
10. Validate every generated brush:
   - convexity
   - nonzero volume
   - valid face plane winding
   - no accidental duplicate coplanar faces
   - no local T-junctions along generated seams
11. Place the generated brush set for each patch or patch assembly inside one
    `func_group` output entity.
12. Rewrite or append the grouped brush output in the map while preserving
    unrelated text.

## Geometry Approach

The implementation should be deterministic and conservative:

- Work in double precision internally.
- Snap only at explicit decision points and report the snap tolerance used.
- Use stable quantization for graph construction, not for final geometry.
- Prefer exact source control points for seam detection.
- Treat both shared boundary vertices and overlapping boundary spans as assembly
  graph edges.
- Sample Bezier patches with a target chord error, then simplify planar runs.
- Carry a shared boundary lattice through every chart in an assembly.

## Texture Approach

Patch control points include UV coordinates. A brush face needs a planar texture
projection. For every source face:

1. Collect the source surface samples used by the face.
2. Fit a local plane and tangent basis.
3. Solve a least-squares mapping from 3D face coordinates to patch UVs.
4. Convert that mapping to the target brush texture fields.
5. Preserve source material name, scale, rotation, and offsets as closely as the
   brush format permits.

When the fit error is too high, split the chart further rather than smearing the
texture across a face that no longer represents the patch.

## Output Policy

Conversion defaults to replacing converted patch blocks with generated brushes
behind clear comments. Every generated brush set for a patch or patch assembly
must be emitted inside a `func_group`; loose generated brushes should be treated
as invalid output. Preserve mode remains available for comparison output.

Phase 5 implements the initial writer path: generated assembly groups are
appended with stable `PatchMeshToBrushes` comments, replace mode removes
converted patch spans before appending the groups, and preserve mode keeps
source patch blocks. Reports include planned group/brush counts and texture
projection fit diagnostics.

Generated `func_group` entities should carry stable comments naming the source
entity, source patch ids, and patch assembly id. If a patch assembly contains
multiple patches, all covering brushes belong to the same group.

Planned modes:

- `--dry-run`: analyze and report only.
- `--preserve-patches`: keep original patches and append generated brushes.
- `--replace-patches`: remove converted patches and insert generated brushes
  (default).
- `--report <path>`: write a conversion report with patch grouping, strategies,
  warnings, brush counts, and texture fit errors.

## Quality Gates

- Every conversion pass has deterministic output.
- Golden fixtures pin exact map and report output for small representative
  cases.
- Malformed inputs are fuzzed through parse, plan, report, build, and reparse.
- Real Quake III Arena era editor output is covered by the q3dm1 sample
  conversion regression.
- Large-map benchmark timing is available through `benchmark_large_map`.
- Each generated brush is convex and has positive volume.
- Every patch-facing generated face records its source patch id.
- Adjacent generated brushes share complete edges where possible.
- Every generated brush belongs to the `func_group` for its source patch or patch
  assembly.
- Brush planning produces validated in-memory brush plans before writer work
  serializes any map text.
- Writer output places every generated brush inside the `func_group` for its
  source patch assembly.
- Flat sections never produce support thickness below `8` units by default.
- Warnings are actionable and include entity and patch indices.
