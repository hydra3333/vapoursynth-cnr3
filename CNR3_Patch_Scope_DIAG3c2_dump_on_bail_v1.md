# CNR3 — PATCH SCOPE: DIAG.3c.2 — PLAN-TRACE DUMP-ON-BAIL + FAILURE DETAIL (invasive; R-PROCESS-21/25)

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Controlling input:** `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v2_3.md` (§4 Set 4 X/E, §4 Set 5,
§7, §8 dump, §12 the 3c.1/3c.2 boundary). Where this scope and the spec disagree, the spec wins and you flag it.
**Status:** PROPOSAL for coder investigate/confirm. Confirm-before-patch, always. This scope names several
mechanism CHOICES it deliberately does NOT settle — your confirm report decides them against the real source.
**Scrutiny:** EXCEPTIONAL. This is the ONLY diagnostic in the arc that writes at the 65 proven `cnr3_set_filter_
error` bail sites — control-flow-adjacent to the getFrame error paths. R-PROCESS-21 (proven code) +
R-PROCESS-25 (propose each proven-line touch before patching) both apply in full.

## 0. Apply-on-top; own commit
DIAG.3c.1 (observe-only capture + single-line emission) is COMMITTED. This is the NEXT cycle, applied on top,
with its OWN scope / confirm / patch / diff-review / proof-gate / commit. It builds on 3c.1's buffer, record
format, BEGIN/END block, and clean-end dump — and must NOT regress any of them.

## 1. What 3c.2 delivers
Failure visibility for the plan-trace. Three additions, all on the FAILURE paths only:
1. **`fail_reason`** on a FAILED record — one of the 16 Set 5 categories (§2), assigned per bail SITE.
2. **Set 4 `E` (error_here)** on the item the bail occurred on, and **`X` (not_reached)** on planned items
   the bail skipped (plan remainder). These are DERIVED codes local to the plan-trace record (do NOT expand
   the production `Cnr3LiveRecoveryHoleOutcome` enum).
3. **Dump-on-bail**: the plan-trace block emits at the moment of failure (not only at clean end), so a run
   that dies mid-flight still yields its trace-so-far with the failure context.

## 2. The 16 failure-reason categories (Set 5, spec v2.3 §4 — ASCII)
```text
 1 COPYFRAME_FAILED                    9 INVALID_LIFECYCLE
 2 COPYFRAME_SOURCE_ALIAS             10 INVALID_BRANCH_FOUNDATION
 3 SOURCE_RETRIEVAL_FAILED            11 SCENE_PROCESSING_FAILED
 4 SOURCE_NOT_REQUESTED               12 BYTE_ESTIMATE_FAILED
 5 ACQUIRE_REF_FAILED                 13 FRAMEDATA_MISSING_OR_UNKNOWN
 6 ADOPT_FAILED                       14 ALLOCATION_FAILED
 7 STORE_PRUNE_FAILED                 15 RECOVERY_PLAN_FAILED_OR_REFUSED
 8 DISCHARGE_FAILED                   16 HOT_ZONE_OBSERVATION_FAILED
```

## 3. FIRST CONFIRM DELIVERABLE — the SITE-TO-CATEGORY TABLE (the foundation; nothing else is correct without it)
Inventory ALL 65 `cnr3_set_filter_error` CALL sites cold against the CURRENT post-3c.1 source and produce a
table: one row per site -> file:line -> assigned Set 5 category -> the containing function/branch (for context).
```text
Expected inventory (re-derive against live source; line numbers have shifted post-3c.1):
  cnr3_arInitial.cpp        14 call sites
  cnr3_arAllFramesReady.cpp 50 call sites   (raw grep 51 includes the DEFINITION at ~AR:526 -- NOT a site)
  vapoursynth-Cnr3.cpp       1 call site    (top-level getFrame state)
  total                     65 call sites
```
- Assign each site by its SOURCE LOCATION, NEVER by parsing the runtime message string (finding (b); a compile-
  time site->category map is exact and survives message edits; `frame_code` already carries the branch/phase).
- Flag any site that does NOT fit one of the 16 cleanly, with your proposed resolution (new category vs closest
  fit). Report the count per category. This table is reviewed BEFORE any code is written.

## 4. FAILED-record semantics (confirm what is reconstructable at each site)
A bailed frame yields a FAILED record instead of a normal R (spec §4 Set 3 `FAILED`):
```text
  outcome     = FAILED
  fail_reason = <Set 5 category from the table for this site>
  E (error_here) = the item the bail occurred ON (the current frame/hole being processed at the bail)
  X (not_reached) = planned items from the O record that were never reached = plan_holes MINUS
                    (items already computed/adopted before the bail) MINUS the E item
  computed / adopted_skipped / post_compute_discarded = whatever completed BEFORE the bail (progress-so-far)
```
Two bail loci, different available state — CONFIRM per site:
- **arAllFramesReady bail** (50 sites): the O record exists (arInitial succeeded); reconstruct E / X / progress
  from the plan (request_data, if still alive at the site) + the processed accumulator. CONFIRM request_data is
  alive at each AR bail site (i.e. the bail precedes the normal discard) and what progress state is readable.
- **arInitial bail** (14 sites) + **top-level** (1): the plan may be incomplete / not yet published (no O
  record). These produce a MINIMAL FAILED record (fail_reason + whatever frame identity is known, empty plan
  lists). CONFIRM what is knowable at each.

## 5. Dump-on-bail mechanism (spec §8 dump; once-guarded)
Spec intent: exactly ONE block per run, emitted at whichever comes FIRST — clean end (3c.1, already built) OR
bail. The clean-end arm has a once-guard; 3c.2 adds the BAIL arm under the same guard so end + bail cannot
double-emit. TWO mechanism questions this scope does NOT settle — decide in your report:
- **(M1) Where the failure is recorded + dumped.** Options: (a) each bail site writes its FAILED record
  (fail_reason + E) into the buffer additively, then a shared bail-dump fires; OR (b) since `cnr3_set_filter_
  error` is a SINGLE COMMON helper (AR:~526, decl plugin_internal.h:~111), pass the Set 5 category as a new
  parameter to it and centralize the FAILED-record-write + dump in ONE place (65 sites then only pass a
  category enum). (b) is far less invasive per-site but changes a widely-called signature and couples the bail
  helper to the plan-trace. Recommend one, with the trade-off. Either way the per-site touch is ADDITIVE.
- **(M2) Whether a bail-arm dump is even needed, or a record-write + the existing clean-end dump suffices.**
  CONFIRM whether VS calls `cnr3_free_filter` (the 3c.1 clean-end dump site) after a `setFilterError` bail. If
  it reliably does, the bail path may only need to WRITE the FAILED record (flushed, R-PROCESS-24) and let the
  existing clean-end dump emit it. If it does NOT reliably run on error, the bail path must dump itself
  (once-guarded). The spec's rationale (end-of-run-only would LOSE the failure) assumes the latter — verify.

## 6. FENCE — do NOT touch / must not regress
```text
- The 3c.1 observe-only CAPTURE: the arInitial RAII O-guard, the four arAllFramesReady branch-local R
  captures, enter_tick sampling, the buffer/field, the tick/seq invariant, the single-line emission, BEGIN/END.
  3c.2 ADDS failure handling; it must not alter any success-path behaviour or the observe-only proof.
- No expansion of the production Cnr3LiveRecoveryHoleOutcome enum (X/E stay local to the record).
- No cache-core, no project-file changes. No pin-list accessor.
- At each bail site the edit is ADDITIVE ONLY: record/pass the reason, then the EXISTING setFilterError +
  return nullptr. Do NOT restructure, reorder, or merge any bail path. Any change beyond a pure additive write
  is R-PROCESS-25: propose it explicitly.
```

## 7. Gate structure
Reuse the 3c.1 master family gate (`CNR3_DIAG_COMPUTE/PRINT_DSUM_PLANTRACE`) — 3c.2 is the same family. With
the master gate OFF, ALL 3c.2 additions compile out and the 65 sites revert to EXACTLY the original
`setFilterError + return nullptr` (this is the R-PROCESS-19 exit gate for the invasive part). CONSIDER (propose)
a nested sub-gate for the bail-site writes only (e.g. `..._BAIL`) if a 3c.1-behaviour build is wanted with the
capture on but bail-writes off; otherwise one master gate. Two-gate #error discipline unchanged; DSUM01-14
untouched.

## 8. R-PROCESS-21/25 discipline (this cycle, in force)
- Propose the EXACT per-site edits (or the single helper-signature change + the 65 category args) for review
  BEFORE patching — the site-to-category table (§3) is that proposal's core.
- Additive-only; whole-patch deletion scan at diff review; verify INVOCATION (the D-1 reflex: a FAILED-record
  writer defined but not wired at some sites would silently drop those failures -> trace every site's write).
- Flush-always (R-PROCESS-24): the FAILED record and the bail dump flush per line — the bytes must reach the
  wire before `return nullptr` on a dying run.

## 9. Proof gate (3c.2)
```text
1. FAILURE-DUMP proof (the point of 3c.2): induce a representative bail (propose HOW -- e.g. a forced-fail
   hook / a scenario that trips a known site) and confirm the plan-trace block STILL emits, with the FAILED
   record carrying fail_reason (correct category for that site) + E on the failing item + X on the plan
   remainder, FLUSHED, before teardown. Prove at least one arInitial bail and one arAllFramesReady bail.
2. FLUSH proof: on the induced bail, no lost tail (R-PROCESS-24).
3. R-PROCESS-19 macro-off: master gate OFF => the 65 sites are byte-identical to the pre-3c.2 originals; the
   whole family compiles out; four-way identical; .vpy byte-identical on/off (the clean-run A/B still PASSES).
4. Four-way selftest all-on / macro-off / restored (fixture updated to exercise a FAILED record).
5. R-PROCESS-21/25: each site edit additive; proven bail paths otherwise unchanged; cache-core + recovery
   selftests unchanged; whole-patch deletion scan clean.
6. Clean-run S-series (S1/S7/S8) unchanged from 3c.1 (no FAILED records on clean runs; no fail_reason/X/E).
```

## 10. Expected file list (confirm exact set)
```text
cnr3_arInitial.cpp           14 bail-site additive writes (or category args)
cnr3_arAllFramesReady.cpp    50 bail-site additive writes (or category args); + the shared FAILED-record /
                             dump helper if mechanism (M1b) chosen; + the common setFilterError signature if (M1b)
vapoursynth-Cnr3.cpp          1 bail-site write (top-level); possibly the bail-arm dump trigger
cnr3_diagnostics.{h,cpp}     FAILED-record fields (fail_reason, E/X in codes), the bail-dump/once-guard bail arm
cnr3_plugin_internal.h       setFilterError decl if (M1b); any FAILED-record plumbing
cnr3_build_config.h          (only if a bail sub-gate is added) + marker bump at commit
cnr3_cache_core_selftest_main.cpp  fixture emits a FAILED record (fail_reason + E/X)
```
Flag any file you must touch that is not listed, and any listed file you do not need.

## 11. Your confirm report should deliver
1. The SITE-TO-CATEGORY TABLE (§3) — the load-bearing artifact.
2. M1 recommendation (per-site write vs common-helper category param) with the trade-off, and M2 (does
   free_filter run after a bail -> is a bail-arm dump needed).
3. Per-locus reconstructability of E / X / progress-so-far (§4), incl. which AR sites keep request_data alive.
4. The bail sub-gate decision (§7); the induced-bail proof method (§9.1).
5. The exact file/site list; anything this scope got wrong or missed (highest-value part).
