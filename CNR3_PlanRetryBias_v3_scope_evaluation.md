# CNR3 PlanRetryBias v3 Scope Evaluation

## Overall verdict

**APPROVE AS A GATED EXPERIMENT**, subject to the small cleanup items below.

The v3 scope is materially stronger than v2. It resolves the two important ambiguity points from the prior review:

1. The retry depth formula is now consistently stated as:

   ```text
   plan_retry_max = min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads / 2))
   ```

2. The macro-off proof is now correctly described as **frame-output byte identity**, not literal binary/log identity, because the edit-version marker changes.

The experiment remains correctly scoped as a biasing probe, not an fmParallel readiness proof and not a replacement for a reservation/await table.

## What v3 gets right

### 1. Correct retry-depth formula is now consistent

The knobs, explanatory note, W3X note, pseudocode, and hard requirement all now align on:

```text
min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads / 2))
```

This removes the v2 implementation ambiguity.

### 2. Macro-off exactness wording is now correct

The hard requirement now says macro OFF compiles to today's **behaviour**, and that R-PROCESS-19 proves S8 frame output byte identity rather than literal binary/log identity. That is the right formulation because the edit-version marker changes.

### 3. Counter set is appropriate for throwaway experiment class

The minimal counter set is adequate because it preserves the load-bearing measurement:

```text
dumped_plan_holes_total
kept_plan_holes_total
```

That pair answers the experiment's central question: did retrying shrink the plans, or did it merely delay the same holes?

The rejected/deferred counters are reasonable for this stage. The scope correctly defers min/max, exact delta, source-request avoided estimate, and richer candidate buckets unless the experiment is promoted to a retained mechanism.

### 4. Throttle-vs-adopt guard is essential and retained

The scope correctly states that duplicate reduction with large fps collapse is not a win. The experiment only looks positive if kept plans get smaller relative to dumped plans and the fps cost remains bounded.

### 5. fmParallelRequests -r4 macro-ON control is valuable

This is a good addition. Since fmParallelRequests -r4 previously had zero duplicate waste, macro ON should ideally produce near-zero dumped plans there. If it sleeps anyway, the experiment is adding latency to a mode that did not need this mitigation.

## Remaining cleanup items before coding

### A. Header still says v1

The filename is `CNR3_Patch_Scope_PlanRetryBias_v3.md`, but the first heading still says:

```text
# CNR3 — PATCH SCOPE: plan-retry biasing experiment (gated) — v1
```

Recommended change:

```text
# CNR3 — PATCH SCOPE: plan-retry biasing experiment (gated) — v3
```

This is not technically blocking, but it prevents avoidable handoff confusion.

### B. Define or remove the "attempts == kept + dumped" self-check

The counter list includes:

```text
plans_dumped_total
plans_kept_on_attempt_1 / _2 / _3plus
```

The self-check says:

```text
attempts == kept + dumped
```

For that self-check to be meaningful, `attempts` must be either:

1. explicitly printed as a counter, for example:

   ```text
   plan_retry_plan_attempts_total
   ```

   with:

   ```text
   plan_retry_plan_attempts_total
     == plans_dumped_total
      + plans_kept_on_attempt_1
      + plans_kept_on_attempt_2
      + plans_kept_on_attempt_3plus
   ```

   or

2. removed, because otherwise it is either undefined or tautological.

Recommended minimal fix:

```text
Add:
  plan_retry_plan_attempts_total

Then print-only self-check:
  plan_retry_plan_attempts_total ==
      plans_dumped_total
    + plans_kept_on_attempt_1
    + plans_kept_on_attempt_2
    + plans_kept_on_attempt_3plus
```

This keeps the counter set small while making the self-check meaningful.

### C. Clarify whether "attempt" includes non-recovery/non-hole plans

The experiment applies to all live plan formation attempts, but only dumps when hole count exceeds the threshold. The summary should make clear whether `plan_retry_plan_attempts_total` counts every formed candidate plan under the experiment path, including attempt-1 plans with zero holes.

Recommended wording:

```text
plan_retry_plan_attempts_total:
  total candidate plans formed while the experiment path was active, including
  plans kept on attempt 1 and dumped/retried candidate plans.
```

### D. Keep warning-only self-checks genuinely warning-only

The scope says print-only self-checks are never wired to selftest and WARN only on failure. That is correct for an experiment. Delivery should ensure these do not alter output, return status, or cache behaviour.

## Implementation-risk notes

### 1. Sleep placement remains the main safety point

The scope correctly requires sleeping with no cache mutex or other lock held. This should be explicitly confirmed in the coder report with the exact code locations.

### 2. Dumped plan destruction is the second safety point

A dumped plan must release pins and balance recovery plan create/destroy accounting. No source requests should be issued for dumped plans, and no temp outputs should be created.

### 3. The experiment may only throttle fmParallel

The proposal remains a timing bias. If the request wave narrows because workers sleep, duplicates may fall without proving useful adoption. The throttle-vs-adopt guard and hole-total pair are therefore mandatory for interpretation.

## Recommended final verdict language

```text
APPROVE AS GATED EXPERIMENT.

v3 resolves the retry-depth and macro-off proof wording issues from v2.
Proceed if the header version is corrected and either plan_retry_plan_attempts_total
is added or the undefined attempts self-check is removed.

The experiment is suitable as a cheap diagnostic and possible mitigation for
fmParallel duplicate recompute storms. It must not be interpreted as fmParallel
readiness unless byte identity, ownership/counter balance, duplicate reduction,
hole shrinkage, and bounded fps loss are all shown.
```

## Suggested small patch to the scope text

### Header

Replace:

```text
# CNR3 — PATCH SCOPE: plan-retry biasing experiment (gated) — v1
```

with:

```text
# CNR3 — PATCH SCOPE: plan-retry biasing experiment (gated) — v3
```

### Counter block

Add after `plan_retry_max` echo line:

```text
plan_retry_plan_attempts_total
```

### Self-check wording

Replace:

```text
attempts == kept + dumped
```

with:

```text
plan_retry_plan_attempts_total ==
    plans_dumped_total
  + plans_kept_on_attempt_1
  + plans_kept_on_attempt_2
  + plans_kept_on_attempt_3plus
```
