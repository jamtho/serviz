# serviz

Native, high-performance series data visualization. Renders time series and numeric data as interactive charts with pan/zoom. Reads a small JSON spec and executes a SQL query via DuckDB, then opens an interactive window.

Single statically-linked binary. Built with DuckDB (data loading + computation), raylib (rendering), and cJSON (spec parsing).

Designed primarily as a tool for AI agents helping users with data analysis — an agent writes a small JSON spec and calls `serviz` to get an interactive chart without needing a browser, notebook, or Python plotting stack.

## Examples

```bash
# Line chart from a parquet file
serviz spec.json

# Save a screenshot
serviz spec.json --screenshot output.png

# Compute and visualise in one step — no intermediate files needed
cat <<'EOF' | serviz
{
  "sql": "SELECT time, temperature AS value FROM read_parquet('sensors.parquet')",
  "layers": [{ "mark": "line" }],
  "title": "Temperature"
}
EOF

# Multi-series: sin vs cos
cat <<'EOF' | serviz
{
  "sql": "SELECT i AS x, sin(i*0.05) AS y, 'sin' AS series FROM range(500) t(i) UNION ALL SELECT i, cos(i*0.05), 'cos' FROM range(500) t(i)",
  "layers": [{ "mark": "line" }],
  "title": "Sin vs Cos"
}
EOF

# OHLC candlestick chart — bucketing is a presentation concern
cat <<'EOF' | serviz
{
  "sql": "SELECT time, price FROM read_parquet('trades.parquet')",
  "layers": [{ "mark": "candle", "bucket": "1 day" }],
  "title": "Daily Candles"
}
EOF

# Histogram
cat <<'EOF' | serviz
{
  "sql": "SELECT i AS x, random() * 100 AS y FROM range(10000) t(i)",
  "layers": [{ "mark": "histogram", "bucket": 5 }],
  "title": "Value Distribution"
}
EOF
```

## Spec format

```json
{
  "sql": "SELECT time, price FROM read_parquet('data.parquet')",
  "layers": [
    { "mark": "line", "scheme": "viridis", "name": "Price" }
  ],
  "title": "Chart Title"
}
```

### Top-level fields

| Field | Required | Description |
|-------|----------|-------------|
| `sql` | yes | Any DuckDB SQL query |
| `layers` | yes | Array of layer objects |
| `title` | no | Chart title |

### Layer fields

| Field | Required | Description |
|-------|----------|-------------|
| `mark` | yes | `"line"`, `"point"`, `"bar"`, `"histogram"`, or `"candle"` |
| `bucket` | for histogram/candle | Bucket size — numeric (e.g. `5`) or time interval (e.g. `"1 day"`) |
| `scheme` | no | Colormap: `"viridis"`, `"inferno"`, `"plasma"`, `"turbo"` (default: viridis) |
| `point_size` | no | Point diameter in pixels (default: 6) |
| `name` | no | Series name for legend |
| `sql` | no | Per-layer SQL override |

### Mark types

| Mark | Description | Bucket |
|------|-------------|--------|
| `line` | Line connecting data points | — |
| `point` | Filled squares at data points | — |
| `bar` | Vertical bars from y=0 | — |
| `histogram` | Bins y values, renders counts as bars | required (numeric bin width) |
| `candle` | OHLC candlestick chart | required (time interval or numeric) |

For `histogram` and `candle`, bucketing is a presentation concern — write SQL that computes raw data, and declare aggregation in the spec. Serviz wraps your SQL with a DuckDB aggregation query internally.

### Column inference

Serviz infers column roles from names (case-insensitive, first match wins):

| Role | Candidates | Fallback |
|------|-----------|----------|
| x | `x`, `time`, `timestamp`, `ts`, `date`, `datetime`, `t` | column 0 |
| y | `y`, `value`, `val`, `v`, `output`, `result`, `price` | column 1 |
| series | `series`, `group`, `category`, `name`, `label`, `id` | — |
| color | `color`, `colour` | — |

Any remaining columns are available in tooltips. You can always override by aliasing in SQL (e.g. `SELECT foo AS x, bar AS y`).

Time types (`TIMESTAMP`, `DATE`, `TIME`) are auto-detected from DuckDB column types — no configuration needed.

### Multiple series

Two mechanisms:

1. **`series` column**: A single query with a column that names each series
   ```sql
   SELECT time AS x, value AS y, sensor_id AS series FROM readings
   ```
2. **Multiple layers**: Each layer can have its own `sql` and `name`

## Usage

```
serviz [spec.json] [--screenshot path]
```

- With a file argument: reads spec from that file
- Without arguments: reads spec from stdin

## Controls

| Input | Action |
|-------|--------|
| Left-click drag | Pan |
| Scroll wheel | Zoom x-axis (y auto-fits to visible data) |
| R | Reset viewport to full data extent |

Tooltips appear on hover showing x, y, series name, and all extra column values. For candle charts, tooltips show open/high/low/close.

## Quick start

```bash
# Download vendored dependencies
python scripts/download_deps.py

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run
build/serviz test_specs/numeric.json             # sine wave
build/serviz test_specs/multi_series.json        # sin vs cos
build/serviz test_specs/timeseries.json          # time series
build/serviz test_specs/candles.json             # OHLC candles
build/serviz test_specs/histogram.json           # histogram
build/serviz test_specs/bars.json                # bar chart
```

## Tests

```bash
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

| Test | Coverage |
|------|----------|
| `test_colormap` | LUT lookup, name parsing, clamping, endpoints |
| `test_transform` | Data-to-pixel roundtrips, boundary mapping, y-axis inversion |
| `test_palette` | Distinct colors, alpha, wrap-around |
| `test_spec` | Valid specs, mark types (line/point/bar/histogram/candle), bucket parsing, bucket validation, error rejection |
| `test_data` | Column inference, positional fallback, series splitting, color column, extras, sorting, case insensitivity, histogram binning, OHLC aggregation |
| `test_axes` | Numeric ticks (basic, small range, negative), time ticks (seconds, hours, years) |

## Project structure

```
src/
  main.c           Entry point, event loop, pan/zoom, screenshot
  spec.c/h         JSON spec parsing
  data.c/h         DuckDB SQL execution, column inference, series splitting, OHLC/histogram aggregation
  axes.c/h         Tick generation (numeric + time-aware) and axis rendering
  render.c/h       CPU rasterisation (lines, points, bars, candles)
  tooltip.c/h      Nearest-point hit testing and tooltip rendering
  transform.h      Inline linear data-to-pixel transforms
  colormap.c/h     viridis/inferno/plasma/turbo 256-entry LUTs
  palette.c/h      Tableau 10 categorical color palette
third_party/
  duckdb/          Vendored DuckDB amalgamation (not in repo — run download_deps.py)
  cjson/           Vendored cJSON
tests/             C unit tests
scripts/           Dependency download
test_specs/        Example spec files
```
