#!/usr/bin/env python3
"""Compare paired reference and experimental ns-3 CSV outputs."""
import argparse
import csv
import math
from pathlib import Path


def rows(root: Path):
    for path in root.rglob("*.csv"):
        with path.open(newline="") as handle:
            data = next(csv.DictReader(handle))
        yield data


def stats(values):
    mean = sum(values) / len(values)
    if len(values) < 2:
        return mean, 0.0
    variance = sum((v - mean) ** 2 for v in values) / (len(values) - 1)
    return mean, 1.96 * math.sqrt(variance / len(values))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--experimental", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    grouped = {}
    for label, root in (("reference", args.reference), ("experimental", args.experimental)):
        for row in rows(root):
            key = (row["nUavs"], row["mobility"])
            grouped.setdefault(key, {}).setdefault(label, []).append(row)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "nUavs", "mobility", "variant", "runs", "pdr_mean", "pdr_ci95",
            "avgDelay_mean", "avgDelay_ci95", "broadcasts_mean", "suppressed_mean",
        ])
        for key in sorted(grouped):
            for label in ("reference", "experimental"):
                data = grouped[key].get(label, [])
                if not data:
                    continue
                pdr, pdr_ci = stats([float(row["pdr"]) for row in data])
                delay, delay_ci = stats([float(row["avgDelay"]) for row in data])
                broadcasts = sum(float(row["broadcasts"]) for row in data) / len(data)
                suppressed = sum(float(row["suppressed"]) for row in data) / len(data)
                writer.writerow([
                    key[0], key[1], label, len(data), f"{pdr:.6f}", f"{pdr_ci:.6f}",
                    f"{delay:.6f}", f"{delay_ci:.6f}", f"{broadcasts:.6f}",
                    f"{suppressed:.6f}",
                ])


if __name__ == "__main__":
    main()
