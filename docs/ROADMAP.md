# Roadmap

## Phase 0: Scaffold

- [x] Create CMake project.
- [x] Add CLI option parsing.
- [x] Add patch definition discovery.
- [x] Add conversion planning shell.
- [x] Add brush builder shell.
- [x] Add project documentation.
- [x] Add baseline tests.

## Phase 1: Map Parser

- [ ] Parse entities and key/value pairs.
- [ ] Parse existing brush blocks.
- [ ] Parse `patchDef`, `patchDef2`, and `patchDef3` dimensions and control
      points.
- [ ] Preserve exact patch source spans.
- [ ] Report malformed patch blocks without aborting dry-run analysis.

## Phase 2: Geometry Core

- [ ] Implement Bezier patch evaluation.
- [ ] Implement surface sampling by chord error.
- [ ] Add plane fitting and planar classification.
- [ ] Add polygon winding and convex volume validation.
- [ ] Add UV projection fitting.

## Phase 3: Patch Grouping

- [ ] Quantize patch boundary vertices.
- [ ] Detect shared and overlapping boundary spans.
- [ ] Build assembly graphs.
- [ ] Classify assemblies as planar, cylindrical, spherical, capped, or
      arbitrary.

## Phase 4: Brush Planning

- [ ] Build shared segmentation lattices.
- [ ] Generate planar extrusions with minimum thickness.
- [ ] Generate curved sheet wedges from backface projection.
- [ ] Generate cylinder sectors and caps.
- [ ] Generate dome and sphere sectors.
- [ ] Add arbitrary fallback subdivision and merge passes.

## Phase 5: Writer and Reports

- [ ] Emit valid `brushDef` blocks.
- [ ] Preserve or replace patch blocks based on CLI mode.
- [ ] Write conversion reports.
- [ ] Include texture projection fit diagnostics.

## Phase 6: Hardening

- [ ] Add golden map fixtures.
- [ ] Add fuzz tests for malformed maps.
- [ ] Add deterministic output tests.
- [ ] Test with real Quake III Arena era editor output.
- [ ] Add performance benchmarks for large maps.
