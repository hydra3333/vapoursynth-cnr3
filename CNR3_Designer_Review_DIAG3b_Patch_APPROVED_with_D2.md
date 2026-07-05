# CNR3 — DESIGNER REVIEW: DIAG.3b patch (CMS07-DIAG.3b-lifecycle-return-scene.patch)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** the DIAG.3b patch (1967 lines, 7 files) + patch notes, 2026-07-04
**Review basis:** full diff read; pre-declared targets from the C1-C4 response §9 all checked; the D-1
"invocation-not-just-definition" lesson applied to every observer.

---

## VERDICT: APPROVED FOR THE PROOF GATE, with ONE PROCESS FINDING (D-2, retro-sanctioned below).
No behavioural defects found. All C1-C4 + C-ALIAS decisions correctly implemented.

## 1. The pre-declared targets — evidence

```text
(a) C-ALIAS at every alias block — PASS. Verified at the pred-present block in full: inside the
    (output_frame == source_frame) branch the observation is cnr3_diag_live_observe_source_release
    (DSUM06-gated, a SOURCE release), no DSUM07 call; temporary_output_created fires only AFTER the
    null+alias checks pass. The release-site pattern covers null/alias/success/failure frees across
    pred-present, floor, hole, target, frame-0 (24 observe_source_release occurrences matching the
    confirm report's inventory).
(b) C1 stored-vs-addFrameRef separation — PASS. observe_temporary_output_stored has exactly TWO live
    call sites (the AS2 floor and hole adoption paths); the production addFrameRef(output_frame) sites
    have NO stored call — the cache copy stays outside the temp-output balance, as decided.
(c) Six retrieve classes — PASS. Six production call sites for the retrieve observer (trigger,
    pred-present, floor, hole, target, frame-0), matching the confirm report's full inventory.
(d) Additive-only at return-decision sites — PASS WITH FINDING D-2 (below). No allows_return refactor
    (decisions still made by the existing explicit checks); the observation hooks are additive — EXCEPT
    one two-line condition hoist in the frame-0 path (D-2).
(e) Six-config compile-out — gating is dense and per-family (33/28/18/9 guards for 06/07/09/14); the
    real matrix is owed at the proof gate.
(f) Exactly-once at multi-exit sites — the release/store/transfer observations are placed per-branch at
    outcome-known points (the alias/null/success/failure branches each observe their own outcome once).
INVOCATION CHECK (the D-1 lesson): all 15 observers have >=4 occurrences — every one defined AND called
    at production sites. NO orphaned observer.
DELETION SCAN: exactly TWO lines removed in the entire patch — the two halves of one frame-0 if-condition
    (the D-2 hoist). Nothing else removed anywhere.
WRITERS: four writers, four flushes, [DSUM-SUMMARY], per-family snapshot accessors; free-filter emission
    adds 06/07/09/14 in numeric order.
```

## 2. FINDING D-2 — an unauthorized (but behaviourally safe) transformation of proven code

```text
WHAT: in cnr3_complete_live_frame0_fresh_start, the original

    if (!cnr3_status_is_ok(store_hard_status) ||
        !cnr3_live_store_status_allows_return(store_status)) {

was transformed to

    const bool frame0_return_allowed = cnr3_live_store_status_allows_return(store_status);
    [DSUM09-gated observation using frame0_return_allowed]
    if (!cnr3_status_is_ok(store_hard_status) || !frame0_return_allowed) {

The hoisted bool is OUTSIDE the diagnostic gate — it exists in the macro-off build, permanently
replacing the original short-circuit form.

BEHAVIOURAL ANALYSIS: identical. cnr3_live_store_status_allows_return is a PURE function of the enum
(verified: returns status==ok || status==duplicate; no side effects). The hoist changes only WHEN the
pure call evaluates (always, vs short-circuit-skipped when hard_store_status already failed — a cold
bail path). The if-condition's boolean outcome is identical in all cases. No observable difference; the
selftest/.vpy outputs cannot differ.

COMPLIANCE ANALYSIS: this is a mechanical transformation of proven code that was NOT proposed first.
R-PROCESS-21 and the DIAG.2b A2 precedent require propose -> designer sanction -> implement, and the
coder's OWN confirm report stated transformations "should be explicitly authorized". A strictly-additive
alternative existed (call the pure function redundantly INSIDE the gated block and leave the original
if verbatim), which would have made the macro-off source byte-identical.

RULING: RETRO-SANCTIONED. The designer hereby formally approves this specific hoist (it is provably
pure, small, and arguably more readable), so the record shows explicit designer sanction for every
proven-code transformation — as with A2. HOWEVER: the SEQUENCE was wrong. Standing instruction to the
coder, restated as hard: ANY modification of an existing proven line — however small, however pure —
must be PROPOSED in the confirm report (or a follow-up question) and sanctioned BEFORE it appears in a
patch. "Behaviourally identical" is the designer's determination to make, not the coder's to assume.
The additive alternative should be the default reflex. No rework is required for D-2 (a rework cycle
would add risk for zero safety gain), but the commit message must note the sanctioned transform.
```

## 3. Proof gate (unchanged from the C1-C4 response §8, restated)

```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-06/07/09/14 blocks emit.
2. R-PROCESS-19 SIX-config matrix: all-on / 06-off / 07-off / 09-off / 14-off / all-four-off — each
   clean build + four-way identical + family block absent when off. Temporary edits not committed.
3. S-series -r 1 (S1/S3/S7/S8):
   - D-SUM-06 source_frame_release_balance == 0 on all four; same_activation_request_violations == 0;
     partial_acquire_failures == 0. (S7/S8's recovery churn exercises the floor/hole/target retrieve+
     release classes hard — the six-class completeness test.)
   - D-SUM-07 temporary_output_balance == 0 on all four. S7/S8 may show duplicate_computed_but_discarded
     > 0 under churn and the balance MUST STILL close (the C1 semantics test).
   - D-SUM-09 lookup_ref_balance == 0; decision yes+no == checked; reason rows sum consistently.
   - D-SUM-14 fields coherent; promotion_mismatches == 0; tiny-profile near-grid caveat line prints.
   - Prior families (01/03/04/05/08/10/11/12/13) unchanged — all balances 0, violations 0, failures 0.
4. Any non-zero balance with no real leak = a missed site -> STOP and report; do not commit.
5. Commit (after 1-3 green): the seven source files only; commit message notes amendments C1-C4,
   C-ALIAS, and the D-2 sanctioned hoist. No build_config, cache-core, or project-file changes.
```
