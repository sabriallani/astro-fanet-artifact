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

## Visual Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=visual-results --enableAnim=1"
```

This creates:

- `*.anim.xml` for NetAnim playback
- `*.routes.xml` for route-table visualization
- `*.csv` for numeric results
