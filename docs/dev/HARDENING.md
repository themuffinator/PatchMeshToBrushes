# Hardening

Phase 6 keeps the converter honest with a mix of exact-output checks,
malformed-input coverage, deterministic output tests, real map regression tests,
and a benchmark tool.

## Golden Fixtures

Golden fixtures live under `tests/golden/`.

- `planar_patch_input.map` is the small source map.
- `planar_patch_preserve_expected.map` is the exact preserve-mode output.
- `planar_patch_replace_expected.map` is the exact replace-mode output.
- `planar_patch_report_expected.md` is the exact report for preserve mode.

`test_golden_conversion` compares generated output and report text against those
fixtures after normalizing line endings.

## Malformed Fuzzing

`test_malformed_fuzz` runs handcrafted malformed maps and deterministic generated
token streams through:

1. map parsing
2. conversion planning
3. markdown report rendering
4. brush building
5. output reparsing

The test does not require malformed inputs to become clean maps. It asserts that
the pipeline stays diagnostic-driven and does not throw.

## Determinism

`test_deterministic_output` converts the q3dm1 sample repeatedly and compares
the exact map and report output. It also verifies preserve-mode reruns replace
previous `MeshToBrushes` generated groups rather than accumulating duplicates.

## Real-Map Regression

`tests/map/q3dm1sample.map` remains the representative Quake III Arena era
editor fixture. `test_q3dm1_fixture` verifies the parser counts, and
`test_q3dm1_conversion` verifies preserve and replace conversion counts:

- `188` source entities
- `987` source brushes
- `113` source patches
- `63` generated patch assembly groups
- `2440` generated brushes

## Benchmark

`benchmark_large_map` is a standalone executable built when
`MTB_BUILD_BENCHMARKS` is enabled.

Example:

```powershell
build\Debug\benchmark_large_map.exe tests\map\q3dm1sample.map 1 3
```

Arguments:

- `<map-path>`: source map to benchmark
- `[repeat-count]`: optional source concatenation multiplier for larger input
- `[iterations]`: optional number of timed runs

The benchmark reports average parse, planning, and build times along with entity,
patch, planned brush, generated group, and generated brush counts.
