# Reproducibility Status

This document is intended to help editors and reviewers understand exactly what
is covered by the public artifact.

## Covered In The Current Repository

| Manuscript component | Artifact status | Relevant files |
| --- | --- | --- |
| A3D-BSM suppression rule | Implemented | `ns-allinone-3.29/ns-3.29/src/astro-fanet/model/a3d-bsm.cc` |
| Fixed calibration coefficients | Implemented in source | `a3d-bsm.cc` |
| Emergency non-suppression override | Implemented | `a3d-bsm.cc` |
| Trust-aware coordination support | Implemented | `trust-manager.cc`, `astro-routing-protocol.cc` |
| HMAC-style intent authentication scaffold | Implemented at simulation-artifact level | `astro-packet.*`, `trust-manager.*` |
| GM3D and RPGM mobility selection | Implemented | `scratch/astro-fanet-sim.cc`, `rpgm-mobility-model.*` |
| Paper swarm sizes and final seeds for A3D-BSM | Scripted | `paper-reproduction/run_a3dbsm_paper_campaign.py` |
| Gray-hole fraction sweep up to `0.3` for A3D-BSM | Scripted | `paper-reproduction/run_a3dbsm_paper_campaign.py` |
| CSV summary of available artifact metrics | Scripted | `paper-reproduction/summarize_a3dbsm_results.py` |

## Not Yet Fully Covered

| Manuscript component | Current gap |
| --- | --- |
| Exact packet-level baselines `SF`, `CB`, `PR`, `SBA`, `3D-DNA-BSP` | Not all exposed as protocol selectors in the public ns-3 scenario |
| Complete reproduction of every table and figure | Requires baseline parity and/or archived raw traces |
| Exact suppression precision table from the manuscript | The current public CSV exposes suppression counts and related counters, but not every manuscript-level post-processed label |
| Non-trust A3D-BSM variant (`A3D-BSM-NT`) | Not currently exposed as a separate command-line mode |
| Frozen raw traces for all final manuscript runs | Not currently versioned in this repository |

## Recommended Positioning

The repository can honestly be described as:

- an executable A3D-BSM ns-3 artifact;
- a public implementation companion to the manuscript;
- a paper-aligned A3D-BSM campaign runner;
- a basis for editor/reviewer inspection of the proposed mechanism.

The repository should not yet be described as:

- a full end-to-end replication archive for every reported baseline;
- a frozen artifact containing all raw traces used to generate the final paper tables;
- an official implementation of third-party baselines such as DNA-BSP.

## Minimum Additions For A Full Journal-Grade Package

To make the repository a full reproduction package, add one of the following:

1. executable implementations of the exact paper baselines (`SF`, `CB`, `PR`,
   `SBA`, `3D-DNA-BSP`) under the same scenario interface; or
2. a frozen archive of raw final-run CSV traces for all protocols, seeds,
   swarm sizes, and mobility models, together with post-processing scripts that
   regenerate the manuscript tables and figures.
