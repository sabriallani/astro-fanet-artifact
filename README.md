# ASTRO-FANET ns-3 Artifact

This repository contains the executable ns-3 artifact used to explore and demonstrate the ASTRO-FANET routing prototype, together with a buildable NetAnim viewer workflow and the LaTeX paper sources.

This artifact is best understood as an `ns-3 prototype and visualization package` for ASTRO-FANET. It is suitable for code inspection, local execution, trace generation, and video demonstrations on macOS. It is not presented here as a strict one-click reproduction of the full CupCarbon campaign described in the manuscript.

## Repository Layout

- `ns-allinone-3.29/ns-3.29/`: ns-3.29 source tree with the custom ASTRO-FANET module and simulation scenario.
- `ns-allinone-3.29/netanim-3.108/`: NetAnim source tree used to visualize generated `.anim.xml` traces.
- `paper/`: LaTeX manuscript sources and figures.
- `docs/`: installation, execution, video demo, and limitations notes.

## Main Entry Points

- Simulation scenario:
  [scratch/astro-fanet-sim.cc](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc)
- Custom ASTRO-FANET module:
  [src/astro-fanet](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/ns-allinone-3.29/ns-3.29/src/astro-fanet)
- Video demo launcher:
  [run-astro-video-demo.sh](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/ns-allinone-3.29/ns-3.29/run-astro-video-demo.sh)

## Quick Start

For macOS:

1. Read [docs/INSTALL_MACOS.md](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/docs/INSTALL_MACOS.md)
2. Read [docs/RUN_SIMULATION.md](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/docs/RUN_SIMULATION.md)
3. For video capture, use [docs/VIDEO_DEMO.md](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/docs/VIDEO_DEMO.md)

Minimal simulation run:

```bash
cd ns-allinone-3.29/ns-3.29
./waf configure --disable-werror
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=results"
```

Video demo run:

```bash
cd ns-allinone-3.29/ns-3.29
./run-astro-video-demo.sh
```

## What This Artifact Demonstrates

- ASTRO-FANET prototype routing logic inside ns-3
- 3D UAV mobility with Gauss-Markov or RPGM configurations
- packet delivery, delay, throughput, AoI, broadcast suppression, and energy-related outputs
- NetAnim export for visual inspection and video recording

## Important Scope Note

The manuscript describes a CupCarbon-centered evaluation workflow with offline MAPPO training and precomputed SLM embeddings. The code in this repository executes an ns-3 prototype of the same overall idea. Some parts of the learning and representation pipeline are simplified in the current artifact. The exact differences are documented in [docs/LIMITATIONS.md](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/docs/LIMITATIONS.md).

## License

This repository includes ns-3 and NetAnim source components distributed under GPLv2-compatible terms. See [LICENSE](/Users/sabriallani/Documents/Claude/Projects/Paper%20Drones/Drone%20paper/astro-fanet-artifact/LICENSE) and preserve upstream notices when redistributing.
