# Run The Simulation

## Standard Console Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf configure --disable-werror
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=results"
```

Expected outputs:

- terminal summary with PDR, delay, throughput, AoI, overhead, energy, and suppression metrics
- one CSV file in the selected output directory

The `astro` mode exercises the A3D-BSM broadcast-suppression path with the fixed calibration coefficients compiled into `src/astro-fanet/model/a3d-bsm.cc`.

## Useful Parameters

- `--nUavs=10`
- `--protocol=astro`
- `--mobility=gm3d`
- `--seed=1001`
- `--simTime=10`
- `--outputDir=results`
- `--enableAnim=1`
- `--videoMode=1`

## Quick Smoke Test

For a short local check after editing the A3D-BSM code:

```bash
cd ns-allinone-3.29/ns-3.29
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=3001 --simTime=5 --outputDir=paper-check-a3d"
```

The CSV should include columns such as `ctrlOverhead`, `broadcasts`, and `suppressed`.

## Paper-Aligned A3D-BSM Campaign

From the repository root, preview the manuscript-aligned A3D-BSM runs:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all --dry-run
```

Run the A3D-BSM paper campaign:

```bash
python3 paper-reproduction/run_a3dbsm_paper_campaign.py --campaign all
```

This uses:

- `N = 20, 40, 60, 80`
- mobility `gm3d` and `rpgm`
- seeds `3001--3010`
- `simTime = 120`
- `pktRate = 1.0`
- Byzantine fractions `0, 0.1, 0.2, 0.3` for the insider sweep

See `paper-reproduction/reproducibility_status.md` before using this artifact
as evidence for manuscript-level reproducibility.

## Visual Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=visual-results --enableAnim=1"
```

This creates:

- `*.anim.xml` for NetAnim playback
- `*.routes.xml` for route-table visualization
- `*.csv` for numeric results
