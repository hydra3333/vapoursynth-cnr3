# CNR3 Plan-Retry Bias v2 — Scope Evaluation

## Summary

The v2 scope is a sensible experimental narrowing. It retains the load-bearing measurements needed to answer the experiment's core question while avoiding a large amount of throwaway diagnostic machinery.

The retained counter pair:

- `dumped_plan_holes_total`
- `kept_plan_holes_total`

is sufficient for the first-order decision: whether retrying actually shrinks hole-heavy plans or merely delays execution.

## Assessment

Proceed as a gated experiment, with the default macro OFF. The proposed mechanism is still best treated as a polling/throttle approximation of the future in-flight reservation/await mechanism, not as a final fmParallel readiness fix.

The design is plausible because the dump/sleep/re-plan decision occurs before issuing source requests for the kept plan. If the original hole-heavy plan was stale only because predecessor outputs were about to be published by peer activations, the second plan should have fewer holes and should avoid both source-request pressure and redundant hole computation.

## Accepted counter compromise

For an experiment, it is reasonable not to adopt all suggested counters. The v2 counter set is lean but still informative:

- echo the knobs and derived retry depth;
- count dumped plans and sleeps;
- split kept plans by first/second/third-or-later attempt;
- count dumped-plan holes;
- count kept-plan holes.

The dropped counters are acceptable omissions for a throwaway probe:

- min/max hole counts;
- per-attempt candidate buckets;
- hole-delta accounting;
- retry-no-sleep breakdowns;
- source-request-avoidance estimate.

If the experiment passes and is promoted, the fuller counter set should be reconsidered.

## Required cleanup before coding

There are stale `plan_retry_max` formula descriptions in the scope that should be normalized before coding. The final rule should be stated everywhere as:

```text
plan_retry_max = min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads / 2))
```

The stale simplified form `max(1, numThreads/2)` still appears in places and could mislead implementation.

Also clarify that “macro OFF compiles to exactly today's code” means experiment loop/counters/helpers vanish and output behaviour is byte-identical. If the edit-version marker changes for the patch, logs/binaries will not literally be byte-identical to the prior commit even though the S8 output proof can be byte-identical.

## Interpretation guard

The throttle-vs-adopt guard is important and should remain. A duplicate reduction is not automatically success.

Positive result:

- duplicates fall;
- `frames_computed` falls toward output-frame count;
- fps does not collapse;
- `kept_plan_holes_total` is materially lower than `dumped_plan_holes_total`.

Negative result:

- duplicates fall only because sleeping throttles the request wave;
- fps collapses;
- kept plans are not materially smaller than dumped plans.

## Verdict

The v2 scope is acceptable for an experiment. It is lean enough to implement, instrumented enough to answer the central question, and honest about the mechanism's limits.

Recommended status:

```text
APPROVE AS GATED EXPERIMENT, subject to wording cleanup on plan_retry_max and macro-off exactness.
```

This should not be treated as an fmParallel readiness fix. It is a measurement probe and possible cheap mitigation. If it fails, it strengthens the case for the reservation table. If it succeeds, it identifies that a large fraction of the waste is short-lived predecessor-publication lag.
