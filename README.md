# ASTRO-FANET ns-3 Artifact

This repository contains the executable ns-3 artifact used to explore and demonstrate the ASTRO-FANET routing prototype, together with a buildable NetAnim viewer workflow.

This artifact is an `ns-3 prototype and visualization package` for ASTRO-FANET. It is suitable for code inspection, local execution, trace generation, and video demonstrations on macOS.

## Repository Layout

- `ns-allinone-3.29/ns-3.29/`: ns-3.29 source tree with the custom ASTRO-FANET module and simulation scenario.
- `ns-allinone-3.29/netanim-3.108/`: NetAnim source tree used to visualize generated `.anim.xml` traces.
- `docs/`: installation, execution, video demo, and limitations notes.

## Main Entry Points

- Simulation scenario:
  [`ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc`](ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc)
- Custom ASTRO-FANET module:
  [`ns-allinone-3.29/ns-3.29/src/astro-fanet`](ns-allinone-3.29/ns-3.29/src/astro-fanet)
- Video demo launcher:
  [`ns-allinone-3.29/ns-3.29/run-astro-video-demo.sh`](ns-allinone-3.29/ns-3.29/run-astro-video-demo.sh)

## Quick Start

For macOS:

1. Read [`docs/INSTALL_MACOS.md`](docs/INSTALL_MACOS.md)
2. Read [`docs/RUN_SIMULATION.md`](docs/RUN_SIMULATION.md)
3. For video capture, use [`docs/VIDEO_DEMO.md`](docs/VIDEO_DEMO.md)

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

This repository is centered on the ns-3 implementation and visualization workflow of ASTRO-FANET. Some parts of the learning and representation pipeline remain simplified in the current executable artifact. The current scope and remaining gaps are documented in [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md).

## License

This repository includes ns-3 and NetAnim source components distributed under GPLv2-compatible terms. See [`LICENSE`](LICENSE) and preserve upstream notices when redistributing.
