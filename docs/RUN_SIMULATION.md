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

## Useful Parameters

- `--nUavs=10`
- `--protocol=astro`
- `--mobility=gm3d`
- `--seed=1001`
- `--simTime=10`
- `--outputDir=results`
- `--enableAnim=1`
- `--videoMode=1`

## Visual Run

```bash
cd ns-allinone-3.29/ns-3.29
./waf --run "astro-fanet-sim --nUavs=10 --protocol=astro --mobility=gm3d --seed=1001 --simTime=10 --outputDir=visual-results --enableAnim=1"
```

This creates:

- `*.anim.xml` for NetAnim playback
- `*.routes.xml` for route-table visualization
- `*.csv` for numeric results
