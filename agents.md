# Agent Guidelines for serviz

## Tooling

- Use **CMake** for the C build system
- Use `python scripts/download_deps.py` to fetch vendored dependencies before building

## Code Style

- C code follows C11 with clear, explicit types and `const` where appropriate
- No comments unless explaining *why* (not *what*)

## Testing

- Write **lots of high-quality, useful tests**
- Unit tests for all pure functions (colormap lookups, coordinate transforms, spec parsing, etc.)
- Integration tests for the data loading pipeline
- Test edge cases: empty datasets, single points, extreme coordinates, missing optional fields
- C tests use a simple test harness (assert-based, one test binary per module)
- Tests compile with `-UNDEBUG` so `assert()` fires even in Release builds
- Run tests with: `cmake --build build && cd build && ctest --output-on-failure`

## Version Control

- **Git commit frequently** — after each logical unit of work
- Write clear, concise commit messages describing *why*, not just *what*
- Keep commits atomic: one concern per commit
- Write commit messages like a human would — no "Co-Authored-By" or AI attribution lines
- No generated boilerplate in commit messages
