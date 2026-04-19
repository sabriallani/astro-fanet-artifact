# Current Limitations

This repository is a practical ns-3 artifact for execution and visualization, but it does not yet constitute a full journal-grade reproduction package for every reported result.

## Current Technical Limits

- The A3D-BSM suppression layer uses fixed offline-calibrated coefficients that are disclosed in `src/astro-fanet/model/a3d-bsm.cc`; no online LLM, SLM, or neural calibration is used at runtime.
- The executable scenario retains older ASTRO-FANET routing scaffolding so the ns-3 demo remains buildable. Those helper files are not the claimed calibration mechanism of the current A3D-BSM manuscript.
- The beacon header carries extra simulator-observable state such as position, velocity, residual energy, and trust score. The paper-level 48-byte intent-plus-HMAC overhead is represented separately from the full ns-3 beacon serialization.
- Some protocol details in the executable prototype are simplified and should be treated as prototype-level logic.

## What Is Still Missing For A Full Reproduction Package

- exact batch scripts for the full multi-seed campaign
- confidence-interval and statistical-test reproduction scripts
- complete baseline parity with every protocol reported in the final ns-3 study
- a fully frozen archive of every raw trace used to produce the final manuscript tables

## Recommended Positioning

The repository should be described as:

- `ns-3 artifact`
- `demonstration and executable reference implementation`
- `visualization and local validation package`
- `A3D-BSM code companion`

It should not yet be described as:

- `complete replication package for all reported results`
- `fully frozen end-to-end reproduction archive`
