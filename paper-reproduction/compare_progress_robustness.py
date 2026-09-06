#!/usr/bin/env python3
"""Summarize paired robustness CSVs by topology, mobility, and Byzantine rate."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


def load(root):
    groups = defaultdict(lambda: defaultdict(list))
    for path in root.rglob("*.csv"):
        with path.open(newline="") as handle:
            row = next(csv.DictReader(handle))
        key = (row["nUavs"], row["mobility"], row["byzFraction"])
        groups[key]["pdr"].append(float(row["pdr"]))
        groups[key]["delay"].append(float(row["avgDelay"]))
        groups[key]["broadcasts"].append(float(row["broadcasts"]))
        groups[key]["suppressed"].append(float(row["suppressed"]))
    return groups


def summary(values):
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
    ref, exp = load(args.reference), load(args.experimental)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "nUavs", "mobility", "byzFraction", "variant", "runs",
            "pdr_mean", "pdr_ci95", "avgDelay_mean", "avgDelay_ci95",
            "broadcasts_mean", "suppressed_mean",
        ])
        for key in sorted(set(ref) | set(exp)):
            for label, groups in (("reference", ref), ("experimental", exp)):
                if key not in groups:
                    continue
                pdr, pdr_ci = summary(groups[key]["pdr"])
                delay, delay_ci = summary(groups[key]["delay"])
                broadcasts = sum(groups[key]["broadcasts"]) / len(groups[key]["broadcasts"])
                suppressed = sum(groups[key]["suppressed"]) / len(groups[key]["suppressed"])
                writer.writerow([
                    *key, label, len(groups[key]["pdr"]), f"{pdr:.6f}",
                    f"{pdr_ci:.6f}", f"{delay:.6f}", f"{delay_ci:.6f}",
                    f"{broadcasts:.6f}", f"{suppressed:.6f}",
                ])


if __name__ == "__main__":
    main()
