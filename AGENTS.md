# Agent Guidance

This repository is built phase by phase from the roadmap. Keep changes focused,
deterministic, and easy to review.

## Working Rules

- Prefer the existing C++20/CMake structure and local module boundaries.
- Keep parser and geometry decisions explicit in tests and documentation.
- Keep all technical and development documents under `docs/dev/`. This includes
  architecture, roadmap, implementation planning, map format, and brush
  construction notes.
- Preserve source-map text whenever possible; later writer phases depend on
  stable source spans.
- Run the relevant build and tests before reporting completion:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

## Borrowed Code And Credits

Only borrow code from projects with licences compatible with this repository's
GPL-3.0 licence. Before using borrowed code, verify that the source licence is
compatible and that any attribution or notice requirements can be satisfied.

Maintain [CREDITS.md](CREDITS.md) whenever code, algorithms, substantial test
fixtures, or implementation text are borrowed or adapted from another project.
Each entry must use elegant embedded Markdown links to the borrowed source code,
the upstream project, and the relevant licence. Prefer descriptive prose over
bare URLs, for example:

```markdown
- Adapted the winding validation routine from
  [ExampleMapTools `winding.cpp`](https://example.invalid/project/winding.cpp),
  distributed under the [GPL-3.0 licence](https://example.invalid/project/COPYING).
```

If no external code has been borrowed, keep the credits file present and state
that explicitly.
