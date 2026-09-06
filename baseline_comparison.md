# Baseline comparison campaign

This document describes the executable baseline comparison added to the
artifact, what it does and does not establish, and how to reproduce it.

## Why this campaign exists

Earlier revisions of the manuscript compared A3D-BSM only against variants of
itself. The scenario exposed five protocol selectors (`astro`, `aodv`, `olsr`,
`epidemic`, `dqn`), but inspection of `scratch/astro-fanet-sim.cc` showed that
`epidemic` and `dqn` were **not independent implementations**: both installed
the AODV stack with modified attributes. No broadcast-suppression baseline was
therefore executable, while the contribution of the paper is precisely
broadcast suppression.

This campaign adds four broadcast-suppression baselines that run inside the
same routing protocol, on the same mobility traces, with the same seeds and the
same emergency traffic.

## What the baselines are

All four are **local re-implementations** written for this artifact. They are
not the authors' original code and must not be described as reference
implementations. They follow the decision rules described in the literature:

| Selector | Family | Rebroadcast decision |
|---|---|---|
| `sf` | Simple flooding | Always rebroadcast once per unique packet |
| `pr` | Probabilistic | Rebroadcast with fixed probability `p` |
| `cb` | Counter-based | Rebroadcast unless `>= C` copies heard during a random delay |
| `sba` | Scalable broadcast | Rebroadcast only if it covers neighbours the sender did not reach |

They are implemented in
`ns-allinone-3.29/ns-3.29/src/astro-fanet/model/broadcast-baselines.{h,cc}`
and hooked into the single rebroadcast decision point of
`astro-routing-protocol.cc`. When a baseline is active, the A3D-BSM
suppression logic (utility score, trust gate, progress guard) is bypassed, so
the comparison isolates the suppression policy rather than the whole stack.

**Fairness note.** All schemes share the identical PHY/MAC, mobility, traffic
pattern, and duplicate-detection cache. The only thing that varies is the
rebroadcast decision.

## Metrics now instrumented

The manuscript promised metrics that the runs never emitted. The following
counters were added to the protocol and are written to each run's `metrics.csv`:

| Metric | Definition as implemented |
|---|---|
| `RR` (redundancy ratio) | duplicate data receptions / total data receptions |
| `SP` (saved rebroadcast ratio) | suppressed / (suppressed + rebroadcast) |
| `BL` (broadcast path length) | mean hop count of delivered emergency packets |
| `ENSR` (emergency non-suppression rate) | emergency packets forwarded / emergency packets eligible |

`ENSR` is the empirical counterpart of the Priority Theorem: the theorem
predicts that an eligible emergency packet is never suppressed, i.e. `ENSR`
should be 100 %. Any value below 100 % is a measured violation and is reported
as such.

## Statistical protocol

- 5 protocols x {10, 20} UAVs x {GM3D, RPGM} x 5 seeds (3001-3005) = 100 runs.
- Each cell reports mean, standard deviation and a **t-based 95 % confidence
  interval** (`aggregate_baseline_campaign.py`). The t quantile is used, not
  1.96, because n = 5 per cell.
- Failed runs are recorded in `status.tsv` and **excluded from the mean while
  remaining visible**. A cell whose runs all failed is rendered as a gap in the
  figures and `--` in the tables. Nothing is imputed or smoothed.

## Reproducing

```bash
# CI (recommended: the toolchain is pinned there)
#   Actions -> "baseline comparison campaign" -> Run workflow

# Aggregate a downloaded results tree
python3 paper-reproduction/aggregate_baseline_campaign.py \
    --results-dir <campaign-results> \
    --output baseline_campaign_summary.csv

# Figures + LaTeX tables
python3 paper-reproduction/generate_baseline_figures.py \
    --summary baseline_campaign_summary.csv \
    --figures figures/baseline-comparison \
    --tables tables/baseline-comparison
```

## Limitations

- The baselines are re-implementations; parameter choices (`p`, `C`, delay
  windows) are stated in `broadcast-baselines.h` and were not tuned per
  scenario. A reviewer may reasonably ask for a sensitivity study.
- 30 s of simulated time per run is short; it is the budget the CI runner
  sustains for 100 runs.
- RPGM cells retain high variance, consistent with earlier campaigns.
