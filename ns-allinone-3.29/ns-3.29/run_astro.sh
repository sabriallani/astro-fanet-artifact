#!/bin/bash
# ASTRO-FANET NS-3 Simulation Runner
# Handles Python 2/3 compatibility automatically
#
# Usage:
#   ./run_astro.sh                                    # Default: N=10, astro, gm3d, seed=1001, 600s
#   ./run_astro.sh --nUavs=30 --simTime=120           # Custom parameters
#   ./run_astro.sh --protocol=aodv --nUavs=20         # Run AODV baseline
#   ./run_astro.sh configure                          # Just configure (first time)
#   ./run_astro.sh build                              # Just build
#
# Available protocols: astro, aodv, olsr, epidemic, dqn
# Available mobility:  gm3d, rpgm

set -e
cd "$(dirname "$0")"

# --- Fix Python compatibility ---
# NS-3 v3.29 waf needs 'python' in PATH; modern systems only have 'python3'
if ! command -v python &>/dev/null && command -v python3 &>/dev/null; then
    echo "[INFO] Creating temporary 'python' symlink -> python3"
    TMPBIN=$(mktemp -d)
    ln -s "$(which python3)" "$TMPBIN/python"
    export PATH="$TMPBIN:$PATH"
    trap "rm -rf $TMPBIN" EXIT
fi

# --- Handle commands ---
if [ "$1" = "configure" ]; then
    echo "[*] Configuring NS-3..."
    python waf configure
    echo "[OK] Configure done. Now run: ./run_astro.sh build"
    exit 0
fi

if [ "$1" = "build" ]; then
    echo "[*] Building NS-3 (this takes a few minutes the first time)..."
    python waf build
    echo "[OK] Build done. Now run: ./run_astro.sh"
    exit 0
fi

# --- Check if configured/built ---
if [ ! -f build/build-status.py ]; then
    echo "[*] First run - configuring NS-3..."
    python waf configure
fi

if [ ! -f build/scratch/astro-fanet-sim ]; then
    echo "[*] Building NS-3 (first time takes ~2 minutes)..."
    python waf build
fi

# --- Default parameters (Table 2 of the paper) ---
ARGS="--nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=600"

# Override with user arguments if provided
if [ $# -gt 0 ]; then
    ARGS="$@"
fi

echo "============================================"
echo " ASTRO-FANET Simulation"
echo " Args: $ARGS"
echo "============================================"
python waf --run "astro-fanet-sim $ARGS"
