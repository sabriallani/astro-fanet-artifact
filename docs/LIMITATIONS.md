# Current Limitations

This repository is a practical ns-3 artifact for execution and visualization, but it does not yet constitute a strict reproduction package for every claim in the manuscript.

## Key Scope Differences

- The manuscript describes a CupCarbon-based evaluation campaign, whereas this artifact runs an ns-3 prototype.
- The SLM stage is emulated in ns-3 and currently uses synthetic or simplified embedding behavior in the executable path.
- The MAPPO agent code supports loading weights, but the current runnable prototype does not package a full frozen training artifact with reproducible checkpoints.
- Some protocol details in the executable prototype are simplified relative to the paper narrative and should be treated as prototype-level logic.

## What Is Still Missing For A Full Journal-Grade Artifact

- frozen training outputs and documented weight files
- precomputed embedding bank and provenance for the representation pipeline
- exact batch scripts for the full multi-seed campaign
- confidence-interval and statistical-test reproduction scripts
- complete baseline parity with every protocol reported in the manuscript
- cross-check between paper claims and executable settings to eliminate narrative drift

## Recommended Positioning

The repository should be described as:

- `ns-3 prototype artifact`
- `demonstration and executable reference implementation`
- `visualization and local validation package`

It should not yet be described as:

- `exact full reproduction of the CupCarbon campaign`
- `complete replication package for all reported results`
