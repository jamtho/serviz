#!/usr/bin/env python3
"""Download vendored dependencies: DuckDB amalgamation and cJSON.

Usage:
    python scripts/download_deps.py [--force]

Options:
    --force  Re-download even if files already present
"""

from __future__ import annotations

import argparse
import io
import sys
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path


DUCKDB_VERSION = "v1.2.1"
CJSON_VERSION = "v1.7.18"

MAX_RETRIES = 3
RETRY_DELAY_S = 2

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def fetch_url(url: str, retries: int = MAX_RETRIES) -> bytes:
    """Fetch a URL with retry on transient errors. Raises on final failure."""
    last_err: Exception | None = None
    for attempt in range(1, retries + 1):
        try:
            resp = urllib.request.urlopen(url, timeout=60)
            return resp.read()
        except (urllib.error.URLError, urllib.error.HTTPError, OSError) as e:
            last_err = e
            if attempt < retries:
                print(f"  Attempt {attempt}/{retries} failed: {e}", file=sys.stderr)
                time.sleep(RETRY_DELAY_S)
    raise RuntimeError(f"Failed to download {url} after {retries} attempts: {last_err}")


def download_duckdb(force: bool = False) -> None:
    """Download DuckDB amalgamation (duckdb.cpp + duckdb.h + duckdb.hpp)."""
    dest = PROJECT_ROOT / "third_party" / "duckdb"
    dest.mkdir(parents=True, exist_ok=True)

    if not force and (dest / "duckdb.cpp").exists() and (dest / "duckdb.h").exists():
        print(f"DuckDB already present at {dest}")
        return

    url = (
        f"https://github.com/duckdb/duckdb/releases/download/{DUCKDB_VERSION}"
        f"/libduckdb-src.zip"
    )
    print(f"Downloading DuckDB {DUCKDB_VERSION} amalgamation...")
    data = fetch_url(url)

    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for name in zf.namelist():
            if name.endswith((".cpp", ".h", ".hpp")):
                fname = Path(name).name
                target = dest / fname
                target.write_bytes(zf.read(name))
                print(f"  Extracted {fname}")

    print("DuckDB amalgamation downloaded.")


def download_cjson(force: bool = False) -> None:
    """Download cJSON source files."""
    dest = PROJECT_ROOT / "third_party" / "cjson"
    dest.mkdir(parents=True, exist_ok=True)

    if not force and (dest / "cJSON.c").exists() and (dest / "cJSON.h").exists():
        print(f"cJSON already present at {dest}")
        return

    base_url = (
        f"https://raw.githubusercontent.com/DaveGamble/cJSON/{CJSON_VERSION}"
    )

    for filename in ("cJSON.c", "cJSON.h"):
        url = f"{base_url}/{filename}"
        print(f"Downloading {filename}...")
        data = fetch_url(url)
        (dest / filename).write_bytes(data)

    print("cJSON downloaded.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Download vendored dependencies.")
    parser.add_argument("--force", action="store_true",
                        help="Re-download even if files already present")
    args = parser.parse_args()

    try:
        download_duckdb(force=args.force)
        download_cjson(force=args.force)
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print("\nAll dependencies downloaded. You can now build with CMake.")


if __name__ == "__main__":
    main()
