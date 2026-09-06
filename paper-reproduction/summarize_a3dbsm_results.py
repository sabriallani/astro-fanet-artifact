#!/usr/bin/env python3
"""Summarize CSV outputs produced by run_a3dbsm_paper_campaign.py."""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


T95 = {
    1: 12.706,
    2: 4.303,
    3: 3.182,
    4: 2.776,
    5: 2.571,
    6: 2.447,
    7: 2.365,
    8: 2.306,
    9: 2.262,
    10: 2.228,
    20: 2.086,
    30: 2.042,
}

METRIC_NAMES = ["pdr", "brr", "avgDelay", "ctrlOverhead", "broadcasts", "suppressed"]
REQUIRED_COLUMNS = {
    "protocol", "nUavs", "mobility", "seed", "simTime", "byzFraction",
    *METRIC_NAMES,
}


def t_critical_95(df: int) -> float:
    if df in T95:
        return T95[df]
    if df < 1:
        return 0.0
    return 1.96


def mean_ci95(values: list[float]) -> tuple[float, float]:
    if not values:
        return float("nan"), float("nan")
    mean = sum(values) / len(values)
    if len(values) < 2:
        return mean, 0.0
    variance = sum((value - mean) ** 2 for value in values) / (len(values) - 1)
    stderr = math.sqrt(variance) / math.sqrt(len(values))
    return mean, t_critical_95(len(values) - 1) * stderr


def load_rows(results_dir: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for csv_path in sorted(results_dir.rglob("*.csv")):
        with csv_path.open(newline="") as handle:
            reader = csv.DictReader(handle)
            columns = set(reader.fieldnames or [])
            missing = REQUIRED_COLUMNS - columns
            if missing:
                names = ", ".join(sorted(missing))
                raise ValueError(f"{csv_path}: missing required columns: {names}")
            file_rows = list(reader)
            for row_number, row in enumerate(file_rows, start=2):
                for metric in METRIC_NAMES:
                    value = row.get(metric, "")
                    try:
                        parsed = float(value)
                    except (TypeError, ValueError):
                        raise ValueError(
                            f"{csv_path}:{row_number}: non-numeric value for {metric}: {value!r}"
                        ) from None
                    if not math.isfinite(parsed):
                        raise ValueError(
                            f"{csv_path}:{row_number}: non-finite value for {metric}: {value!r}"
                        )
            rows.extend(file_rows)
    return rows


def numeric(row: dict[str, str], key: str) -> float | None:
    try:
        return float(row[key])
    except (KeyError, TypeError, ValueError):
        return None


def summarize(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    groups: dict[tuple[str, str, str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        key = (
            row.get("protocol", ""),
            row.get("nUavs", ""),
            row.get("mobility", ""),
            row.get("byzFraction", "0"),
        )
        groups[key].append(row)

    metric_names = METRIC_NAMES
    output_rows: list[dict[str, str]] = []
    for key, group_rows in sorted(groups.items()):
        protocol, n_uavs, mobility, byz_fraction = key
        summary = {
            "protocol": protocol,
            "nUavs": n_uavs,
            "mobility": mobility,
            "byzFraction": byz_fraction,
            "runs": str(len(group_rows)),
        }
        for metric in metric_names:
            values = [value for value in (numeric(row, metric) for row in group_rows) if value is not None]
            mean, ci = mean_ci95(values)
            summary[f"{metric}_mean"] = f"{mean:.6g}"
            summary[f"{metric}_ci95"] = f"{ci:.6g}"
        output_rows.append(summary)
    return output_rows


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Summarize A3D-BSM paper campaign CSV files")
    parser.add_argument("--results-dir", required=True)
    parser.add_argument("--output", default="paper-reproduction/a3dbsm_summary.csv")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    results_dir = Path(args.results_dir)
    try:
        rows = load_rows(results_dir)
    except ValueError as error:
        print(f"CSV validation failed: {error}")
        return 1
    if not rows:
        print(f"No CSV files found under {results_dir}")
        return 1

    summary_rows = summarize(rows)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary_rows[0].keys()))
        writer.writeheader()
        writer.writerows(summary_rows)

    print(f"Wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
