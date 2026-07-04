# CNR3 — DESIGNER RESPONSE: DIAG.3a Confirm Report + Plan/Result Cross-Check (2026-07-04)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** `CNR3_DIAG3a_Coder_Confirm_Report_to_Designer_2026-07-04.md` + `CNR3_DIAG3a_PlanResult_Cross_Check_Report_2026-07-04.md`
**Status:** CONFIRM REPORT ACCEPTED with amendments B1-B5. Cross-check ACCEPTED (review-only, non-gating).
**Outcome:** PROCEED to DIAG.3a patch generation per the report's §6 shape + the amendments below.

---

## 0. Sync note (coordinator)

The coordinator flagged a possible process de-sync ("coder advised to proceed"). For the record, the
correct DIAG.2b-style ordering is: confirm report -> designer review (this document) -> patch. The coder
DID deliver a confirm report (not a patch), so the discipline held on the coder side. This response is the
designer review that authorizes patching. If any "proceed" instruction reached the coder ahead of this
review, it is now superseded/ratified by this document — the coder should treat THIS as the authority for
what to build, and any earlier verbal go-ahead as consistent with it. No rework implied; the confirm report
is exactly what was wanted at this stage.

## 1. Verification statement (designer independently checked against post-DIAG.2b source)

```text
VERIFIED TRUE:
- D-SUM-12 balance is PROVABLE: exactly ONE `delete request_data` site (arAllFramesReady:453) inside
  cnr3_discard_frame_data_with_cache(); 51 call sites route through it. So a recovery plan published in
  arInitial is guaranteed to hit that single teardown on EVERY path incl. all bails. The coder's
  create@publish / destroy@teardown(branch==recovery) proposal therefore balances by construction — the
  D-SUM-12 analogue of the single-freeFrame reasoning that made D-SUM-04 provable. CONFIRMED.
- D-SUM-01 pattern exactly as described: stats in Cnr3FilterData (plugin_internal.h:40-42), mutable
  diagnostics-only std::mutex (diagnostics.h:110), observe locks ONLY the diag mutex, snapshot-then-
  release-then-format. CONFIRMED — mirror it, per-family own struct + own mutex, no shared mutex.
- Five-way branch enum exists (Cnr3LiveGetFrameBranch + Cnr3LiveRecoveryBranch). CONFIRMED.
- D-SUM-13 capacity constants exist (CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
  CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 100 tiny / 1000 normal). CONFIRMED.

REFINEMENT (designer check the report did not surface):
- There are FOUR `*frame_data = request_data` publish sites in arInitial (57, 94, 121, 451), not one.
  Sites 57/94/121 are the NON-recovery branches (cache_hit / frame0 / predecessor_present early requests);
  451 is the recovery-branch publish. The coder cited only 451. The BALANCE IS STILL CORRECT because
  recovery_plans_created increments only at the recovery publish AND destroy is gated on branch==recovery
  — the three non-recovery publishes neither increment created nor match the destroy gate. But the patch
  must key the created-hook on the RECOVERY branch specifically (site 451), NOT on "the publish site,"
  since publish is not unique. See B1. This is the one place the report's framing was imprecise; the
  correctness survives the gating, but the implementation must be branch-keyed to stay correct.
```

## 2. Answers to the report's five questions + amendments B1-B5

```text
Q1 accepted -> B1: YES, accepted/published recovery plans only. recovery_plans_created ++ at the RECOVERY
   branch publish (arInitial:451, branch==recovery), NOT at every *frame_data=request_data (4 sites).
   recovery_plans_destroyed ++ in cnr3_discard_frame_data_with_cache when branch==recovery. Do NOT count
   scratch Cnr3CacheRecoverySearchPlan value-constructions. Balance = created - destroyed, MUST be 0 at
   teardown (the S-series backstop, as with D-SUM-04). Scratch-plan lifecycle NOT wanted for 3a.

Q2 accepted -> B2: holes_identified / holes_filled / source_frames_for_holes_* count
   recovery_plan.hole_frame_numbers ONLY — exclude target n, exclude floor base. Floor fresh-start:
   keep separate floor counters (recovery_span folding: exact uses requested-anchor span; floor counted
   separately, per scope). Agreed exactly as proposed.

Q3 accepted -> B3: D-SUM-13 container = fixed-capacity open-addressed table, capacity
   max(B, active_ceiling_max) * 16 floor 1024 (normal 16000 / tiny 1600), linear probing, per-instance
   diag-mutex-protected, saturation-honest (compute_count_map_saturated + compute_count_observations_
   dropped, writer prints "counts are lower bounds if saturated"). Matches the D-SUM-10 ring derivation
   discipline. APPROVED. One constraint: the std::array<...,16000> lives in Cnr3FilterData PER INSTANCE
   — confirm the per-instance footprint (16000 * sizeof(entry) ~ 128KB/instance) is acceptable; it is for
   diagnostics builds (compiles out when the gate is off), but note it in the patch so it is not a surprise.

Q4 accepted -> B4: depth = recovery distance from anchor/floor; direct (cache_hit/frame0/pred_present)
   and fresh-start depth 0. recalculation_depth_histogram[7] bins acceptable. Agreed.

Q5 accepted -> B5: DEFER the in-run DSUM10+DSUM12 ring-correlation counter. The coder's feasibility
   finding is correct and independently verified: it would require a cross-family cache snapshot copy or
   cache-internal read from getFrame-side, breaking the "no cache-core change" DIAG.3a boundary. External/
   offline correlation (D-SUM-12 recovered targets vs D-SUM-10 [DSUM10-RING-*] dumps) is the clean 3a
   baseline. Record the deferred in-run counter as a note under FI-11 (it is the same investigation).
```

## 3. D-SUM-12 field-mapping (report §4.4) — accepted, two confirmations

The source-grounded mappings are accepted. Two low-risk confirmations for the patch (not blockers):
```text
- fallback_failures and bounded_start_honesty_failures observe at ACCEPTED-recovery arAllFramesReady
  failure sites (the report's cited 1001-1039 / 1066-1182 regions). These are observe-only reads of the
  outcome — they must NOT alter the bail. Apply the multi-exit exactly-once discipline (DIAG.2b §3.2).
- source_frames_for_holes_retrieved at the hole getFrameFilter success (1271-1275), excluding target
  (1435) and floor (1098-1102) — agreed; keep those three retrieval sites distinct.
```

## 4. Plan/Result cross-check — ACCEPTED, review-only, NON-GATING

The cross-check is exactly the parallel deliverable requested. It does NOT expose a DIAG.3a correctness
issue (verified: its findings are all about the FUTURE plan-trace family, not the 03/12/13 telemetry), so
it does not gate the 3a patch. Its five amendments are accepted as the STARTING POINT for the plan-trace
family when it is eventually scoped (a DIAG.3-family item, post-3a):
```text
- sources must be branch-specific (cache_hit/frame0/pred_present request n without populating
  source_request_frame_numbers) — a real gap in the spec's Set 2; fold into the spec at implementation.
- pinned/unpinned as DERIVED facts, no generic pin-list enumerator unless a new accessor is approved.
- a source-line site-to-failure-category TABLE before touching bail paths (not a message parser).
- consider adding ALLOCATION_FAILED / RECOVERY_PLAN_FAILED_OR_REFUSED / HOT_ZONE_OBSERVATION_FAILED to
  the ~13 categories (the report notes >50 setFilterError sites once arInitial + top-level are included —
  the "~50" in the spec was cache/AR-side only; accept the correction).
- compile-time from/to for the first implementation (mirrors the diagnostic gate style, easier macro-off
  proof) — this ANSWERS the spec's open question; record it as decided.
- dump-on-bail is the invasive part -> its own future patch + R-PROCESS-21 review, NOT 3a.
```
These get folded into the plan/result spec (a v2) when the plan-trace family is scoped. For now: recorded,
not implemented. No action in DIAG.3a.

## 5. Proof gate (unchanged from scope §7, restated)

```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-03/12/13 [DSUM-SUMMARY] blocks emit.
2. R-PROCESS-19 five-config matrix: all on; 03 off; 12 off; 13 off; all three off. Each clean + four-way
   identical + family block absent when off. Temporary build_config edits not committed.
3. S-series -r 1 (S1/S3/S7/S8):
   - D-SUM-12 recovery_plan_balance == 0 on all four (the create/destroy completeness backstop).
   - recovery_rate shapes: S1 ~0% (in-order control), S7/S8 the deterministic exact/floor baseline.
   - D-SUM-13 compute_count_map_saturated == FALSE on all four (capacity holds); recalc counts consistent
     with the churn (S7/S8 show recalculation; S1 ~0).
   - Read D-SUM-12 recovered targets vs D-SUM-10 ring dumps = the FI-11 external correlation.
4. Prior families (01/04/05/08/10/11) unchanged.
```

## 6. Authorization

Confirm report accepted with B1-B5. **Generate the DIAG.3a patch** (D-SUM-03/12/13 only) against the
post-DIAG.2b baseline, per report §6 shape + B1-B5. Deliver the .patch + patch-notes + the usual apply
command block; designer reviews the diff before build/commit — hardest on: (a) the D-SUM-12 created-hook
being branch-keyed at the recovery publish (not the generic publish site) and the balance netting to zero;
(b) the D-SUM-13 open-addressed table's bounds/saturation handling and per-instance footprint; (c) five-
config compile-out; (d) observe-only-ness at the fallback/honesty failure sites (no bail altered).
```
