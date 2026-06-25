# serviz Code Review

Findings from an initial review pass. Tests proving these bugs were added
first (TDD-style) before fixes were applied.

## Significant (completeness)

1. **Per-layer `sql` and `name` are parsed but never used.** `main.c` loads
   data only from `spec.sql` using `spec.layers[0].mark`. `render_rasterise`
   renders every layer against the same `DataSet`. Layer `name` is never
   displayed (legend uses `ds.series[si].name`). Multi-layer specs just
   re-render the same data with different marks. README documents both as
   features. *Not yet fixed — tracked as known limitation.*

2. **No tests for `render.c` or `tooltip.c`.** 410 lines of rasterisation,
   hit-testing, and tooltip formatting with zero unit tests.
   `compute_bar_width`, the Bresenham line, OHLC tooltip rendering are
   untested. *Partial coverage added: `lower_bound` (tooltip hit testing
   core) now tested.*

3. **No CI, no `LICENSE`, no `install` target, no `--version`/`--help`.**
   Public GitHub repo with no license, no workflow, no install target.
   *`--help`/`--version` now work. CI/license/install not yet added.*

4. **Warning flags only on `serviz`, not on tests.** `CMakeLists.txt`
   applied `-Wall -Wextra -Wpedantic` only to the main target. *Fixed: now
   global via `SERVIZ_WARNING_FLAGS` interface library.*

## Bugs / Correctness (with regression tests)

5. **`--screenshot` as the only arg is treated as a spec filename.**
   `serviz --screenshot` (no value) fell through to `spec_file = "--screenshot"`
   → "cannot open spec file '--screenshot'". Same for `--help`/`--version`
   which were treated as spec filenames. *Fixed: flag parsing extracted
   into `cli` module, flags handled before positional args. Regression test
   in `tests/test_cli.c`.*

6. **`bucket: 0` silently produces inf/garbage.** No validation that
   histogram bucket width > 0; SQL became `floor(y/0)*0`, yielding infinity.
   *Fixed: `data_load_histogram` rejects `bucket_width <= 0`; `spec_parse`
   rejects `bucket <= 0` for histogram/candle. Regression test in
   `tests/test_data.c::test_histogram_zero_bucket_rejected`.*

7. **Unknown `mark` names silently became `line`.** A typo like
   `"lines"` rendered a line chart with no warning. Problematic for an
   AI-agent-targeted tool. *Fixed: `parse_mark` returns -1 on unknown;
   `spec_parse` rejects with a clear error. Regression test in
   `tests/test_spec.c::test_unknown_mark_rejected`.*

8. **Unchecked mallocs in `data_load_ohlc`.** Allocated open/high/low/close
   with no NULL check; a failure crashed in the next loop.
   *Fixed: checked and cleaned up on failure. No regression test (would
   require malloc failure injection).*

9. **`probe_columns` didn't load httpfs for `s3://` queries.** OHLC/histogram
   over S3 broke because probing failed. *Fixed: `probe_columns` now loads
   httpfs for `s3://` queries like `data_load` does. No regression test
   (requires network).*

10. **Query double-execution on DECIMAL columns.** Ran the full query,
    detected DECIMAL, then re-ran the wrapped query. First run should be
    a `LIMIT 0` probe. *Fixed: now probes with `SELECT * FROM (...) LIMIT 0`
    to detect DECIMAL columns cheaply, then executes the wrapped (or
    original) query exactly once.*

11. **Bar baseline comment lies.** `render.c` comment said "Baseline at y=0
    if visible, otherwise bottom of viewport" but code always used y=0.
    *Fixed: baseline now clamps to the viewport edge when y=0 is outside
    the visible range.*

## Minor / Quality

12. **Dead code in `axes.c`.** `t->value = v;` assigned twice; first is
    overwritten. *Removed.*

13. **Dead code in `tooltip.c`.** `margin_data` computed then
    `(void)margin_data;`. *Removed.*

14. **`CMakeLists.txt` error message** referenced "geoviz" — a different
    project, confusing copy-paste. *Fixed: now mentions download_deps.py.*

15. **`download_deps.py`** — no error handling, no checksum verification,
    no retry. *Fixed: now has retry with backoff, HTTP error checking,
    timeout, and `--force` flag. No checksum verification (upstream
    doesn't publish them).*

16. **`agents.md`** references Python/pytest conventions but there's no
    Python test code — boilerplate that doesn't apply. *Fixed: removed
    Python/pytest references, added note about `-UNDEBUG` test flag and
    `download_deps.py`.*

17. **Screenshot uses a magic 300-frame wait.** Fragile and undocumented.
    *Fixed: replaced with a named constant `SCREENSHOT_DELAY_FRAMES`
    (10 frames ≈ 0.17s, enough for the render to settle).*

18. **`assert()` in Release builds is a no-op.** All test files use
    `assert()` as their assertion mechanism, but `-DNDEBUG` is defined in
    Release builds (`CMAKE_C_FLAGS_RELEASE = -O3 -DNDEBUG`). This means the
    assertions vanish and tests *pass regardless of whether checks hold* —
    they only `printf("PASS...")`. Discovered when enabling `-Wall` on
    tests triggered `-Wunused-variable` for the `int rc = ...; assert(rc)`
    pattern. *Fixed: test targets now compile with `-UNDEBUG` (via
    `serviz_test_flags` interface library) which re-enables `assert()` in
    Release builds.*

19. **`spec.c:read_file` ignores `fread` return value.** Flagged by
    `-Wunused-result` when compiling with extra warnings. *Fixed: now
    uses the return value to NUL-terminate, and checks `ftell < 0`.*

## Test coverage added

- `tests/test_cli.c` — new file covering arg parsing (flags, positional,
  missing values, help/version)
- `tests/test_spec.c` — added cases for unknown mark names, negative
  buckets, string buckets for histogram
- `tests/test_data.c` — added case proving zero bucket is rejected
- `tests/test_tooltip.c` — new file covering `lower_bound` binary search
  and hit-test edge cases (empty dataset, out-of-range, nearest-point)

## Not yet addressed (future work)

- Per-layer data loading + display layer `name` in legend (#1)
- Full render/tooltip tests (#2)
- CI, LICENSE, install target (#3)
