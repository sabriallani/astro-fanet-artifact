#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NETANIM_DIR="$SCRIPT_DIR/../netanim-3.108"
OUTPUT_DIR="${1:-video-demo}"
N_UAVS="${2:-12}"
SIM_TIME="${3:-30}"
SEED="${4:-1001}"
OPEN_GUI="${OPEN_GUI:-1}"

cd "$SCRIPT_DIR"

./waf configure --disable-werror >/dev/null

RUN_ARGS="astro-fanet-sim --nUavs=${N_UAVS} --protocol=astro --mobility=gm3d --seed=${SEED} --simTime=${SIM_TIME} --pktRate=1.25 --outputDir=${OUTPUT_DIR} --videoMode=1"

./waf --run "$RUN_ARGS"

ANIM_FILE="$SCRIPT_DIR/${OUTPUT_DIR}/astro_n${N_UAVS}_gm3d_s${SEED}.anim.xml"
ROUTES_FILE="$SCRIPT_DIR/${OUTPUT_DIR}/astro_n${N_UAVS}_gm3d_s${SEED}.routes.xml"

echo
echo "Animation XML : $ANIM_FILE"
echo "Routes XML    : $ROUTES_FILE"

if [[ "$OPEN_GUI" == "1" ]]; then
  "$NETANIM_DIR/NetAnim" "$ANIM_FILE"
fi
