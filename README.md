# A3D-BSM ns-3 Artifact

**ns-3 simulation artifact for the paper**  
**“A3D-BSM: Priority-Aware 3D Broadcast Suppression for Distributed UAV Networks with Insider-Resilient Coordination”**

**Author:** Sabri Allani  
**Repository:** [sabriallani/astro-fanet-artifact](https://github.com/sabriallani/astro-fanet-artifact)

## Overview

This repository contains the executable ns-3 simulation code used to study **A3D-BSM**, a packet-level broadcast suppression mechanism for Flying Ad Hoc Networks (FANETs) that combines:

- adaptive **3D ellipsoidal broadcast suppression**
- a priority-aware Emergency override
- authenticated intent exchange with trust-aware neighbor filtering
- measured packet delivery, rebroadcast reduction, delay, energy, and control-overhead metrics

The repository is intended as the **code companion and executable artifact** for the paper titled:

> **A3D-BSM: Priority-Aware 3D Broadcast Suppression for Distributed UAV Networks with Insider-Resilient Coordination**

In practical terms, this repository lets a reader:

- inspect the implementation of the A3D-BSM suppression layer and its ns-3 integration
- run the simulation locally in **ns-3**
- generate numerical outputs in `.csv`
- export visual traces for **NetAnim**
- create demonstration videos of the FANET scenario

## Relation To The Paper

This codebase is provided in the context of the above paper as the **simulation artifact associated with the manuscript**. The paper's main contribution is the A3D-BSM broadcast-suppression mechanism; older ASTRO-FANET routing helpers are retained only as executable ns-3 scaffolding around that mechanism.

The repository maps the paper's main technical blocks to executable code:

- **A3D-BSM broadcast suppression**: `src/astro-fanet/model/a3d-bsm.cc`
- **Trust / coordination support**: `src/astro-fanet/model/trust-manager.cc`
- **Routing and ns-3 integration scaffold**: `src/astro-fanet/model/astro-routing-protocol.cc`
- **End-to-end scenario configuration**: `scratch/astro-fanet-sim.cc`

The A3D-BSM calibration used in this artifact is fixed and disclosed in the source code. It does **not** depend on an online LLM, SLM, or trained neural policy at runtime.

The paper explains the method at the algorithmic level; this repository shows how the simulation is assembled and executed in practice.

## What The Simulation Does

At runtime, the simulation performs the following high-level workflow:

1. It creates a FANET scenario with UAV nodes and a sink.
2. It assigns a mobility model such as **Gauss-Markov 3D** or **RPGM**.
3. It configures IEEE `802.11a` ad hoc communication.
4. It generates heterogeneous traffic classes such as emergency, command, sensing, and telemetry packets.
5. It activates the A3D-BSM-enabled custom mode or a baseline protocol.
6. It records packet delivery, delay, throughput, AoI, control overhead, suppression activity, and energy-related counters.
7. It optionally exports a **NetAnim** trace for visual playback.

The main executable scenario is:

- [`ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc`](ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc)

## How A3D-BSM Is Implemented In This Repository

The implementation is organized around a custom `astro-fanet` ns-3 module.

### 1. Scenario Layer

The scenario file initializes:

- number of UAVs
- mobility model
- wireless network
- traffic generation
- energy accounting
- simulation outputs
- animation export

Main file:

- [`ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc`](ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc)

### 2. Routing Layer

The routing protocol is implemented in:

- [`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/astro-routing-protocol.cc`](ns-allinone-3.29/ns-3.29/src/astro-fanet/model/astro-routing-protocol.cc)

This layer is responsible for the ns-3 integration around A3D-BSM:

- beacon handling
- neighbor-table maintenance
- route input/output logic
- forwarding decisions
- broadcast handling
- collection of delivery, rebroadcast, energy, and overhead metrics

### 3. Broadcast Suppression Layer

The adaptive 3D suppression logic is implemented in:

- [`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/a3d-bsm.cc`](ns-allinone-3.29/ns-3.29/src/astro-fanet/model/a3d-bsm.cc)

Its role is to reduce redundant rebroadcasts in dense 3D FANET scenarios.

### 4. Trust Support Layer

Trust-aware coordination support is implemented in:

- [`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/trust-manager.cc`](ns-allinone-3.29/ns-3.29/src/astro-fanet/model/trust-manager.cc)

This module supports authenticated coordination and behavior scoring in the simulation pipeline.

### 5. Legacy Prototype Helpers

The repository still contains earlier prototype files such as:

- [`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/mappo-agent.cc`](ns-allinone-3.29/ns-3.29/src/astro-fanet/model/mappo-agent.cc)
- [`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/slm-emulator.cc`](ns-allinone-3.29/ns-3.29/src/astro-fanet/model/slm-emulator.cc)

These files are retained to keep the older executable scaffold buildable. They should not be interpreted as the A3D-BSM calibration method or as a claimed learning contribution of the current manuscript.

## Repository Structure

- [`ns-allinone-3.29/ns-3.29/`](ns-allinone-3.29/ns-3.29): ns-3 source tree containing the ASTRO-FANET module and runnable scenario
- [`ns-allinone-3.29/netanim-3.108/`](ns-allinone-3.29/netanim-3.108): NetAnim viewer source used to replay generated traces
- [`docs/INSTALL_MACOS.md`](docs/INSTALL_MACOS.md): macOS installation notes
- [`docs/RUN_SIMULATION.md`](docs/RUN_SIMULATION.md): simulation commands and parameters
- [`docs/VIDEO_DEMO.md`](docs/VIDEO_DEMO.md): video and NetAnim workflow
- [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md): current scope and artifact limitations
- [`docs/REPOSITORY_SCOPE.md`](docs/REPOSITORY_SCOPE.md): explanation of what is intentionally included in the repository
- [`paper-reproduction/`](paper-reproduction): paper-aligned A3D-BSM campaign scripts, coverage matrix, and editor/reviewer reproducibility notes

## Quick Start

### Standard Simulation Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf configure --disable-werror
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=results"
```

This run produces:

- a terminal summary of key metrics
- a `.csv` file in the chosen output directory

### Visual Simulation Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=visual-results --enableAnim=1"
```

This run produces:

- `*.anim.xml` for NetAnim playback
- `*.routes.xml` for route visualization
- `*.csv` for numerical outputs

### One-Command Video Demo

```bash
cd ns-allinone-3.29/ns-3.29
./run-astro-video-demo.sh
```

Demo launcher:

- [`ns-allinone-3.29/ns-3.29/run-astro-video-demo.sh`](ns-allinone-3.29/ns-3.29/run-astro-video-demo.sh)

### Paper-Aligned A3D-BSM Campaign

The `paper-reproduction/` folder contains scripts aligned with the manuscript's
A3D-BSM settings: `N = 20, 40, 60, 80`, GM3D/RPGM mobility, seeds
`3001--3010`, 120-second runs, and the gray-hole fraction sweep
`0, 0.1, 0.2, 0.3`.

Preview the commands without running ns-3:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all --dry-run
```

Run the paper-aligned A3D-BSM campaign:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all
```

See [`paper-reproduction/reproducibility_status.md`](paper-reproduction/reproducibility_status.md)
for the exact coverage matrix and current gaps relative to every manuscript
baseline.

## Main Simulation Parameters

The current executable scenario supports parameters such as:

- `--nUavs`
- `--protocol`
- `--mobility`
- `--seed`
- `--simTime`
- `--outputDir`
- `--enableAnim`
- `--videoMode`

For full examples, see:

- [`docs/RUN_SIMULATION.md`](docs/RUN_SIMULATION.md)

## Available Protocol Modes

The scenario currently exposes the following protocol selections in the executable artifact:

- `astro`
- `aodv`
- `olsr`
- `epidemic`
- `dqn`

The `astro` mode is the custom ns-3 mode that exercises the A3D-BSM layer. The others are included as comparison modes inside the current simulation codebase.

## Output Metrics

The simulation reports metrics such as:

- **PDR**: packet delivery ratio
- **Average end-to-end delay**
- **Throughput**
- **AoI**: Age of Information
- **Control overhead**
- **Broadcast suppression activity**
- **Total energy**
- **Energy per useful bit**

These metrics are printed to the terminal and written to `.csv` files for post-processing.

## How To Read The Results

The artifact is most useful for:

- understanding how A3D-BSM behaves under different FANET densities
- comparing ASTRO mode with classical baseline modes
- studying the impact of mobility choice such as `gm3d` vs `rpgm`
- generating traces and videos for visual inspection

The repository therefore supports both:

- **numerical analysis**, through CSV outputs
- **visual analysis**, through NetAnim traces

## Visualization Workflow

For visual replay:

1. run the scenario with `--enableAnim=1`
2. open the generated `.anim.xml` file with NetAnim
3. inspect node movement, relays, and scenario behavior

See:

- [`docs/VIDEO_DEMO.md`](docs/VIDEO_DEMO.md)

## Scope And Positioning

This repository should be understood as:

- the **GitHub code companion** for the A3D-BSM paper
- an **ns-3 executable simulation artifact**
- a **reference implementation for local execution and visual demonstration**
- a **paper-aligned A3D-BSM campaign artifact**

It should not be overstated as a fully frozen end-to-end reproduction package
for every plot in the manuscript. The current implementation is best described
as a practical and inspectable research artifact for the A3D-BSM simulation
study, with explicit reproduction-scope notes in `paper-reproduction/`.

For current limitations, see:

- [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md)

## Citation-Style Reference

If you refer to this repository in connection with the manuscript, use the paper title:

> **Sabri Allani, “A3D-BSM: Priority-Aware 3D Broadcast Suppression for Distributed UAV Networks with Insider-Resilient Coordination.”**

## License

This repository contains upstream `ns-3` and `NetAnim` source components together with custom ASTRO-FANET code. Please preserve upstream notices and consult:

- [`LICENSE`](LICENSE)
