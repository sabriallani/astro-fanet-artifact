#!/usr/bin/env python3
"""
ASTRO-FANET NS-3 Simulation Campaign Runner
=============================================
Reproduces the evaluation campaign from Section 4 of the paper.

Configurations:
- Protocols: ASTRO-FANET, AODV, OLSR, Epidemic, DQN-QR
- UAV counts: 10, 20, 30, 40
- Mobility: Gauss-Markov 3D (gm3d), RPGM
- Seeds: 1001-1030 (30 independent runs per config)
- Stress scenario: N=10, 25-40 m/s, alpha_GM=0.4
- Byzantine: f_byz = 0.0, 0.1, 0.2

Usage:
  python3 run_campaign.py [--ns3-dir /path/to/ns-3.29] [--parallel 4]
"""

import subprocess
import os
import sys
import argparse
import itertools
from concurrent.futures import ProcessPoolExecutor, as_completed
import time

# ---- Campaign configurations (matching Table 2) ----

PROTOCOLS = ["astro", "aodv", "olsr", "epidemic", "dqn"]
N_UAVS = [10, 20, 30, 40]
MOBILITIES = ["gm3d", "rpgm"]
SEEDS = list(range(1001, 1031))  # 30 seeds
SIM_TIME = 600.0

# Stress scenario (Section 4.6.6)
STRESS_CONFIG = {
    "nUavs": 10,
    "minSpeed": 25.0,
    "maxSpeed": 40.0,
    "gmAlpha": 0.4,
    "mobility": "gm3d",
}

# Byzantine scenarios (Section 4.6.7)
BYZANTINE_FRACTIONS = [0.0, 0.1, 0.2]


def run_single_sim(ns3_dir, protocol, n_uavs, mobility, seed,
                   extra_args=None, output_dir="results"):
    """Run a single NS-3 simulation."""
    cmd = [
        "./waf", "--run",
        f"astro-fanet-sim "
        f"--protocol={protocol} "
        f"--nUavs={n_uavs} "
        f"--mobility={mobility} "
        f"--seed={seed} "
        f"--simTime={SIM_TIME} "
        f"--outputDir={output_dir}"
    ]

    if extra_args:
        cmd[-1] = cmd[-1].rstrip('"') + " " + extra_args

    print(f"  Running: {protocol} N={n_uavs} {mobility} seed={seed}")

    try:
        result = subprocess.run(
            cmd, cwd=ns3_dir, capture_output=True, text=True, timeout=1800
        )
        if result.returncode != 0:
            print(f"  ERROR: {result.stderr[:200]}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print(f"  TIMEOUT: {protocol} N={n_uavs} {mobility} seed={seed}")
        return False
    except Exception as e:
        print(f"  EXCEPTION: {e}")
        return False


def run_main_campaign(ns3_dir, parallel=4):
    """Run the main evaluation campaign (Tables 3-8)."""
    print("\n=== Main Campaign ===")
    os.makedirs(os.path.join(ns3_dir, "results"), exist_ok=True)

    tasks = list(itertools.product(PROTOCOLS, N_UAVS, MOBILITIES, SEEDS))
    total = len(tasks)
    print(f"Total simulations: {total}")

    completed = 0
    failed = 0
    start_time = time.time()

    with ProcessPoolExecutor(max_workers=parallel) as executor:
        futures = {}
        for protocol, n_uavs, mobility, seed in tasks:
            future = executor.submit(
                run_single_sim, ns3_dir, protocol, n_uavs, mobility, seed
            )
            futures[future] = (protocol, n_uavs, mobility, seed)

        for future in as_completed(futures):
            config = futures[future]
            if future.result():
                completed += 1
            else:
                failed += 1
            if (completed + failed) % 50 == 0:
                elapsed = time.time() - start_time
                rate = (completed + failed) / elapsed * 60
                print(f"  Progress: {completed + failed}/{total} "
                      f"({rate:.1f} sims/min)")

    print(f"\nCompleted: {completed}, Failed: {failed}")
    return completed, failed


def run_stress_campaign(ns3_dir, parallel=4):
    """Run stress scenario (Section 4.6.6)."""
    print("\n=== Stress Scenario Campaign ===")

    extra_args = (f"--minSpeed={STRESS_CONFIG['minSpeed']} "
                  f"--maxSpeed={STRESS_CONFIG['maxSpeed']} "
                  f"--gmAlpha={STRESS_CONFIG['gmAlpha']}")

    for protocol in PROTOCOLS:
        for seed in SEEDS:
            run_single_sim(
                ns3_dir, protocol,
                STRESS_CONFIG["nUavs"],
                STRESS_CONFIG["mobility"],
                seed,
                extra_args=extra_args,
                output_dir="results/stress"
            )


def run_byzantine_campaign(ns3_dir, parallel=4):
    """Run Byzantine agent experiment (Section 4.6.7)."""
    print("\n=== Byzantine Campaign ===")

    for byz_frac in BYZANTINE_FRACTIONS:
        for seed in SEEDS:
            extra_args = f"--byzFraction={byz_frac}"
            run_single_sim(
                ns3_dir, "astro", 30, "gm3d", seed,
                extra_args=extra_args,
                output_dir="results/byzantine"
            )


def main():
    parser = argparse.ArgumentParser(description="ASTRO-FANET simulation campaign")
    parser.add_argument("--ns3-dir", default=".",
                        help="Path to ns-3.29 directory")
    parser.add_argument("--parallel", type=int, default=4,
                        help="Number of parallel simulations")
    parser.add_argument("--campaign", choices=["main", "stress", "byzantine", "all"],
                        default="all", help="Which campaign to run")
    args = parser.parse_args()

    ns3_dir = os.path.abspath(args.ns3_dir)
    print(f"NS-3 directory: {ns3_dir}")

    # Build first
    print("\n=== Building NS-3 ===")
    build_result = subprocess.run(
        ["./waf", "build"], cwd=ns3_dir, capture_output=True, text=True
    )
    if build_result.returncode != 0:
        print(f"Build failed:\n{build_result.stderr}")
        sys.exit(1)
    print("Build successful.")

    if args.campaign in ("main", "all"):
        run_main_campaign(ns3_dir, args.parallel)
    if args.campaign in ("stress", "all"):
        run_stress_campaign(ns3_dir, args.parallel)
    if args.campaign in ("byzantine", "all"):
        run_byzantine_campaign(ns3_dir, args.parallel)

    print("\n=== Campaign complete. Run analyze_results.py to generate figures. ===")


if __name__ == "__main__":
    main()
