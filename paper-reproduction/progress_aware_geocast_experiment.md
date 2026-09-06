# Experimental validation — A3D-BSM + progress-aware geocast

Status: **experimental branch only; not promoted to the reference algorithm**

Branch: `experiment/progress-aware-geocast`
Commit used by CI: `45a560d208dcfdceeacfe60f367cd754ff037033`

## Algorithm tested

After the A3D-BSM rebroadcast decision, a relay is allowed to transmit only if
its current position is strictly closer to the modeled sink position than the
previous relay position. Non-progressing relays are suppressed. The guard is
implemented in `AstroRoutingProtocol::RouteInput()` through
`IsProgressingRelay()`.

The reference branch and `main` were not modified.

## Real diagnostic results

| Scenario | Reference PDR | Experimental PDR | Difference |
|---|---:|---:|---:|
| 10 UAV, GM3D | 71.5328% | 99.2701% | +27.7373 pp |
| 20 UAV, GM3D | 54.6814% +/- 3.99286 | 53.2853% +/- 4.37213 | -1.3961 pp |
| 20 UAV, RPGM | 48.5240% | 57.3801% | +8.8561 pp |

Average delay:

| Scenario | Reference | Experimental |
|---|---:|---:|
| 10 UAV, GM3D | 0.357143 ms | 0.301471 ms |
| 20 UAV, GM3D | 0.442251 ms | 0.275767 ms |
| 20 UAV, RPGM | 7.78707 ms | 3.57556 ms |

Broadcast counts decreased:

| Scenario | Reference | Experimental |
|---|---:|---:|
| 10 UAV, GM3D | 680 | 308 |
| 20 UAV, GM3D | 3520 | 1724 |
| 20 UAV, RPGM | 1920 | 898 |

Control overhead remained unchanged in this diagnostic because the current
CSV denominator and control-byte accounting are not changed by the relay guard.

## Interpretation

The result is **promising but mixed**:

- strong improvement at 10 UAV GM3D;
- improvement at 20 UAV RPGM;
- no improvement at 20 UAV GM3D within the current two-run diagnostic;
- lower delay and fewer rebroadcasts in all three listed scenarios;
- no basis for claiming universal PDR scalability improvement yet.

The experiment demonstrates that progress-aware suppression can reduce useless
rebroadcasts and can improve delivery in some mobility/topology conditions, but
the 20-UAV GM3D result prevents a universal claim.

## Validation status

- Local test suite after integration: 23 tests OK.
- GitHub smoke workflow: success.
- GitHub diagnostic workflow: success.
- PDR values remained <= 100%.
- Reference branch was not changed.

## Promotion decision

**Do not merge or promote yet.** Run more controlled seeds for 20-UAV GM3D
and RPGM, then compare confidence intervals and paired seed-by-seed differences.
Promotion is justified only if the advantage persists without unacceptable
rebroadcast or overhead cost.

No manuscript text or paper claims were changed by this experiment.

## Paired validation extension

A paired GitHub Actions validation ran the reference and experimental branches
with identical seeds and settings:

- 10 UAV GM3D, seeds 3001--3003;
- 20 UAV GM3D, seeds 3001--3003;
- 20 UAV RPGM, seeds 3001--3003.

Eight paired runs produced valid CSV results. The `20 UAV RPGM, seed 3003`
run failed identically in both variants with the ns-3 buffer deserialization
assertion `m_current >= m_dataStart && m_current < m_dataEnd`; it is a shared
scenario failure, not an experimental-versus-reference difference, and is not
included in the means.

Paired means over valid runs:

| Scenario | Reference PDR | Experimental PDR | Difference |
|---|---:|---:|---:|
| 10 UAV GM3D (3 runs) | 67.3739% | 83.4403% | +16.0664 pp |
| 20 UAV GM3D (3 runs) | 50.2935% | 60.0875% | +9.7940 pp |
| 20 UAV RPGM (2 valid runs) | 57.7508% | 55.2759% | -2.4750 pp |

The paired validation confirms a positive effect for both GM3D cases and no
confirmed improvement for RPGM. It supports a conditional claim about
progress-aware suppression under GM3D, not a universal claim across mobility
models. The common RPGM seed failure must be fixed or explicitly excluded from
future campaign design before final publication figures are generated.
