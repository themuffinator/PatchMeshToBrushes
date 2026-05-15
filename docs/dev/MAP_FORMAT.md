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

Generated brushes are not written as loose top-level world brushes. All brushes
covering a single patch mesh, or a grouped set of related patch meshes, must be
contained in one `func_group` entity so the result remains a coherent editable
unit.

Phase 5 emits generated groups after the preserved or rewritten source map. In
preserve mode, source patch primitives stay untouched. In replace mode, only the
converted patch primitive spans are removed before the generated groups are
appended. Existing `MeshToBrushes` generated groups are removed before new
groups are appended so repeated runs do not accumulate duplicate output.

## Comments and Formatting

The writer should insert generated content with stable comments:

```text
// MeshToBrushes begin: source entity 0 patch assembly 3
{
"classname" "func_group"
"_mtb_assembly" "3"
"_mtb_source_patches" "0:4 0:5"
...
}
// MeshToBrushes end
```

Stable comments make repeated conversion and manual auditing easier.

## Entity Handling

Patches should be converted in their owning entity. Worldspawn patches remain in
worldspawn semantically, but their generated brush replacements are wrapped in
`func_group` entities for editor grouping. Entity key/value pairs must be
preserved.

If source patches already live in a `func_group`, the generated brushes may
replace or accompany that group. If related patches are spread across multiple
groups but share or overlap vertices, the conversion planner should prefer a
single generated `func_group` for the complete patch assembly and report the
source entity indices it covers.

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
