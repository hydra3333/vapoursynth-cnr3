# CNR3 — DESIGNER REVIEW: DIAG.2b patch (CMS07-DIAG.2b-ownership-integrity-store.patch)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** the coder's DIAG.2b patch (1194 lines, 6 files) + patch notes, 2026-07-04
**Review basis:** full diff read, every hunk, verified against the current post-DIAG.2a source baseline.

---

## VERDICT: APPROVED FOR THE PROOF GATE. No defects found.

All four pre-declared review targets pass, all five amendments (A1-A5) are implemented as specified,
and the patch is pattern-faithful to the DIAG.1/2a conventions throughout. Two implementation
subtleties that were NOT explicitly specified were handled correctly by the coder unprompted (§3
below) — the strongest quality signal in the patch.

## 1. The four pre-declared targets — evidence

### (a) Five-configuration compile-out (R-PROCESS-19) — PASS at diff level
Every element of every family is individually gated: structs, mutable members, helper
declarations/definitions, all hook call sites, snapshot accessors (compute gates); writers,
free_filter emission calls (print gates); selftest reference emitters (compute-gated bodies,
print-gated writes). Placement is adjacent to the existing DSUM10/11 gated blocks in every file,
so the gating pattern is uniform and auditable. The coder's sandbox syntax matrix passed all five
configurations. THE REAL VS2026 MATRIX REMAINS THE EXIT GATE (see §5).

### (b) D-SUM-05 macro macro-off byte-identity — PASS
```text
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
#   define CNR3_DSUM05_FAIL(tag) return observe_cache_invariant_failure_locked((tag))
#else
#   define CNR3_DSUM05_FAIL(tag) return false
#endif
```
Macro-off, each site expands to literally `return false;` (site supplies the semicolon) — byte-
identical to the original statement. Exactly 18 sites transformed with 18 distinct stable tags;
the whole-patch deletion scan (below) proves ONLY those 18 lines were removed; the final
`return true` is untouched; `invariant_checks_performed` is counted ONCE at function entry per
A2; and the coder added `#undef CNR3_DSUM05_FAIL` immediately after the function — macro scoped
to its single user, a hygiene measure beyond the spec.

### (c) Observe-only-ness at every hook — PASS
The decisive evidence: a scan of EVERY deletion line in the entire 1194-line patch returns
exactly 18 lines, every one `return false;` (replaced value-identically by the macro). Nothing
else is removed or modified anywhere. Every hook is an additive gated insertion:
```text
D-SUM-04 (five sites, per the approved map):
  pin_frame_locked        — observe immediately after ++slot.pin_count
  unpin_frame_locked      — observe immediately after --slot.pin_count
  lookup..._locked        — observe after non-null addFrameRef, before *out write
  wrapper adopt-success   — observe_lookup_ref_transferred before return ok
  wrapper adopt-failure   — observe released_by_cache_core + ownership_error before the
                            existing freeFrame + return (A5 definition honoured)
D-SUM-05: entry hook + the 18 macro sites, all inside the one central predicate
D-SUM-08: observe at EVERY outcome-known exit of store_owned_frame_and_prune_impl, exactly-once
          per invocation via a gated local store_outcome_observed flag (see §3.2)
```
No return value, Cnr3Status, or control flow is altered at any site.

### (d) A1 duplicate exclusion — PASS, verified in code
`store_failed = (store_status != Cnr3Status::ok && store_status != Cnr3Status::duplicate)` —
exactly the amendment. `duplicates_seen` and `incoming_rejected` carry duplicate telemetry
separately (both cover the production-duplicate-via-store_status and AS2-summary cases).

## 2. Amendments A2-A5 — all implemented as specified
```text
A2: macro form, entry-counted checks, first_violation_site captured once, helper is const/
    noexcept/returns-false-unconditionally, fully compiled out when off.           IMPLEMENTED
A3: pin-list fields consciously omitted — and the narrowing is even PRINTED as a note line in
    the D-SUM-04 summary output, so the field/gate-comment mismatch self-documents. IMPLEMENTED
A4: stores_total counted at the impl once outcome known; public-wrapper invalid-arg exits (and
    the impl's own pre-lock Invalid-kind rejection) are before any hook — not counted. IMPLEMENTED
A5: ownership_errors increments ONLY on the wrapper adoption-failure rebalance path.  IMPLEMENTED
```

## 3. Two unspecified subtleties the coder handled correctly (quality signals)

### 3.1 The unlocked-context race — found and solved
D-SUM-04 sites 4/5 (wrapper adopt/fail) run AFTER cache_mutex_ is released, while sites 1-3
mutate the same stats under the lock. A naive implementation would race. The coder's observers
for the unlocked sites internally take `std::lock_guard<std::mutex>(cache_mutex_)` before
mutating — all ownership_diag_stats_ mutations are serialized under one lock, no race, no
deadlock (the wrapper has fully released before these run), and the `_locked`-suffix naming
convention correctly signals which context each helper expects. Verified against the wrapper's
actual lock scope in current source.

### 3.2 The multi-exit store impl — the D-SUM-05 lesson generalized
store_owned_frame_and_prune_impl has multiple exits after the store outcome is known (AS2 fail,
pin-not-recorded, production fail, retire fail, prune fail, success). A single end-of-function
hook would miss the early-exit outcomes — the same class of problem as D-SUM-05's early returns.
The coder observed at every outcome-known exit with an exactly-once flag, unprompted. Verified
supporting facts: all three public wrappers assign out_summary.store_kind BEFORE calling the impl
(source lines 824/862/901), so every observation sees a valid kind; and the header observe
function bounds-guards the by-kind array (index < COUNT), so the Invalid sentinel is safe.

## 4. Designer's independent adversarial checks (beyond the coder's claims)
```text
- Whole-patch deletion scan: 18 deletions, all `return false;` — additive-only proven.
- pin_count mutation hunt: the only assignments outside pin/unpin_frame_locked are the fresh-slot
  initialisation in store_owned_frame_locked (benign, new local slot) — balance completeness holds.
- Snapshot accessors: ownership snapshot samples total_pin_count_crosscheck = total_pin_count_locked()
  AT SNAPSHOT TIME UNDER LOCK (same-moment cross-check, correct); integrity snapshot fills the
  structural summary sample under lock; writers receive by-value copies and format outside locks.
- Balance arithmetic in the writer avoids unsigned underflow (signed branch on ordering).
- free_filter order is 01/04/05/08/10/11, each print-gated.
- Writers: [DSUM-SUMMARY] tag, interpretation lines per scope, cnr3_diag_flush_stderr() at each
  writer end (three writers, three flushes — verified).
- Selftest reference emitters: compute-gated bodies; the D-SUM-04 synthetic example is deliberately
  BALANCED (2/2 pins, 2 = 1+1 lookup refs), demonstrating the healthy zero-balance pattern.
```

## 5. Proof gate (coordinator actions — the patch is approved to proceed to this)

```text
1. Apply per the coder's notes (git apply --check both forms, then apply, git diff --check).
2. Build Debug + Release (both projects), default config (all three compute gates ON as shipped).
3. Four-way: Debug 56/56 exit 0 | Release 56/56 exit 0 | forced-fail 55/56 exit 1 | verbose 56/56
   exit 0 — with D-SUM-04/05/08 [DSUM-SUMMARY] blocks emitting populated fields.
4. R-PROCESS-19 matrix (five configs, temporary build_config edits, DO NOT COMMIT them):
   all ON | 04 off | 05 off | 08 off | all three off — each: clean build, four-way identical,
   family blocks absent when its gate is off.
5. S-SERIES ACCEPTANCE (the D-SUM-04 completeness criterion): run S1/S3/S7/S8 under -r 1 and
   confirm on ALL FOUR scenarios:
      pin_balance == 0 AND lookup_ref_balance == 0 (at teardown)
      D-SUM-05 invariant_violations_detected == 0
      D-SUM-08 fields consistent with the run (S7/S8 show stores by kind, duplicates/AS2 activity)
   A non-zero balance with no real leak = a missed acquire/release/transfer path -> STOP, report,
   do not commit. (This is the backstop for the single-caller/completeness claims.)
6. Commit: stage ONLY the six source files (no .vcxproj change expected — cnr3_cache_diagnostics.cpp
   is already a compiled member since DIAG.2a; verify git status shows no project-file delta).
   No .patch/.md/log artifacts staged. Fold amendments A1-A5 into the commit message per the
   designer response §9.
```

## 6. Minor observations for the record (none blocking)
```text
- Field name total_pin_count_crosscheck (vs the report's total_pin_count_at_summary): the
  implemented name is better — accepted.
- Retire-fail/prune-fail exits record store_status (typically ok) — correct semantics: the STORE
  succeeded; those downstream failures are not store_failures and are not silently lost (they
  surface as the impl's returned status).
- The D-SUM-05 entry hook adds an O(slots) total_pin_count walk per invariant check — present only
  when the gate is on; compiles out in production (R-PROCESS-19). Acceptable diagnostic cost.
- stores_total counts impl-level outcome exits including failure outcomes — consistent with A4's
  "once the outcome is known" wording; the per-kind and failure counters disambiguate.
```

## 7. Coder performance note (for the coordinator)
Second consecutive strong deliverable from the new coder session: amendment-complete, pattern-
faithful, additive-only proven by the deletion scan, and two unspecified concurrency/completeness
subtleties handled correctly without prompting. The trust established by the confirm report is
reinforced by the patch. Exceptional scrutiny can relax to normal-hard for the next cycle.
