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

## Robustness and Byzantine validation

A second paired matrix tested three additional nominal RPGM seeds and three
seeds for each of GM3D and RPGM with a Byzantine fraction of `0.2`.

| Scenario | Reference PDR | Experimental PDR | Difference |
|---|---:|---:|---:|
| 20 UAV RPGM, nominal, 3 new runs | 27.0917% | 46.4939% | +19.4022 pp |
| 20 UAV GM3D, Byzantine 0.2, 3 runs | 65.5403% | 47.0879% | -18.4524 pp |
| 20 UAV RPGM, Byzantine 0.2, 2 valid runs | 59.7154% | 75.3870% | +15.6716 pp |

The `20 UAV RPGM, seed 3003` Byzantine run failed identically in both variants
with the same ns-3 buffer assertion and was excluded. All other robustness
runs completed and produced valid CSVs.

The Byzantine GM3D regression is a promotion blocker. It shows that the simple
strict-progress guard is not sufficient as an insider-resilient policy: under
some topologies it can remove alternate relays that are needed when a forward
relay is Byzantine. The current progress-aware guard must therefore remain an
experimental result and must not be merged into the principal algorithm.

Final experimental decision: **retain the branch and evidence, do not promote
this exact guard, and do not use it to claim universal robustness**. A future
candidate would need trust-aware fallback or relay diversity, followed by the
same paired validation matrix.

## Second candidate: trust-aware emergency fallback

The first fallback was refined so that a non-progressing relay could forward an
`EMERGENCY` packet only when the local node was honest and no authenticated
trusted neighbor known in the beacon table could make geographic progress from
the previous relay position. Ordinary packets remained strictly
progress-aware.

The implementation passed the local regression suite (`24` tests) and the
GitHub Actions compilation and smoke validation. The paired validation used
distinct reference and experimental binaries; its source commit was
`3a864385f7f6ce28a4d7eda49edd08d4da144317`.

| Scenario | Reference PDR | Candidate PDR | Difference |
|---|---:|---:|---:|
| 10 UAV GM3D, nominal, 3 runs | 67.3739% | 83.4403% | +16.0664 pp |
| 20 UAV GM3D, nominal, 3 runs | 50.2935% | 61.4475% | +11.1540 pp |
| 20 UAV RPGM, nominal, 2 valid runs | 57.7508% | 57.5821% | -0.1687 pp |
| 20 UAV GM3D, Byzantine 0.2, 3 runs | 65.5403% | 47.8255% | -17.7148 pp |
| 20 UAV RPGM, Byzantine 0.2, 2 valid runs | 59.7154% | 62.6814% | +2.9659 pp |

The common `20 UAV RPGM, seed 3003` Byzantine failure remained present in both
variants and was not used in either mean. The candidate reduces broadcasts and
delay, but the GM3D Byzantine regression remains a decisive blocker. The
candidate is therefore retained as an experimental analysis only and is not
promoted to `main` or described as a robust improvement.

## Final bounded-diversity candidate

The fallback was finally bounded to the first two hops of `EMERGENCY` packets.
After hop two, strict geographic progress filtering is restored. This reduces
the chance that an early Byzantine relay eliminates the only usable branch
without turning the whole route into a broadcast flood.

The final candidate was validated with `24` local tests, successful CI
compilation, smoke validation, paired validation, and the robustness matrix.

| Scenario | Reference PDR | Two-hop candidate PDR | Difference |
|---|---:|---:|---:|
| 10 UAV GM3D, nominal, 3 runs | 67.3739% | 73.5345% | +6.1606 pp |
| 20 UAV GM3D, nominal, 3 runs | 50.2935% | 53.2776% | +2.9841 pp |
| 20 UAV RPGM, nominal, 2 valid runs | 57.7508% | 55.0156% | -2.7352 pp |
| 20 UAV GM3D, Byzantine 0.2, 3 runs | 65.5403% | 59.9405% | -5.5998 pp |
| 20 UAV RPGM, Byzantine 0.2, 2 valid runs | 59.7154% | 60.3495% | +0.6341 pp |

Compared with the unrestricted progress-aware guard, the two-hop bound removes
most of the GM3D Byzantine regression and preserves the RPGM Byzantine result.
It does not completely eliminate the GM3D Byzantine gap, so it remains a
conditional experimental result rather than a universal robust improvement.
The common RPGM seed `3003` failure remains identical in reference and
experimental runs and is retained as an explicit limitation.
