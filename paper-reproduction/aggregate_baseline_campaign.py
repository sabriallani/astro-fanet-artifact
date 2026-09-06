#!/usr/bin/env python3
"""Aggregate the baseline comparison campaign into per-cell statistics.

Reads every per-run ``metrics.csv`` produced by ``astro-fanet-sim`` under the
campaign results directory and emits:

* a raw CSV with one row per successful run;
* a summary CSV with one row per (protocol, nUavs, mobility) cell carrying the
  mean, sample standard deviation, 95 % confidence half-width and the number of
  runs actually aggregated.

Only runs that produced a parsable CSV are counted.  Missing or crashed runs are
reported on stderr and excluded -- they are never silently replaced.

Deliberately dependency-free (no pandas/numpy): the CI image only guarantees the
standard library.
"""

from __future__ import annotations

import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

# Student t critical values (two-sided, 95 %) for small samples, indexed by
# degrees of freedom.  Using the normal 1.96 with n=5 would understate the
# interval, which is exactly the overclaiming the manuscript must avoid.
T_CRITICAL_95 = {
    1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571,
    6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
    11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131,
    16: 2.120, 17: 2.110, 18: 2.101, 19: 2.093, 20: 2.086,
}

# Metrics aggregated per cell.  These are exactly the quantities the manuscript
# is allowed to plot or tabulate for the comparison.
METRICS = [
    "pdr",
    "avgDelay",
    "throughput",
    "avgAoI",
    "ctrlOverhead",
    "energyPerBit",
    "brr",
    "broadcasts",
    "suppressed",
    "totalEnergy",
    "redundancyRatio",
    "savedRebroadcastRatio",
    "broadcastPathLength",
    "emergencyNonSuppressionRate",
    "dataReceptions",
    "duplicateReceptions",
    "rebroadcasts",
    "rebroadcastSuppressions",
]

KEY_FIELDS = ("protocol", "nUavs", "mobility")


def t_critical(df: int) -> float:
    """Two-sided 95 % Student t value, falling back to the normal limit."""
    if df <= 0:
        return float("nan")
    return T_CRITICAL_95.get(df, 1.96)


def load_runs(results_dir: Path) -> list[dict]:
    """Load every per-run metrics CSV under *results_dir*."""
    runs: list[dict] = []
    for csv_path in sorted(results_dir.rglob("*.csv")):
        try:
            with csv_path.open(newline="") as handle:
                rows = list(csv.DictReader(handle))
        except OSError as exc:
            print(f"WARN unreadable {csv_path}: {exc}", file=sys.stderr)
            continue
        if not rows:
            print(f"WARN empty {csv_path}", file=sys.stderr)
            continue
        for row in rows:
            if not row.get("protocol"):
                print(f"WARN no protocol column in {csv_path}", file=sys.stderr)
                continue
            row["__source"] = str(csv_path)
            runs.append(row)
    return runs


def to_float(value: str | None) -> float | None:
    if value is None or value == "":
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def summarise(runs: list[dict]) -> list[dict]:
    """Group runs per cell and compute mean / sd / 95 % CI half-width."""
    cells: dict[tuple, list[dict]] = defaultdict(list)
    for row in runs:
        cells[tuple(row.get(field, "") for field in KEY_FIELDS)].append(row)

    summary: list[dict] = []
    for key in sorted(cells):
        group = cells[key]
        record: dict[str, object] = dict(zip(KEY_FIELDS, key))
        record["nRuns"] = len(group)
        record["seeds"] = ";".join(sorted(r.get("seed", "?") for r in group))

        for metric in METRICS:
            values = [v for v in (to_float(r.get(metric)) for r in group) if v is not None]
            if not values:
                record[f"{metric}_mean"] = ""
                record[f"{metric}_sd"] = ""
                record[f"{metric}_ci95"] = ""
                record[f"{metric}_n"] = 0
                continue
            mean = statistics.fmean(values)
            record[f"{metric}_mean"] = f"{mean:.6f}"
            record[f"{metric}_n"] = len(values)
            if len(values) > 1:
                sd = statistics.stdev(values)
                half = t_critical(len(values) - 1) * sd / math.sqrt(len(values))
                record[f"{metric}_sd"] = f"{sd:.6f}"
                record[f"{metric}_ci95"] = f"{half:.6f}"
            else:
                # A single run has no dispersion estimate; say so rather than
                # printing a zero-width interval.
                record[f"{metric}_sd"] = ""
                record[f"{metric}_ci95"] = ""
        summary.append(record)
    return summary


def write_csv(path: Path, rows: list[dict], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--raw-output", required=True, type=Path)
    args = parser.parse_args()

    if not args.results.is_dir():
        print(f"ERROR results directory not found: {args.results}", file=sys.stderr)
        return 1

    runs = load_runs(args.results)
    if not runs:
        print(f"ERROR no parsable run CSV under {args.results}", file=sys.stderr)
        return 1

    raw_fields = [k for k in runs[0] if k != "__source"] + ["__source"]
    write_csv(args.raw_output, runs, raw_fields)

    summary = summarise(runs)
    summary_fields = list(KEY_FIELDS) + ["nRuns", "seeds"]
    for metric in METRICS:
        summary_fields += [f"{metric}_mean", f"{metric}_sd", f"{metric}_ci95", f"{metric}_n"]
    write_csv(args.output, summary, summary_fields)

    print(f"Aggregated {len(runs)} runs into {len(summary)} cells")
    for record in summary:
        print(
            f"  {record['protocol']:>5} n={record['nUavs']:>2} {record['mobility']:>4} "
            f"runs={record['nRuns']} pdr={record.get('pdr_mean', '')} "
            f"ci={record.get('pdr_ci95', '')}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
