# Current Limitations

This repository is a practical ns-3 artifact for execution and visualization, but it does not yet constitute a full journal-grade reproduction package for every reported result.

## Current Technical Limits

- The SLM stage is emulated in ns-3 and currently uses synthetic or simplified embedding behavior in the executable path.
- The MAPPO agent code supports loading weights, but the current runnable prototype does not package a full frozen training artifact with reproducible checkpoints.
- Some protocol details in the executable prototype are simplified and should be treated as prototype-level logic.

## What Is Still Missing For A Full Reproduction Package

- frozen training outputs and documented weight files
- precomputed embedding bank and provenance for the representation pipeline
- exact batch scripts for the full multi-seed campaign
- confidence-interval and statistical-test reproduction scripts
- complete baseline parity with every protocol reported in the final ns-3 study
- tighter alignment between the manuscript claims and the packaged executable settings

## Recommended Positioning

The repository should be described as:

- `ns-3 artifact`
- `demonstration and executable reference implementation`
- `visualization and local validation package`

It should not yet be described as:

- `complete replication package for all reported results`
- `fully frozen training-and-evaluation artifact`
