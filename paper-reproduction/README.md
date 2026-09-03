# Paper Reproduction Notes

This folder documents the reproducibility bridge between the manuscript

> A3D-BSM: Priority-Aware 3D Broadcast Suppression for Distributed UAV Networks with Insider-Resilient Coordination

and the executable ns-3 artifact in this repository.

## What This Folder Covers

The scripts here run the paper-aligned A3D-BSM ns-3 campaign using the public
artifact:

- protocol mode: `astro` (the implementation path that exercises A3D-BSM)
- swarm sizes: `20, 40, 60, 80`
- mobility models: `gm3d, rpgm`
- final-evaluation seeds: `3001--3010`
- simulation duration: `120 s`
- packet rate: `1 pkt/s`
- Byzantine/gray-hole fractions for the insider experiment: `0, 0.1, 0.2, 0.3`

The main runner is:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all
```

For a quick command preview without running ns-3:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all --dry-run
```

For a short smoke test:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign smoke
```

After results are generated, summarize the available CSV metrics with:

```bash
python3 paper-reproduction/summarize_a3dbsm_results.py \
  --results-dir ns-allinone-3.29/ns-3.29/results/paper-a3dbsm
```

Each non-dry-run campaign also writes a `run-manifest.json` file in every output
folder. The manifest records the exact ns-3 command, working directory, and
parsed parameters (`nUavs`, mobility, seed, simulation time, packet rate, and
Byzantine fraction) for every requested run.

## Important Scope Statement

This repository currently provides an inspectable and runnable A3D-BSM
implementation artifact. It should be cited as the implementation and execution
companion for the paper.

It is not yet a complete reproduction package for every manuscript figure and
table because the exact paper baselines (`SF`, `CB`, `PR`, `SBA`, and
`3D-DNA-BSP`) are not all exposed as executable protocol selectors in the
current public scenario. The legacy protocol modes (`aodv`, `olsr`,
`epidemic`, `dqn`) are useful for code execution and comparison experiments,
but they are not the packet-level broadcast baselines reported in the
manuscript.

See `reproducibility_status.md` for the coverage matrix.

## Recommended Wording For The Manuscript

Use language such as:

```latex
The A3D-BSM ns-3 implementation and executable simulation artifact are
available at \url{https://github.com/sabriallani/astro-fanet-artifact}. The
repository provides the A3D-BSM suppression layer, trust-support implementation,
scenario configuration, and paper-aligned A3D-BSM campaign scripts. Full
multi-seed reproduction scripts and raw traces for every baseline and figure
will be archived with the camera-ready version or made available upon
reasonable request.
```
