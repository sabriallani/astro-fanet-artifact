#!/bin/bash
# ============================================================
# ASTRO-FANET NS-3 Simulation - Build and Run Guide
# ============================================================
# This script builds NS-3 with the ASTRO-FANET module and runs
# the simulation campaign matching the paper's evaluation.
#
# Prerequisites:
#   - GCC 7+ or Clang 5+
#   - Python 2.7+ or 3.5+
#   - pkg-config, libxml2-dev
#
# Usage:
#   chmod +x BUILD_AND_RUN.sh
#   ./BUILD_AND_RUN.sh          # Build + quick test
#   ./BUILD_AND_RUN.sh full     # Build + full campaign
# ============================================================

set -e

NS3_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
echo "NS-3 directory: $NS3_DIR"
cd "$NS3_DIR"

# ---- Step 1: Configure and Build ----
echo ""
echo "=== Step 1: Configuring NS-3 ==="
./waf configure --enable-examples --enable-tests

echo ""
echo "=== Step 2: Building NS-3 ==="
./waf build

echo ""
echo "=== Build successful! ==="

# ---- Step 2: Quick test run ----
echo ""
echo "=== Step 3: Quick test (single run) ==="
mkdir -p results

# Single ASTRO-FANET run
echo "Running ASTRO-FANET (N=10, GM3D, seed=1001)..."
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=60 --outputDir=results"

echo ""
echo "=== Quick test passed! ==="

# ---- Step 3: Full campaign (optional) ----
if [ "${1}" == "full" ]; then
    echo ""
    echo "=== Step 4: Full simulation campaign ==="
    echo "This will take several hours depending on your hardware."
    echo ""

    # Main campaign: all protocols x all N x all mobility x 30 seeds
    PROTOCOLS=("astro" "aodv" "olsr" "epidemic" "dqn")
    N_VALUES=(10 20 30 40)
    MOBILITIES=("gm3d" "rpgm")

    for proto in "${PROTOCOLS[@]}"; do
        for n in "${N_VALUES[@]}"; do
            for mob in "${MOBILITIES[@]}"; do
                for seed in $(seq 1001 1030); do
                    echo "Running: $proto N=$n $mob seed=$seed"
                    ./waf --run "astro-fanet-sim \
                        --protocol=$proto \
                        --nUavs=$n \
                        --mobility=$mob \
                        --seed=$seed \
                        --outputDir=results" 2>/dev/null || true
                done
            done
        done
    done

    # Stress scenario
    echo ""
    echo "=== Stress scenario ==="
    mkdir -p results/stress
    for proto in "${PROTOCOLS[@]}"; do
        for seed in $(seq 1001 1030); do
            ./waf --run "astro-fanet-sim \
                --protocol=$proto \
                --nUavs=10 \
                --mobility=gm3d \
                --seed=$seed \
                --minSpeed=25 \
                --maxSpeed=40 \
                --gmAlpha=0.4 \
                --outputDir=results/stress" 2>/dev/null || true
        done
    done

    # Byzantine experiment
    echo ""
    echo "=== Byzantine experiment ==="
    mkdir -p results/byzantine
    for byz in 0.0 0.1 0.2; do
        for seed in $(seq 1001 1030); do
            ./waf --run "astro-fanet-sim \
                --protocol=astro \
                --nUavs=30 \
                --mobility=gm3d \
                --seed=$seed \
                --byzFraction=$byz \
                --outputDir=results/byzantine" 2>/dev/null || true
        done
    done

    echo ""
    echo "=== Campaign complete! ==="
    echo "Run the analysis script to generate tables and figures:"
    echo "  python3 scratch/astro-scripts/analyze_results.py --results-dir results/"
fi

echo ""
echo "============================================================"
echo "  ASTRO-FANET NS-3 Simulation Ready"
echo "============================================================"
echo ""
echo "Quick commands:"
echo "  # Single run:"
echo "  ./waf --run \"astro-fanet-sim --nUavs=30 --protocol=astro --mobility=gm3d --seed=1001\""
echo ""
echo "  # Compare with AODV:"
echo "  ./waf --run \"astro-fanet-sim --nUavs=30 --protocol=aodv --mobility=gm3d --seed=1001\""
echo ""
echo "  # Stress scenario:"
echo "  ./waf --run \"astro-fanet-sim --nUavs=10 --protocol=astro --minSpeed=25 --maxSpeed=40 --gmAlpha=0.4\""
echo ""
echo "  # Byzantine test:"
echo "  ./waf --run \"astro-fanet-sim --nUavs=30 --protocol=astro --byzFraction=0.2\""
echo ""
echo "  # Full campaign (Python):"
echo "  python3 scratch/astro-scripts/run_campaign.py --ns3-dir . --parallel 4"
echo ""
echo "  # Analyze results:"
echo "  python3 scratch/astro-scripts/analyze_results.py --results-dir results/"
echo ""
