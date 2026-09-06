# Final validation interpretation

All values below are generated from the published CSV summaries; no historical manuscript value is reused.

## Paired nominal validation

- **10 UAV, GM3D:** PDR changes from 67.373900% to 73.534500% (+6.1606 percentage points); mean delay changes by -0.005892 and broadcasts by -0.67.
- **20 UAV, GM3D:** PDR changes from 50.293467% to 53.277600% (+2.9841 percentage points); mean delay changes by -0.030034 and broadcasts by +0.00.
- **20 UAV, RPGM:** PDR changes from 57.750800% to 55.015600% (-2.7352 percentage points); mean delay changes by +1.668065 and broadcasts by +17.00.

## Byzantine and mobility robustness

- **20 UAV, GM3D, Byzantine fraction 0.2:** PDR changes from 65.540300% to 59.940467% (-5.5998 percentage points). This is a scenario-dependent result, not evidence of universal Byzantine robustness.
- **20 UAV, RPGM, Byzantine fraction 0:** PDR changes from 27.091667% to 36.344167% (+9.2525 percentage points). This is a scenario-dependent result, not evidence of universal Byzantine robustness.
- **20 UAV, RPGM, Byzantine fraction 0.2:** PDR changes from 59.715400% to 60.349450% (+0.6341 percentage points). This is a scenario-dependent result, not evidence of universal Byzantine robustness.

## Reproducibility limits

The paired campaign contains a common failed run for 20-UAV RPGM seed 3003; it is marked FAIL in both reference and experimental status files and is not silently converted into a numerical result. The final candidate therefore remains an experimental, mobility-dependent variant. The artifact does not claim official packet-level reproduction of baselines that are not executable in the public scenario.
