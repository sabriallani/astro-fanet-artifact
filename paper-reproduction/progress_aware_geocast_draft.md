# Draft — Progress-Aware Geocast Prototype

Status: **prototype validated, not promoted to the ns-3 algorithm**

Date: 2026-09-06

## Objective

Test a safer A3D-BSM forwarding rule before modifying the production routing
algorithm. The prototype is isolated under `prototype/` and is not imported by
`astro-fanet-sim.cc`.

## Proposed rule

For a current relay `u` and candidate neighbor `v`:

1. compute `d(u, sink)` and `d(v, sink)`;
2. compute link distance `d(u, v)`;
3. keep `v` only if `d(u, v) <= radio_range`;
4. keep `v` only if `d(v, sink) < d(u, sink) - epsilon`;
5. select the candidate with maximum positive progress;
6. suppress only when no positive-progress relay exists.

The rule prevents a relay from forwarding backward or selecting a neighbor that
cannot be reached by the current node. It also prevents a useful forwarder from
being suppressed merely because the neighborhood is dense.

## Files

- `prototype/progress_aware_geocast.py`
- `tests/test_progress_aware_geocast.py`

The production ns-3 algorithm was **not modified** by this draft.

## Tests executed

```text
python -m unittest tests.test_progress_aware_geocast -v
3 tests OK

python -m unittest discover -s tests -v
22 tests OK
```

The first prototype test run correctly exposed an implementation error: the
prototype initially compared the neighbor-to-sink distance with the radio range
instead of the current-to-neighbor link distance. That error was corrected, and
the complete suite then passed.

## What is validated

- backward neighbors are rejected;
- a positive-progress neighbor is selected;
- suppression is returned only when no useful relay is available;
- a deterministic line topology reaches the sink without a loop;
- existing ns-3 regression tests remain green.

## What is not yet validated

This is not yet an ns-3 PDR result. It does not prove that the rule improves the
full mobile wireless simulation. Promotion requires a separate experimental
integration, followed by smoke and diagnostic runs, and comparison against the
current implementation using unchanged radio, mobility, and traffic settings.

## Promotion gate

Do not modify the production algorithm until the user confirms promotion after
reviewing the integrated ns-3 results. Required evidence:

- compilation success;
- no PDR above 100%;
- unique-packet PDR;
- no forwarding loops in traces;
- comparison on the same seeds and scenarios;
- raw CSV, manifest, and logs published;
- no claim of improvement unless the measured comparison supports it.
