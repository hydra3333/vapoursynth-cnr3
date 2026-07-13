# CNR3 - Scope Evaluation: Plan-Retry Biasing Experiment v1

## Source reviewed

- `CNR3_Patch_Scope_PlanRetryBias_v1.md`
- Proposed success marker: `CMS07-EXPERIMENT.plan-retry-bias`
- Proposed baseline: current committed tree, `CMS07-SCAFFOLD.filter-mode-selector`
- Proposed class: gated experiment, macro OFF by default, behaviour-affecting only when `CNR3_EXPERIMENT_PLAN_RETRY_BIAS` is enabled.

## Executive verdict

The scope is coherent as an experiment and is suitable for a gated patch phase, provided it is treated as a measurement-and-mitigation experiment rather than a correctness fix.

The proposal is deliberately simple: when arInitial forms a recovery plan with too many holes, discard that plan, sleep briefly, then re-plan. If the holes were caused by peer activations that were already computing the missing predecessors, the later plan should see more published outputs and should either become smaller or avoid recovery-hole recomputation altogether.

This is a plausible brute-force mitigation for fmParallel duplicate recompute pressure, especially where the current failure mode is "predecessor not cache-visible yet, but likely to become visible shortly." It is not a substitute for an in-flight reservation/await table because it cannot distinguish genuinely absent frames from in-flight frames and it cannot wake exactly at publication time.

## Scope classification

Recommended classification:

```text
CMS07-EXPERIMENT.plan-retry-bias
Gated behaviour-affecting experiment
Committed default: OFF
Macro ON: experimental live plugin behaviour change
```

This should not be classified as a normal readiness fix. It is a bounded experiment designed to measure whether a large fraction of fmParallel duplicate recomputation is caused by short-lived predecessor publication lag.

## Why the proposal is technically plausible

The earlier logs indicated that under fmParallel, many activations form stale recovery plans because predecessor outputs are still being produced by peers. Those stale plans later compute holes and lose at store time. The proposed bias attacks that timing window directly:

```text
large hole plan formed
sleep/yield briefly
re-plan after peer outputs may have published
request only sources for the kept plan
```

The key strength is placement: retry happens before source-frame requests for the discarded plan. If implemented exactly as scoped, dumped plans avoid both the source-request wave and the subsequent hole-compute path.

This means the experiment can reduce three forms of waste:

1. Recovery-hole source requests for plans that would later become stale.
2. Recovery-hole computation for holes that peers are about to publish.
3. Late duplicate-store rejection after redundant computation.

## Main expected benefit

The most likely successful outcome is not elimination of all duplicate work. The realistic benefit is a reduction in the worst fmParallel duplicate storms, especially at wider request windows such as `-r 8`.

The proposed success criterion is sensible:

```text
>= 5x reduction in duplicates at r8
for <= 20% fps loss
```

This criterion is useful because the experiment intentionally trades bounded latency for less duplicate work. It should be judged by the duplicate/fps tradeoff, not by raw fps alone.

## Main risks

### 1. It may merely throttle fmParallel

The sleep may reduce duplicate recomputation partly by slowing arInitial progress and narrowing the effective request wave. That is still useful as an experiment, but it is not the same as a precise coordination mechanism.

If performance improves while duplicates fall, that is a meaningful win. If duplicates fall only because fps collapses, the result should be treated as a negative experiment.

### 2. Sleeping in arInitial can occupy VapourSynth workers

A sleeping activation yields the CPU but still occupies a VS worker slot. This is bounded and not a busy loop, but it can reduce scheduler availability. The delivery note should explicitly state this.

This risk is especially important if `plan_retry_max = max(1, numThreads / 2)` permits many simultaneous sleeping activations. The implementation must confirm that no cache mutex or other shared lock is held during sleep.

### 3. It cannot distinguish in-flight holes from real holes

A large-hole plan may be caused by a genuine jump, cold cache, or other legitimate absence rather than in-flight predecessor lag. In those cases the sleep tax is pure latency.

This is why the experiment counters are important. They should show whether retries actually reduce kept-plan hole counts.

### 4. The threshold may be workload-sensitive

The proposed threshold `H=2` is plausible. However, `H=1` may reduce more waste while increasing sleep frequency, and higher thresholds may preserve throughput while missing some storms. The plan correctly allows an H=1 vs H=2 comparison.

### 5. The current `numThreads/2` retry count may be too large or too coarse

`max(1, numThreads / 2)` is defensible for a first experiment, but it ties retry depth to core configuration rather than observed recovery span or request window. It should be considered experimental, not a settled policy.

A later refinement could cap it explicitly, for example:

```text
plan_retry_max = min(4, max(1, numThreads / 2))
```

The current scope mentions 4 * 25 ms = 100 ms at 8 threads, which suggests an implicit cap may be desirable if very high thread counts are possible.

## Implementation requirements that should remain hard gates

The seven hard requirements in the scope are appropriate and should remain non-negotiable:

1. Macro OFF compiles to today's behaviour.
2. Sleep occurs with no cache mutex or other lock held.
3. Dumped plan destruction is complete and balanced.
4. `numThreads` is obtained once at filter creation and stored.
5. Worst-case added latency is bounded and documented.
6. Experiment counters are gated and observe-only.
7. `edit_version` is updated to the experimental marker while preserving the mode suffix.

The most important implementation audit points are:

```text
- no source requests issued for dumped plans;
- all pins released for dumped plans;
- recovery_plans_created and recovery_plans_destroyed still balance;
- no temporary output created by dumped plans;
- sleep occurs after all cache-core locks are released;
- macro-off diff is byte-identical where required;
- macro-on selftest remains unchanged because live plan routes are unreachable.
```

## Suggested refinements before coding

### Refinement 1: add pre/post retry hole counters

The proposed counters are good. I recommend adding two more if cheap:

```text
retry_original_plan_holes_total
retry_final_plan_holes_total
```

or, if keeping the counter set minimal:

```text
dumped_plan_holes_total
kept_plan_holes_total
```

Without both dumped and kept hole totals, it will be harder to prove that the retry ladder actually reduced plan size rather than merely delayed work.

### Refinement 2: count attempts by bucket

The scope proposes `plans_kept_on_attempt_1` and `plans_kept_on_attempt_2plus`. Consider splitting into:

```text
plans_kept_on_attempt_1
plans_kept_on_attempt_2
plans_kept_on_attempt_3plus
```

This would help identify whether a single sleep catches most cases or whether the retry ladder is paying repeated sleep tax.

### Refinement 3: print active knobs in the experiment summary

The `[DSUM-EXPERIMENT]` block should emit:

```text
enabled
sleep_ms
hole_threshold
plan_retry_max
plans_dumped_total
retry_sleeps_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2plus
dumped_plan_holes_total
kept_plan_holes_total
```

That makes logs self-describing and prevents later confusion about which knob values were compiled.

### Refinement 4: prove no accidental effect on fmParallelRequests/fmUnordered

The scope already requests fmUnordered `-r 1` macro ON. I would also consider one cheap `fmParallelRequests -r 4` macro ON run if time permits. The experiment is meant to target fmParallel duplicate recompute, but it could also unnecessarily sleep under fmParallelRequests no-r style stale planning. That would be useful to know.

## Suggested proof matrix

Minimum proof matrix:

```text
Macro OFF:
  Debug selftest normal
  Release selftest normal
  Release forced-fail harness
  Release verbose
  S8 byte-identical proof

Macro ON:
  Debug selftest normal
  Release selftest normal
  Release forced-fail harness
  Release verbose

Live measurement:
  fmUnordered -r 1, macro ON
  fmParallel -r 2, macro OFF and ON
  fmParallel -r 4, macro OFF and ON
  fmParallel -r 8, macro OFF and ON
  Optional: fmParallel -r 8 H=1 vs H=2
```

For live runs, collect:

```text
fps
duplicates_seen
incoming_rejected
frames_computed
bailed_after_compute
recovery_plans_created
holes_identified
source_frames_for_holes_requested
source_frames_for_holes_retrieved
recalculated_frame_count
plan retry experiment counters
```

## Brief effectiveness assessment

This proposal may be somewhat successful, and it is worth testing.

It is most likely to help if the fmParallel duplicate recomputation storm is dominated by short-lived publication lag: that is, holes are absent at plan-open time but become present within roughly 25-100 ms. The earlier behaviour strongly suggests that at least part of the waste has this shape.

It will not solve true concurrent ownership of in-flight frames. It is a timing bias, not a state model. Residual duplicate computation can still occur when two activations re-plan at the same time and both keep overlapping residual holes. It will also impose latency on genuinely absent holes.

The best possible result is a cheap mitigation that reduces duplicate storms enough to continue development while deferring the reservation table. The worst likely result is a controlled negative experiment showing that sleep-based bias is too blunt, thereby justifying the reservation/await design.

## Overall recommendation

Proceed with the patch as a gated experiment, with the refinements above if they are cheap. Do not present it as a fix. Present it as:

```text
A bounded plan-retry bias experiment that measures whether short-lived
predecessor publication lag is the dominant cause of fmParallel redundant
recomputation.
```

A PASS for this phase should mean:

```text
- macro OFF is behaviour-identical;
- macro ON is ownership-clean;
- no locks are held during sleep;
- dumped plans are fully destroyed;
- experiment counters prove what happened;
- live fmParallel measurements show whether duplicate recompute falls enough to
  justify the sleep tradeoff.
```

---

## Addendum A - recommended experiment counters and summary emission

This addendum is information-only and does not change the core scope unless the coordinator accepts it.

### A.1 Recommended searchable label

Use a new searchable summary label, separate from `[DSUM-PLANTRACE]`, for example:

```text
[DSUM-PLANRETRY]
```

Rationale:

```text
- It is short and grep/findstr-friendly.
- It clearly names the experiment.
- It avoids overloading PLANTRACE, which already means per-plan O/R timeline records.
- It can be fully gated under CNR3_EXPERIMENT_PLAN_RETRY_BIAS.
```

Alternative acceptable labels:

```text
[DSUM-EXPERIMENT-PLANRETRY]
[DSUM-PLAN-RETRY]
```

Preferred label:

```text
[DSUM-PLANRETRY]
```

### A.2 Counter set recommended for macro-ON builds

The original scope proposed:

```text
plans_dumped_total
retry_sleeps_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2plus
kept_plan_holes_total
```

I recommend extending that set to make the experiment self-proving:

```text
plan_retry_enabled
plan_retry_sleep_ms
plan_retry_hole_threshold
plan_retry_max

plan_retry_attempts_total
plans_dumped_total
retry_sleeps_total
plans_kept_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2
plans_kept_on_attempt_3plus
plans_kept_on_last_attempt

candidate_plans_over_threshold_total
candidate_plans_at_or_under_threshold_total

dumped_plan_holes_total
dumped_plan_holes_min
dumped_plan_holes_max
kept_plan_holes_total
kept_plan_holes_min
kept_plan_holes_max
hole_delta_from_dump_to_keep_total

source_requests_avoided_by_dumped_plans_estimate

retry_no_sleep_last_attempt_kept_total
retry_no_sleep_under_threshold_kept_total
retry_no_sleep_no_recovery_plan_total
```

### A.3 Minimal counter set if the above is too much

If the coder wants a very small first pass, use this minimum:

```text
plan_retry_enabled
plan_retry_sleep_ms
plan_retry_hole_threshold
plan_retry_max
plans_dumped_total
retry_sleeps_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2plus
dumped_plan_holes_total
kept_plan_holes_total
kept_plan_holes_max
source_requests_avoided_by_dumped_plans_estimate
```

The two most important additions beyond the original scope are:

```text
dumped_plan_holes_total
kept_plan_holes_total
```

Those prove whether retrying actually shrinks the plan population, rather than merely delaying the same number of holes.

### A.4 Counter definitions

Recommended definitions:

```text
plan_retry_enabled
    Printed as 1 when CNR3_EXPERIMENT_PLAN_RETRY_BIAS is compiled in.

plan_retry_sleep_ms
    The compiled CNR3_PLAN_RETRY_SLEEP_MS value.

plan_retry_hole_threshold
    The compiled CNR3_PLAN_RETRY_HOLE_THRESHOLD value.

plan_retry_max
    The instance retry bound computed at filter creation, e.g. max(1, numThreads / 2).

plan_retry_attempts_total
    Number of plan-formation attempts made under the retry loop. Includes attempt 1.

plans_dumped_total
    Number of freshly formed plans destroyed because hole_count > threshold and another attempt was allowed.

retry_sleeps_total
    Number of sleeps actually taken. Should equal plans_dumped_total unless future logic adds non-sleep dumps.

plans_kept_total
    Number of plans retained after the retry loop. Should equal the number of activations that entered the recovery-plan retry path and kept a recovery plan.

plans_kept_on_attempt_1
    Kept without sleeping.

plans_kept_on_attempt_2
    Kept after one dump/sleep/retry.

plans_kept_on_attempt_3plus
    Kept after two or more sleeps.

plans_kept_on_last_attempt
    Kept because the retry budget was exhausted even though hole_count may still be above threshold.

candidate_plans_over_threshold_total
    Number of formed plans with hole_count > threshold, including the last kept attempt if it remains over threshold.

candidate_plans_at_or_under_threshold_total
    Number of formed plans with hole_count <= threshold.

dumped_plan_holes_total
    Sum of hole_count for all dumped plans.

dumped_plan_holes_min / dumped_plan_holes_max
    Minimum/maximum hole_count among dumped plans. Print 0/0 if there were no dumped plans.

kept_plan_holes_total
    Sum of hole_count for all kept plans.

kept_plan_holes_min / kept_plan_holes_max
    Minimum/maximum hole_count among kept plans. Print 0/0 if there were no kept recovery plans.

hole_delta_from_dump_to_keep_total
    For activations with at least one dumped plan, add:
        first_dumped_plan_hole_count - kept_plan_hole_count
    Clamp only if necessary for unsigned safety; conceptually negative deltas should be visible if retry made a plan worse.

source_requests_avoided_by_dumped_plans_estimate
    Sum of source count that would have been requested by dumped plans if kept. For ordinary recovery plans this is usually related to dumped plan source list length, not necessarily equal to hole_count. Prefer exact planned source-list count if accessible; otherwise clearly name it as an estimate.

retry_no_sleep_last_attempt_kept_total
    Count of plans kept on the final allowed attempt because the retry budget was exhausted.

retry_no_sleep_under_threshold_kept_total
    Count of plans kept because hole_count <= threshold.

retry_no_sleep_no_recovery_plan_total
    Count of activations where no recovery plan was formed and therefore retry was irrelevant. Only add this if cheap and semantically clear.
```

### A.5 Suggested summary block

Emit near the existing D-SUM summaries, only under `CNR3_EXPERIMENT_PLAN_RETRY_BIAS`:

```text
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plan-retry bias experiment summary
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] interpretation: gated timing-bias experiment; dumped plans are destroyed before source requests; sleeps must occur without cache locks held
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] enabled                                      1
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] sleep_ms                                     <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] hole_threshold                               <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plan_retry_max                               <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plan_retry_attempts_total                    <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] candidate_plans_over_threshold_total         <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] candidate_plans_at_or_under_threshold_total  <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_dumped_total                           <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] retry_sleeps_total                           <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_kept_total                             <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_kept_on_attempt_1                      <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_kept_on_attempt_2                      <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_kept_on_attempt_3plus                  <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] plans_kept_on_last_attempt                   <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] dumped_plan_holes_total                      <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] dumped_plan_holes_min                        <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] dumped_plan_holes_max                        <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] kept_plan_holes_total                        <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] kept_plan_holes_min                          <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] kept_plan_holes_max                          <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] hole_delta_from_dump_to_keep_total           <value>
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] source_requests_avoided_by_dumped_plans_estimate <value>
```

### A.6 Suggested self-checks for the summary block

If cheap, add self-check lines:

```text
plans_kept_total == plans_kept_on_attempt_1 + plans_kept_on_attempt_2 + plans_kept_on_attempt_3plus
retry_sleeps_total == plans_dumped_total
plan_retry_attempts_total == plans_kept_total + plans_dumped_total
plans_dumped_total == 0 when macro ON but no recovery plans form
```

Suggested emitted form:

```text
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] self-check: kept attempt buckets sum to plans_kept_total (%llu vs %llu) -> OK
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] self-check: retry_sleeps_total equals plans_dumped_total (%llu vs %llu) -> OK
CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] self-check: attempts equal kept + dumped (%llu vs %llu) -> OK
```

Use WARN only if these fail; failing here would indicate the experiment counters are unreliable.

### A.7 Suggested `findstr` additions

Add these patterns to the summary extraction list:

```text
DSUM-PLANRETRY
plan_retry_attempts_total
candidate_plans_over_threshold_total
candidate_plans_at_or_under_threshold_total
plans_dumped_total
retry_sleeps_total
plans_kept_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2
plans_kept_on_attempt_3plus
plans_kept_on_last_attempt
dumped_plan_holes_total
dumped_plan_holes_min
dumped_plan_holes_max
kept_plan_holes_total
kept_plan_holes_min
kept_plan_holes_max
hole_delta_from_dump_to_keep_total
source_requests_avoided_by_dumped_plans_estimate
```

---

## Appendix B - information-only verdict

This appendix is not a coding requirement unless accepted by the coordinator.

### B.1 Brief verdict

The plan-retry bias proposal is worth testing as a gated experiment. It is a plausible brute-force mitigation for fmParallel duplicate recomputation because it directly targets the observed timing window: a recovery plan is formed while peer activations are still computing predecessor outputs that may be published shortly afterward.

The proposal should not be described as a fix. It is a polling/yield approximation of an in-flight reservation or await table. It can reduce waste when holes are short-lived publication lag; it cannot prove ownership coordination or eliminate simultaneous claims on residual holes.

### B.2 Why it may help

It may help if most fmParallel hole recomputes are caused by this pattern:

```text
T0: arInitial forms a hole-heavy recovery plan.
T1: peer activations are already computing some of those holes/predecessors.
T2: retry sleep yields long enough for peers to publish.
T3: re-plan sees a nearer anchor/fewer holes.
T4: kept plan requests fewer source frames and computes fewer recovery holes.
```

Because the retry occurs before source requests for the discarded plan, a successful retry can reduce both source-request pressure and duplicate compute pressure.

### B.3 Why it may fail or mislead

It may fail if the missing holes are not mostly in-flight peer outputs. In that case the sleep merely delays work. It may also appear to succeed by throttling fmParallel enough to narrow the request wave, which is useful to know but is not equivalent to solving the underlying coordination problem.

The decisive measurement is not just fps. The decisive measurement is:

```text
duplicates_seen / incoming_rejected / bailed_after_compute fall sharply,
frames_computed falls toward the output-frame count,
kept_plan_holes_total falls relative to dumped_plan_holes_total,
fps does not collapse,
byte output remains identical to the baseline.
```

### B.4 Recommended success language

If the experiment succeeds, use language like:

```text
The gated plan-retry bias substantially reduces fmParallel redundant recompute
pressure under the tested request windows. It is a useful mitigation and a
measurement of short-lived predecessor publication lag, but it is not a final
replacement for an in-flight reservation/await mechanism.
```

If it fails, use language like:

```text
The gated plan-retry bias did not sufficiently reduce redundant recomputation
within the allowed fps budget. The result supports proceeding to the explicit
in-flight reservation/await design rather than further timing-bias tuning.
```
