# CNR3 — PATCH SCOPE: plan-retry biasing experiment (gated) — v3

**Marker on success:** `CMS07-EXPERIMENT.plan-retry-bias`
**Baseline:** current committed tree (`CMS07-SCAFFOLD.filter-mode-selector`).
**Track:** gated EXPERIMENT (Ruling-5 / TINY-scaffold class: behaviour-affecting ONLY when its macro is
enabled; committed default = macro OFF = behaviour identical to today, provably).

## 1. Goal, plain English

Under fmParallel, activations plan while their predecessors are still being computed by peers, producing
hole-heavy recovery plans whose holes get redundantly recomputed and discarded at the store race
(measured duplicates 197 / 575 / 3550 at -r 2/4/8). This experiment biases plan formation: if a freshly
formed plan has "too many holes", DUMP it, SLEEP briefly (yielding the core), and RE-PLAN — because within
a few frame-times the in-flight predecessors will have been stored, and the re-formed plan will adopt
instead of recompute. It is a polling approximation of the reservation/await mechanism (research scheme #1)
using only existing machinery. It is a BIAS, not a fix: it cannot distinguish in-flight holes from genuinely
absent ones, and races on the residual holes remain possible.

## 2. The gate and knobs (cnr3_build_config.h, near the other scaffolds)

```cpp
/*  PLAN-RETRY BIASING EXPERIMENT (fmParallel redundant-recompute mitigation).
    Uncomment to enable. OFF (default) = plan formation is single-pass, exactly today's behaviour.
    ON = arInitial may retry plan formation up to RETRY_MAX times, sleeping RETRY_SLEEP_MS between
    attempts, whenever a formed plan contains more than RETRY_HOLE_THRESHOLD holes. The LAST attempt's
    plan is always kept (no infinite loop; worst-case added latency = RETRY_MAX * RETRY_SLEEP_MS).      */
//#define CNR3_EXPERIMENT_PLAN_RETRY_BIAS 1

#if defined(CNR3_EXPERIMENT_PLAN_RETRY_BIAS)
/*  Adjustable experiment parameters (exist ONLY when the experiment is enabled):
    CNR3_PLAN_RETRY_SLEEP_MS       - yield-sleep between plan attempts (true sleep, releases the core).
    CNR3_PLAN_RETRY_HOLE_THRESHOLD - a freshly formed plan with MORE than this many holes is dumped
                                     and re-attempted (H=2 permits limited early out-of-order computes
                                     where the bail-early/adopt mechanisms may save recompute later).
    CNR3_PLAN_RETRY_MAX is NOT a #define: derived at filter creation as
    min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads/2)).                                                 */
#   define CNR3_PLAN_RETRY_SLEEP_MS        25
#   define CNR3_PLAN_RETRY_HOLE_THRESHOLD  2
#   define CNR3_PLAN_RETRY_MAX_CAP         4
#endif
```

Note on the three knobs: `CNR3_PLAN_RETRY_MAX_CAP` is the UPPER BOUND on retry attempts; the actual
per-instance `plan_retry_max` is derived at filter creation as
`min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads/2))`. So the cap lets you sweep retry depth (e.g. 2/4/6)
without touching code, while still scaling down on low-core machines. Worst-case added latency per frame
= `plan_retry_max * CNR3_PLAN_RETRY_SLEEP_MS` (<= CAP * SLEEP_MS = 100ms at the defaults).

Knob values per W3X: S=25ms, H=2 (H=1 to be compared empirically), CAP=4; N derived = min(CAP, max(1, numThreads/2)).

## 3. Mechanism (arInitial only; no change inside plan formation itself)

```
attempts_allowed = instance.plan_retry_max   // computed once at create: min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads/2))
for attempt in 1..attempts_allowed:
    form plan (existing code path, existing mutexing, UNCHANGED)
    if plan.hole_count > H and attempt < attempts_allowed:
        destroy plan cleanly (release pins; recovery_plans_destroyed must still balance)
        sleep S ms                                  // std::this_thread::sleep_for — true yield, no busy loop
        continue
    else:
        keep this plan; break
proceed with the kept plan exactly as today (source requests issued only for the KEPT plan)
```

Key placement fact: the retry decision happens BEFORE arInitial issues the plan's source-frame requests, so
dumped plans also avoid their ~span-many source fetches (a secondary saving).

## 4. Hard requirements (coder confirm-report must address each)

1. **Macro OFF compiles to today's BEHAVIOUR.** The loop, knobs, counters, and any helper must vanish
   under the gate. Precisely (coder cleanup accepted): the R-PROCESS-19 proof is that macro-off S8 FRAME
   OUTPUT is byte-identical to the prior commit; the binary and logs are NOT literally identical (the
   edit_version marker changes) — frame bytes are the gated fact. With the macro ON but fmUnordered -r 1
   (no holes ever), behaviour must also be unchanged (loop exits on first pass).
2. **The sleep must occur with NO cache mutex (or any lock) held.** Confirm cold that plan formation's
   locks are fully released before the sleep. Sleeping while holding the cache lock would stall every
   other activation — the exact opposite of the intent.
3. **Plan destruction on dump is complete:** pins released, plan create/destroy counters balance
   (D-SUM-12 recovery_plan_balance stays 0), no source requests issued for dumped plans, no temp outputs
   created. Confirm the existing destroy path is reusable as-is or state what is missing.
4. **numThreads:** obtain once at filter creation via vsapi->getCoreInfo(core, &info).numThreads; store
   plan_retry_max = **min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads/2))** in instance data (the cap is a
   compile-time knob, default 4, so retry depth can be swept without code changes while still scaling down
   on low-core machines — W3D ruling on coder risk #5). Confirm the API call and struct field cold.
5. **Bounded worst case:** total added latency per frame <= plan_retry_max * SLEEP_MS (<= 4 * 25 = 100ms).
   A sleeping activation occupies a VS worker — bounded and deadlock-free (sleep always wakes), but state
   this in the delivery note.
6. **Experiment counters (gated under the same macro, observe-only, count-never-compute).** Emit a
   `[DSUM-PLANRETRY]` block (separate label from PLANTRACE, fully gated). The RULED counter set (coder's
   A.3 minimal, minus the estimate counter which is imprecise by the coder's own note — OUT):
   ```
   plan_retry_enabled / plan_retry_sleep_ms / plan_retry_hole_threshold / plan_retry_max_cap / plan_retry_max   (echo knobs; plan_retry_max is the DERIVED value = min(cap, max(1, numThreads/2)))
   plan_retry_plan_attempts_total   (total candidate plans formed while the experiment path was active,
                                     INCLUDING attempt-1 plans with zero holes and all dumped/retried candidates)
   plans_dumped_total
   retry_sleeps_total
   plans_kept_on_attempt_1 / plans_kept_on_attempt_2 / plans_kept_on_attempt_3plus
   dumped_plan_holes_total          <- REQUIRED (the key pair)
   kept_plan_holes_total            <- REQUIRED (the key pair)
   ```
   dumped_plan_holes_total vs kept_plan_holes_total is the load-bearing measurement: it distinguishes
   "retry SHRANK the plans" from "retry merely DELAYED the same holes" — the experiment's core question.
   Print-only self-checks (never wired to selftest):
   `plan_retry_plan_attempts_total == plans_dumped_total + plans_kept_on_attempt_1 + plans_kept_on_attempt_2 + plans_kept_on_attempt_3plus`;
   and `retry_sleeps_total == plans_dumped_total`. WARN only on failure (never alters output/return/cache).
   EXPLICITLY DEFERRED (too much machinery for a throwaway probe): min/max hole counts,
   hole_delta_from_dump_to_keep (unsigned-clamp subtlety per coder), candidate_plans_* buckets, the three
   retry_no_sleep_* breakdowns, source_requests_avoided_estimate (imprecise). If the experiment PASSES and
   is promoted to a kept mechanism, revisit the fuller A.2 set then.
7. **edit_version** -> `CMS07-EXPERIMENT.plan-retry-bias` (mode suffix mechanism unchanged; marker will
   read `...plan-retry-bias:fmParallel` etc.).

## 5. Proof gate

1. Canonical 4-way, macro OFF: 56/56 unchanged. Macro ON: 56/56 unchanged (selftest never forms live
   plans; confirm the experiment path is unreachable from selftest routes).
2. R-PROCESS-19 macro-off byte-identical (S8 fc /b) — proves the gate.
3. **The measurement (the point of the experiment): fmParallel -r ladder, macro ON, NORMAL cache,
   same 200-frame clip.** Baseline duplicates (macro OFF): r2=197, r4=575, r8=3550. Success criterion
   (W3D): >=5x reduction in duplicates at r8 for <=20% fps loss. Report duplicates, frames_computed,
   fps, AND the [DSUM-PLANRETRY] counters at r2/4/8; plus one fmUnordered -r1 run macro ON proving no
   change where no holes form. H=1 vs H=2 comparison at r8 if time permits.
   Also one **fmParallelRequests -r4 macro ON** run (coder refinement 4): that mode already had ZERO
   waste, so if the experiment makes it dump/sleep, that is added latency for no benefit — a mild strike
   worth knowing. Expect near-zero plans_dumped there.
   **THROTTLE-VS-ADOPT GUARD (critical interpretation rule):** a duplicate drop that comes with a large
   fps collapse is the sleep merely THROTTLING fmParallel (narrowing the request wave), NOT smart
   adoption. Distinguish via the counters: real success shows kept_plan_holes_total FALLING relative to
   dumped_plan_holes_total (plans genuinely got smaller on re-plan). If duplicates fall only because fps
   collapsed and kept-holes did NOT shrink vs dumped-holes, record it as a NEGATIVE experiment (justifies
   the reservation table), not a win.
4. Run-log emission checks per R-PROCESS-27 (marker + filter_mode lines present).

## 6. What this is NOT

Not the reservation table (research #1). It cannot wake at the exact publish moment, cannot distinguish
in-flight from genuinely-absent holes (it pays the sleep tax on cold holes), and leaves the simultaneous-
claim race open on residual holes. If the ladder shows it insufficient, the reservation table is the
designed follow-up; if it shows it sufficient, we bank a cheap win. Either result also measures what
fraction of the waste is "predecessor arrives within a few frame-times" — exactly the fraction scheme #1
would capture with finer machinery.
