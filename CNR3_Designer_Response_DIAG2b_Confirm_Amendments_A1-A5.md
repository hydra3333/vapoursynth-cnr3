# CNR3 — DESIGNER RESPONSE to the DIAG.2b Targeted-Confirm Report (2026-07-04)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**To:** coder (W3C)
**Re:** `CNR3_DIAG2b_Targeted_Confirm_Report_to_Designer_2026-07-04.md`
**Status:** REPORT ACCEPTED. All §8 confirms verified independently against the same src baseline.
**Outcome:** PROCEED to DIAG.2b patch generation per §7 of your report, subject to the numbered
amendments below (A1-A5). This response + the v2 scope together are the controlling pair; no v3
scope will be issued (the deltas are small and precise — regenerating the scope risks drift).

---

## 1. Verification statement (designer's independent check)

Before accepting, the designer independently re-verified the load-bearing claims against the current
source baseline. All held:

```text
1. Cnr3Status::duplicate is a distinct status; cnr3_status_is_ok() is true only for ok; and
   cnr3_live_store_status_allows_return() returns (ok || duplicate) — the §5.3 finding is REAL.
2. lookup_frame_and_add_ref_locked() has EXACTLY ONE call site (the public wrapper at
   cnr3_cache_core.cpp:1538) — the §3.2 single-caller/completeness claim is verified.
3. The public wrapper's success-adopt (reset_to_owned_frame) and failure-freeFrame branches are as
   cited — the three lookup-ref counter sites are correct and complete.
4. cache_state_invariants_hold_locked() has 18 early `return false` sites and one final `return true`
   — the §4.2 early-returns finding is real and material (a single end-hook would miss violations).
5. All four pin-release delegations to unpin_frame_locked() verified at the cited lines (1598, 1618,
   2669, 3603); no other release path exists.
6. ADDITIONAL designer check the report did not cover: the only other `slot.pin_count` assignment in
   cache_core (line 2553, `slot.pin_count = 0;`) was traced — it is the initialisation of a freshly
   constructed local Cnr3CacheSlot inside store_owned_frame_locked() before push_back, NOT a zeroing
   of a live pinned slot. It has no effect on the pin balance. The ++/-- at 3660/3713 remain the only
   balance-relevant mutations. Your completeness claim therefore survives a check you did not know
   would be run — noted with approval.
```

## 2. AMENDMENT A1 — D-SUM-08 `store_failures` definition (your §9 question): ACCEPTED

Decision: **YES — count only genuine failures; exclude duplicate.**

```text
store_failures += (store_status != Cnr3Status::ok && store_status != Cnr3Status::duplicate)
```

Rationale confirmed: `duplicate` is an allowed, healthy first-in-best-dressed outcome eligible for
authoritative return; counting it as failure would false-report correct behaviour. `duplicates_seen`
and `incoming_rejected` already carry the duplicate telemetry separately, so no information is lost.

Your layering judgement is also accepted: do NOT call `cnr3_live_store_status_allows_return()` from
cache diagnostics (it is plugin-integration-layer). The cache layer counts the two statuses directly
as above. The v2 scope's literal `store_status != ok` wording is hereby amended.

## 3. AMENDMENT A2 — D-SUM-05 return-observer: APPROVED, in the MACRO form, with exact semantics

Your §4.2 finding is accepted and the observation pattern is sanctioned under R-PROCESS-21 as a
DESIGNER-APPROVED mechanical transformation (this paragraph is the explicit approval that rule
requires). Implement it in the following exact form, chosen to keep the diff minimal and reviewable:

```text
SEMANTICS:
  invariant_checks_performed  ++ ONCE per invocation, at function ENTRY (one gated hook line at the
                                 top of cache_state_invariants_hold_locked()). Do NOT count per
                                 return site — the metric is "invariant checks performed", i.e.
                                 invocations of the central predicate.
  invariant_violations_detected ++ at each `return false` site, via the macro below.
  first_violation_site          set (once, if currently null) to the site tag at the first false.

FORM (one definition site, near the other DSUM gates in the diagnostics header or cache_core.cpp):
  #if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
  #   define CNR3_DSUM05_FAIL(tag) \
          return observe_cache_invariant_failure_locked((tag))   /* records violation+site, returns false */
  #else
  #   define CNR3_DSUM05_FAIL(tag) return false
  #endif

CALL SITES: each of the 18 `return false;` lines inside cache_state_invariants_hold_locked() becomes
  CNR3_DSUM05_FAIL("short_stable_tag");
— a one-line mechanical change per site; macro-off expands to the literal original `return false`.
The final `return true` is NOT touched (checks_performed is counted at entry).

CONSTRAINTS on observe_cache_invariant_failure_locked():
  - const-callable (mutable member), noexcept, returns false unconditionally;
  - updates ONLY the D-SUM-05 diagnostic counters/site tag;
  - no allocation, no formatting, no printing, no lock acquisition, no recursion into the predicate;
  - fully compiled out (declaration and definition) when the compute macro is undefined.

TAGS: short stable literals per check block, e.g. "hot_zone_model", "frame_index_entry",
  "slot_state", "checkpoint_positions", "checkpoint_slot_class" — coder derives one per block; the
  exact strings are the coder's choice, held stable thereafter (they appear in the summary output).
```

Why the macro form over per-site `#if/#else` blocks: 18 four-line ifdef blocks is diff noise that
obscures review; the macro localises the gating to one definition and makes each site a one-token
change whose macro-off expansion is byte-identical to the original statement. All observation remains
inside the single central function — the "single hook surface" intent of the scope is preserved.

## 4. AMENDMENT A3 — D-SUM-04 pin-list fields: OPTION A (minimal)

Your §3.4 Option A is accepted: DIAG.2b prints the two balances (`pin_balance`,
`lookup_ref_balance`) + the `total_pin_count()` teardown cross-check, and does NOT claim a separate
pin-list balance. The gate comment's `pin_list_records/discharges/balance` fields are deliberately
NOT implemented in DIAG.2b — pin-list ownership and slot pin state are related-but-distinct concepts
and the AS4 double-count hazard you identified is exactly why the narrow set is right. Record this as
a conscious narrowing in the patch notes (one line), so the gate-comment/field mismatch is documented
rather than silent.

## 5. AMENDMENT A4 — `stores_total` boundary: ACCEPTED as proposed

Count at the common `store_owned_frame_and_prune_impl()` once the outcome is known (§5.4 / §7.3).
Public-wrapper invalid-argument early returns are not counted; DIAG.2b is not broadened for them.
Your hook placement (after store — and after prune if the combined summary is complete and still
under the same lock) is consistent with DIAG.2a's observe pattern and approved.

## 6. AMENDMENT A5 — `ownership_errors` field: KEEP, with a precise definition

Your proposed struct adds `ownership_errors` (not in the v2 scope). Keep it, defined narrowly:

```text
ownership_errors ++ ONLY in the public lookup wrapper's adoption-failure branch (the
"should be unreachable" rebalance path that also increments lookup_refs_released_by_cache_core).
Interpretation line: "ownership_errors should be 0; non-zero means an adoption failure occurred
(rebalanced safely, but investigate)."
```

It is the cheap tripwire on the branch that should never fire. Any other meaning is out of scope.

## 7. Everything else in §7 (patch shape): APPROVED AS WRITTEN

- Structs/fields per §7.1 (with A5's definition), mutable members alongside prune_diag_stats_ per §7.2.
- Hook sites per §7.3 exactly (D-SUM-04: the five named sites; D-SUM-05: per A2; D-SUM-08: per A4).
- Writers per §7.4 (existing D-SUM-10/11 style, [DSUM-SUMMARY], stderr-only, flush at writer end,
  no formatting under cache locks — snapshot accessors are lock-owning, writers take by-value copies).
- Free-filter writer order per §7.5 (01, 04, 05, 08, 10, 11).
- Selftest synthetic reference emitters per §7.6 (compute-gated bodies, print-gated writes).
- No build_config.h change (gates verified present at your cited line ranges).

## 8. Proof gate (unchanged from scope + your §8, restated as the acceptance record)

```text
1. Four-way, default config, all three compute gates ON:
     Debug 56/56 exit 0 | Release 56/56 exit 0 | forced-fail 55/56 exit 1 | verbose 56/56 exit 0
   with D-SUM-04/05/08 [DSUM-SUMMARY] blocks emitting populated fields.
2. R-PROCESS-19 matrix (five configurations): all ON; 04 OFF/05+08 ON; 05 OFF/04+08 ON;
   08 OFF/04+05 ON; all three OFF. Each: clean compile/link, family compiled out, selftest totals
   unchanged, .vpy on/off byte-identical apart from the intentionally absent family blocks.
   Do not commit the macro-off build_config edits.
3. S-series -r 1 (S1/S3/S7/S8) acceptance:
     pin_balance == 0 AND lookup_ref_balance == 0 at teardown on ALL FOUR scenarios
     (non-zero with no real leak = a missed path -> STOP and report, do not accept);
     D-SUM-05 violations == 0; D-SUM-08 fields consistent with observed store/duplicate/AS2 activity.
4. Housekeeping at commit: stage only source files (+ .vcxproj/.filters ONLY if a new TU membership
   is genuinely required — not expected for DIAG.2b since cnr3_cache_diagnostics.cpp is already a
   compiled member); no .patch/.md/log artifacts staged.
```

## 9. Note for the record (coordinator/reconcile)

Amendments A1-A5 are scope deltas to DIAG.2b v2 and travel with this response; fold them into the
DIAG.2b commit message and the next doc reconcile (DELTA/provenance) so the store_failures semantics
and the D-SUM-05 macro sanction are permanently recorded. FI-11/12/13 boundaries were correctly
maintained throughout the report — no scope creep detected.

**Generate the DIAG.2b patch.** Deliver as a single .patch against the current baseline, with your
usual apply-check command block, and the designer will review the DIFF before any build/commit —
hardest on: (a) the five-configuration compile-out (R-PROCESS-19), (b) the D-SUM-05 macro's
macro-off expansion being byte-identical to the original returns, (c) observe-only-ness at every
hook (no return value, status, or control flow altered), and (d) the store_failures duplicate
exclusion per A1.
