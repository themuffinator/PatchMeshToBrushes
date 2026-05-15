# Map Format Notes

## Patch Families

The converter should recognize:

- `patchDef`
- `patchDef2`
- `patchDef3`

Patch blocks commonly contain:

- material name
- optional patch flags or dimensions depending on variant
- a 2D control grid
- control points with position and texture coordinates

The mature parser should preserve source spans so the writer can replace patch
blocks without disturbing unrelated map text.

Phase 1 parses patch materials, five-value dimension tuples, and nested control
point grids. Malformed patch blocks are retained as patch records with
diagnostics so dry-run analysis can continue across the rest of the map.

## Brush Output

The target output is convex `brushDef` data. Each brush must describe a valid
closed convex polyhedron. Faces that approximate the source patch carry the
source texture projection; support faces are caulked.

## Comments and Formatting

The writer should insert generated content with stable comments:

```text
// MeshToBrushes begin: entity 0 patch assembly 3
...
// MeshToBrushes end
```

Stable comments make repeated conversion and manual auditing easier.

## Entity Handling

Patches should be converted in their owning entity. Worldspawn patches remain in
worldspawn unless the user requests otherwise. Entity key/value pairs must be
preserved.

## Source Preservation

The implementation should keep a source span for every patch:

```text
start_offset
end_offset
body_start_offset
body_end_offset
entity_index
patch_index_in_entity
```

This enables:

- precise diagnostics
- replacement mode
- preserve mode
- report links back to original map positions
