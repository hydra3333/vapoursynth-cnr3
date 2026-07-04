# CNR3 — DESIGNER REVIEW: DIAG.3a patch (CMS07-DIAG.3a-recovery-rate-recalc.patch)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** the DIAG.3a patch (1730 lines, 7 files) + patch notes, 2026-07-04
**Review basis:** full diff read, load-bearing claims verified against post-DIAG.2b source.

---

## VERDICT: ONE BLOCKING DEFECT (D-1). Fix is a single call-site insertion. Everything else PASSES.

The patch is otherwise strong — B1-B5 correctly applied, D-SUM-13 open-addressed table is sound,
gating uniform, observe-only respected. But one defect breaks the D-SUM-12 balance that the whole
family's completeness rests on. Fix it, re-run the gate, then it commits.

## D-1 (BLOCKING) — the D-SUM-12 destroy observer is defined but NEVER invoked

```text
EVIDENCE:
- cnr3_diag_live_observe_recovery_plan_destroyed_if_needed() appears EXACTLY ONCE in the patch — its
  definition. There is ZERO call site anywhere in production code.
- The only cnr3_diag_dsum12_observe_recovery_plan_destroyed() call in a code path (patch line ~502) is
  inside the SELFTEST synthetic reference emitter (hardcoded values, PRINT-gated) — it balances the
  synthetic block only, NOT the production teardown.
- cnr3_discard_frame_data_with_cache() — the single real teardown (one delete request_data, ~51 routing
  call sites) — is CALLED at 15+ sites but its DEFINITION is not modified by the patch, so the destroy
  observer was never wired into it.

CONSEQUENCE:
- recovery_plans_created increments at the recovery publish (correct, B1).
- recovery_plans_destroyed NEVER increments in production.
- recovery_plan_balance = created - destroyed will read POSITIVE (== recovery count), not 0.
- The S-series acceptance gate (recovery_plan_balance == 0) FAILS on any scenario with recoveries
  (S3/S7/S8). The completeness proof the D-SUM-12 balance exists to provide is not closed.

WHY IT PASSED SANDBOX: the function compiles (it is defined and syntactically valid), and the selftest
exercises the underlying observe_recovery_plan_destroyed() with synthetic values, so a syntax check and
a shallow read both pass. Only tracing the PRODUCTION call graph reveals the destroy side is orphaned.
This is the "verify invocation, not just definition" check.
```

### The fix (single insertion, mirrors the already-correct fallback/honesty wiring)

The patch already wires the sibling observers `..._fallback_failure_if_needed()` and
`..._honesty_failure_if_needed()` at their sites (5 + 5 calls, verified). The destroy observer must be
wired the same way — invoked once on the teardown path, before the frameData is discarded. Two options,
either acceptable; Option A is cleanest:

```text
OPTION A (preferred): invoke inside cnr3_discard_frame_data_with_cache() itself, before delete
  request_data, gated on CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN. Because this is the SINGLE delete site
  with ~51 routing callers, one insertion here covers every teardown path (incl. all bails) by
  construction — the exact structural guarantee that made this balance provable (designer response §1).
  Requires the function to see the request_data before delete; it already casts *frame_data to
  Cnr3LiveGetFrameFrameData* at that point, so the destroyed_if_needed(*request_data) call drops in
  directly before `delete request_data;`.

OPTION B: invoke destroyed_if_needed(*request_data) immediately before EACH cnr3_discard_frame_data_
  with_cache(...) call site (15+ sites). Correct but fragile (a future added discard site would miss it)
  and violates the "single teardown" elegance. Only use if Option A is somehow blocked.

REQUIRED: destroyed_if_needed is idempotent-safe via the dsum12_recovery_plan_stats != nullptr guard
  (already in its body), so it fires exactly for published recovery plans and is a no-op otherwise —
  so Option A is safe even though discard runs for non-recovery frameData too.
```

Note the destroy observer must run for the plan EVEN ON non-bail success paths (a recovery that
completes normally is still "destroyed" at teardown). Option A gets this automatically; verify the
success path also routes through cnr3_discard_frame_data_with_cache (it does — the normal completion
discards the frameData like every other path).

## What PASSES (verified against source)

```text
B1 (created side): correct — recovery_plans_created hooked ONLY at the recovery publish (arInitial,
   branch-keyed), NOT at the three non-recovery publishes (which correctly get branch-RATE observers
   observe_branch_cache_hit/pred_present/frame0 feeding frames_total). The created side is right; only
   the destroy side (D-1) is broken.
B2: holes_identified/filled + hole-source counters use recovery_plan.hole_frame_numbers only; target/
   floor/direct excluded. Correct.
B3 (D-SUM-13 container): CORRECT and sound. Fixed-capacity open-addressed table, bounded probe loop
   (probe < CAPACITY — cannot infinite-loop), insert-if-empty / update-if-match / saturate-on-full-loop,
   compute_count capped at UINT16_MAX, depth clamped to uint8, saturating dropped-counter,
   compute_count_map_saturated flag + writer "lower bounds if saturated" line. Capacity 16000 normal /
   1600 tiny per the derivation. Per-instance ~128KB acknowledged in notes. No unbounded growth, no
   overflow. APPROVED.
B4: recalculation depth = distance from anchor/floor; direct/fresh-start depth 0. Correct.
B5: in-run ring-correlation deferred; external correlation only. Correct.
D-SUM-03: 13 observe calls wired (invocation/termination/holes_filled). Gated (10 guards).
D-SUM-12 rate: 16 branch/publish observe calls; fallback (5) + honesty (5) failure observers WIRED at
   bail sites (observe-only, before the existing return — additive, no control-flow change). Gated (23).
Five-config gating present for all three families (DSUM03/12/13 compute guards throughout).
Observe-only: no return/status/control-flow altered at any site; failure observers read outcome only.
```

## Revised proof gate (after D-1 fix)

```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-03/12/13 blocks emit.
2. R-PROCESS-19 five-config matrix: all on; 03 off; 12 off; 13 off; all three off — clean + four-way
   identical + family block absent when off. (Temporary build_config edits not committed.)
3. S-series -r 1 (S1/S3/S7/S8) — THE D-1 REGRESSION CHECK:
   - recovery_plan_balance == 0 on ALL FOUR (this is what D-1 breaks; must be 0 after the fix — on
     S3/S7/S8 which HAVE recoveries, this proves created==destroyed under real churn).
   - recovery_rate: S1 ~0% (in-order control); S7/S8 deterministic exact/floor baseline.
   - compute_count_map_saturated == FALSE on all four.
   - prior families unchanged: D-SUM-04 balances 0, D-SUM-05 violations 0, D-SUM-08 failures 0.
4. Read D-SUM-12 recovered targets vs D-SUM-10 ring dumps = FI-11 external correlation.
```

## Instruction to coder

```text
Apply fix D-1 (Option A preferred): invoke cnr3_diag_live_observe_recovery_plan_destroyed_if_needed(
*request_data) inside cnr3_discard_frame_data_with_cache() immediately before `delete request_data;`,
gated on CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN. This is the single teardown covering all ~51 paths, so
one insertion closes the balance for every path including bails. The observer's nullptr guard makes it a
no-op for non-recovery frameData, so it is safe at this shared site. Regenerate the patch; no other change
required — the rest of the patch is approved. Then the coordinator runs the revised proof gate; the
S-series recovery_plan_balance == 0 is the specific D-1 regression check.
```
