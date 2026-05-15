# Brush Construction

## Construction Principles

Good output should look intentionally modeled, not mechanically combed into
thin strips. The converter should prefer broad, stable convex pieces with shared
edge boundaries.

Core rules:

- Avoid T-junctions by creating a shared segmentation lattice for touching
  patches and adjacent charts.
- Do not let one chart create edge cuts that its neighbor does not know about.
- Merge coplanar or near-coplanar runs before brush construction.
- Prefer wedges, fans, and caps with shared vertices over independent strips.
- Reject brushes with tiny sliver faces unless no better representation exists.
- Keep support faces caulked.

## Flat Sections

If a chart is planar or nearly planar, generate an extrusion along the backface
normal. The extrusion distance must be at least the configured minimum thickness,
which defaults to `8` units.

Flat sheet brush:

1. Determine the source face winding.
2. Fit or derive a stable plane.
3. Offset the back plane by `min_brush_thickness`.
4. Connect corresponding front and back edges.
5. Assign source texture to the front face and caulk to all support faces.

## Curved Open Sheets

For simple curvature, segment along curvature changes and project each segment
from its backface direction.

A curved strip should become a sequence of wedge-like convex brushes where:

- front faces approximate the curve using chord planes
- rear faces follow a consistent offset direction
- side planes align with neighboring segment boundaries
- adjoining strips share full edges

The planner should minimize segment count subject to chord error, convexity, and
texture fit constraints.

## Cylinders and Tubes

Cylindrical assemblies should be detected from repeated radial structure rather
than treated as independent patches.

Brush strategy:

- infer the cylinder axis
- divide the circumference into shared radial sectors
- create wedge brushes between adjacent radial planes
- cap open ends with coherent cap brushes when source patches include caps
- use the same angular segmentation across touching cylinder side patches

This avoids the common failure mode where each patch emits vertical strips that
terminate at unrelated heights or angles.

## Spheres, Domes, and Closed Shells

Closed assemblies need global planning. Independent patch extrusion can create
overlaps, cracks, or inverted brushes.

Brush strategy:

- classify whether the assembly is a closed shell, hemisphere, dome, or partial
  shell
- choose radial and latitude segmentation shared across all charts
- construct convex sectors that meet at complete boundaries
- collapse pole regions into controlled fan brushes when appropriate
- validate that all sectors have positive volume

## Arbitrary Patch Assemblies

When no simple primitive classification fits, the fallback should still be
methodical:

1. Sample the patch assembly.
2. Build a constrained triangulation or quad lattice over the sampled surface.
3. Merge adjacent cells that are planar enough and convex when extruded.
4. Generate brushes from merged cells.
5. Split any failed brush and retry.

The fallback should produce a report explaining why primitive classification was
not used.

## Texture Projection

Every source face should inherit its source patch texture. The projection should
be solved from the patch UVs rather than approximated with a default map editor
texture axis.

Process:

1. Gather the 3D source samples and UV values used by the face.
2. Fit a plane to the face.
3. Build a stable local tangent basis.
4. Solve affine UV projection in that basis.
5. Emit equivalent brush texture settings.
6. If error exceeds tolerance, split the face or warn.

Support faces always use:

```text
textures/common/caulk
```

## Validation

Before writing a brush, validate:

- at least four planes
- positive enclosed volume
- all face windings are nondegenerate
- all vertices are finite
- every non-source face is caulk
- adjacent generated faces share full edges or explicitly documented seams
