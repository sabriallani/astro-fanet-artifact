#!/usr/bin/env python3
"""Run paper-aligned A3D-BSM ns-3 campaigns.

This runner uses the public artifact protocol mode `astro`, which is the mode
that exercises the A3D-BSM implementation. It intentionally does not claim to
run the manuscript baselines that are not exposed by the current scenario.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import shlex
import subprocess
from collections import defaultdict
from pathlib import Path


N_UAVS = [20, 40, 60, 80]
MOBILITIES = ["gm3d", "rpgm"]
SEEDS = list(range(3001, 3011))
BYZANTINE_FRACTIONS = [0.0, 0.1, 0.2, 0.3]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def ns3_dir() -> Path:
    return repo_root() / "ns-allinone-3.29" / "ns-3.29"


def build_command(n_uavs: int, mobility: str, seed: int, sim_time: float,
                  pkt_rate: float, output_dir: str, byz_fraction: float = 0.0) -> list[str]:
    scenario = (
        "astro-fanet-sim "
        "--protocol=astro "
        f"--nUavs={n_uavs} "
        f"--mobility={mobility} "
        f"--seed={seed} "
        f"--simTime={sim_time} "
        f"--pktRate={pkt_rate} "
        f"--byzFraction={byz_fraction} "
        f"--outputDir={output_dir}"
    )
    return ["./waf", "--run", scenario]


def parse_run_parameters(command: list[str]) -> dict[str, object]:
    """Extract the ns-3 parameters from one generated command."""
    values: dict[str, object] = {"command": command}
    for token in shlex.split(command[-1]):
        if not token.startswith("--") or "=" not in token:
            continue
        key, value = token[2:].split("=", 1)
        if key in {"nUavs", "seed"}:
            values[key] = int(value)
        elif key in {"simTime", "pktRate", "byzFraction"}:
            values[key] = float(value)
        else:
            values[key] = value
    return values


def build_manifests(tasks: list[list[str]], cwd: Path) -> dict[str, dict[str, object]]:
    """Build deterministic, per-output-directory run manifests."""
    grouped: dict[str, list[dict[str, object]]] = defaultdict(list)
    for task in tasks:
        parameters = parse_run_parameters(task)
        output_dir = str(parameters["outputDir"])
        grouped[output_dir].append(parameters)

    return {
        output_dir: {
            "schemaVersion": 1,
            "workingDirectory": str(cwd),
            "runs": runs,
        }
        for output_dir, runs in sorted(grouped.items())
    }


def write_manifests(tasks: list[list[str]], cwd: Path) -> list[Path]:
    """Write one manifest before execution and return the created paths."""
    paths: list[Path] = []
    for output_dir, manifest in build_manifests(tasks, cwd).items():
        manifest_path = cwd / output_dir / "run-manifest.json"
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        paths.append(manifest_path)
    return paths


def run_command(command: list[str], cwd: Path, dry_run: bool) -> bool:
    printable = " ".join(command)
    if dry_run:
        print(printable)
        return True

    result = subprocess.run(command, cwd=str(cwd), text=True)
    return result.returncode == 0


def configure_and_build(cwd: Path, dry_run: bool, skip_build: bool) -> bool:
    if skip_build:
        return True

    commands = [
        ["./waf", "configure", "--disable-werror"],
        ["./waf", "build"],
    ]
    for command in commands:
        if not run_command(command, cwd, dry_run):
            return False
    return True


def main_tasks(sim_time: float, pkt_rate: float, output_dir: str) -> list[list[str]]:
    tasks = []
    for n_uavs in N_UAVS:
        for mobility in MOBILITIES:
            for seed in SEEDS:
                tasks.append(build_command(n_uavs, mobility, seed, sim_time,
                                           pkt_rate, output_dir, 0.0))
    return tasks


def byzantine_tasks(sim_time: float, pkt_rate: float, output_dir: str) -> list[list[str]]:
    tasks = []
    for byz_fraction in BYZANTINE_FRACTIONS:
        for seed in SEEDS:
            tasks.append(build_command(60, "gm3d", seed, sim_time, pkt_rate,
                                       output_dir, byz_fraction))
    return tasks


def smoke_tasks(output_dir: str) -> list[list[str]]:
    return [build_command(20, "gm3d", 3001, 10.0, 1.0, output_dir, 0.0)]


def run_tasks(tasks: list[list[str]], cwd: Path, parallel: int, dry_run: bool) -> int:
    if dry_run or parallel <= 1:
        failures = 0
        for task in tasks:
            failures += 0 if run_command(task, cwd, dry_run) else 1
        return failures

    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel) as executor:
        futures = [executor.submit(run_command, task, cwd, False) for task in tasks]
        for future in concurrent.futures.as_completed(futures):
            if not future.result():
                failures += 1
    return failures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run paper-aligned A3D-BSM campaigns")
    parser.add_argument("--campaign", choices=["smoke", "main", "byzantine", "all"],
                        default="smoke")
    parser.add_argument("--sim-time", type=float, default=120.0)
    parser.add_argument("--pkt-rate", type=float, default=1.0)
    parser.add_argument("--parallel", type=int, default=1)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--output-dir", default=None,
                        help="Output directory relative to ns-3.29")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cwd = ns3_dir()
    if not cwd.exists():
        raise SystemExit(f"ns-3 directory not found: {cwd}")

    if not configure_and_build(cwd, args.dry_run, args.skip_build):
        return 1

    selected_tasks: list[list[str]] = []
    if args.campaign == "smoke":
        output_dir = args.output_dir or "results/paper-a3dbsm-smoke"
        selected_tasks.extend(smoke_tasks(output_dir))
    if args.campaign in ("main", "all"):
        output_dir = args.output_dir or "results/paper-a3dbsm"
        selected_tasks.extend(main_tasks(args.sim_time, args.pkt_rate, output_dir))
    if args.campaign in ("byzantine", "all"):
        output_dir = args.output_dir or "results/paper-a3dbsm-byzantine"
        selected_tasks.extend(byzantine_tasks(args.sim_time, args.pkt_rate, output_dir))

    print(f"Selected {len(selected_tasks)} run(s)")
    if not args.dry_run:
        manifest_paths = write_manifests(selected_tasks, cwd)
        for manifest_path in manifest_paths:
            print(f"Wrote run manifest: {manifest_path}")
    failures = run_tasks(selected_tasks, cwd, args.parallel, args.dry_run)
    if failures:
        print(f"Completed with {failures} failed run(s)")
        return 1

    print("Campaign completed successfully")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
