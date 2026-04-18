# Video Demo Workflow

## One-Command Demo

```bash
cd ns-allinone-3.29/ns-3.29
./run-astro-video-demo.sh
```

This script:

- configures and runs the ASTRO-FANET ns-3 scenario
- enables a cleaner NetAnim preset intended for recording
- generates `.anim.xml`, `.routes.xml`, and `.csv` outputs
- opens NetAnim automatically when `OPEN_GUI=1`

## Run Without Opening The GUI

```bash
cd ns-allinone-3.29/ns-3.29
OPEN_GUI=0 ./run-astro-video-demo.sh
```

## Custom Demo Run

```bash
cd ns-allinone-3.29/ns-3.29
./run-astro-video-demo.sh video-demo 12 30 1001
```

Arguments:

1. output directory
2. number of UAVs
3. simulation time in seconds
4. random seed

## Open A Generated Trace Manually

```bash
cd ns-allinone-3.29/netanim-3.108
./NetAnim ../ns-3.29/video-demo/astro_n12_gm3d_s1001.anim.xml
```

## Demo Preset Notes

The video preset favors visual clarity:

- larger nodes
- visible sink
- background image
- route and counter overlays

It is intended for demonstration videos rather than for strict numerical campaign reporting.
