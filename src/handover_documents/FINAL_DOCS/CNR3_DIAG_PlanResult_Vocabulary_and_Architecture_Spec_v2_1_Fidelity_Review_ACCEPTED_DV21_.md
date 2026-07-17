# CNR3 — FIDELITY REVIEW: Plan-Trace SPEC v2 (by the prior designer chat, per the agreed assisted cycle)

**Reviewed against:** spec v1 + the coder cross-check report + the Ring & PlanTrace Rationale doc (Part B),
plus independent verification of v2's source claims against the fresh post-DIAG.3b committed src.

## VERDICT: ACCEPTED — one numeric correction (D-V2-1). No fidelity loss found. Feedback, not rewrite.

The v2 is a faithful full revision: all SIX findings resolved and provenance-recorded (§13), all EIGHT
locked decisions carried with their reasoning and rejected alternatives bound in place, nothing silently
re-opened, and the 3c.1/3c.2 boundary delineated without deciding the split. The inline-metadata mechanism
([RESOLVED]/[LOCKED]/[coordinator-confirmed] tags with WHY + rejected-alternative attached to each decision)
works as intended — the reasoning is now inseparable from the decisions.

## D-V2-1 (the one correction): the bail-site total is 65, not 66

```text
v2 states (Set 5, §7, §12, §13c): arInitial 14 / arAllFramesReady 51 / top-level 1 = 66.
VERIFIED against the post-3b committed source:
  - arInitial raw grep:  14  (all call sites)                                        -> 14 CORRECT
  - AR raw grep:         51  — BUT this INCLUDES the function DEFINITION at
    cnr3_arAllFramesReady.cpp:526 (`void cnr3_set_filter_error(`); the definition
    lives in AR itself (plugin_internal.h:111 is only the declaration).
    AR CALL SITES = 50.                                                              -> 51 is RAW, not sites
  - vapoursynth-Cnr3.cpp: 1                                                          -> 1 CORRECT
  TOTAL BAIL SITES = 14 + 50 + 1 = 65.
PROVENANCE of the off-by-one: the cross-check report's own phrasing ("51 sites plus the helper function
definition") already conflated raw-count with site-count; v2 propagated it in good faith and its
"confirmed against committed source" is TRUE for raw grep counts — the definition-exclusion is the step
that was missed. Not a fidelity failure; a propagated imprecision, now corrected at the review layer
exactly as the assisted cycle intended.
FIX: change 66 -> 65 and "AR 51" -> "AR 50 call sites (raw grep 51 includes the definition at AR:526)"
in the four places (Set 5 count block, §7, §12 3c.2 scope line, §13 finding (c)). Note the spec ALREADY
says the counts are a snapshot and the authoritative table is built at 3c.2 scope time from live source —
that discipline stands and would have caught this then; correcting now keeps the record honest earlier.
```

## Verification evidence (what was independently checked and held)

```text
FINDING (a) sources branch-specific — VERIFIED IN SOURCE: every mutation of source_request_frame_numbers
  (clear at arInitial:363; reserve/push at 375-392) is inside the recovery fill; the remaining references
  (594/597) are READS in the request loop; the three non-recovery branches never touch the vector. The
  v2 derivation (sources=[n] for cache_hit/frame0/pred_present; =the vector for recovery) is exact.
FINDING (6) pinned branch-derived — pin-list privacy claim consistent with the cross-check's source
  citations; the [coordinator-confirmed] no-new-accessor ruling is recorded with the R-PROCESS-25 escape
  hatch. Correctly named as the sixth finding with the 5-vs-6 provenance in §0 and §13.
FINDINGS (b)(d)(e) — table-not-parser with rationale; from/to recorded DECIDED compile-time with the
  runtime option deferred; dump-on-bail isolated as 3c.2 with the table as foundation artifact. All as
  the cross-check + handoff note required.
LOCKED DECISIONS — all eight present with reasoning: blocks-not-streaming (with the explicit "do not
  reintroduce as an improvement" guard); per-item facts only with BOTH pin-count rejections and the
  coordinator quote, plus the where-balances-DO-live cross-reference (an improvement over v1); once-
  guarded dump-on-end-OR-bail with the uniform-bail trace retained as the feasibility argument; flush-
  always tied to R-PROCESS-24/Doc A v3.13; windowed buffer no-ring with the contrast to the D-SUM-10/13
  saturation discipline; steady-ticks + UTC-at-dump; three legend-headed views with the numbers-inside-
  the-#if rule; human-lists + machine-codes.
BOUNDARY (§12) — 3c.1/3c.2 delineated precisely (3c.1: Sets 1-3 + C/K/L/U/N, clean-end dump only, no
  bail touch; 3c.2: bail trigger + E/X + reasons + the table + per-site additive writes), dependency
  stated, the coordinator's capture-fails-from-day-one direction preserved as scoping INPUT, split NOT
  decided. Exactly the brief.
SRC BASELINE — the supplied src.zip verified post-DIAG.3b (the 06/07/09/14 machinery present).
```

## Endorsement (for the coordinator to relay)

With D-V2-1 applied (a four-place numeric edit producing v2.1 or an amended v2 — the ND's choice of
version label, provided the change note records the correction), this spec is the CONTROLLING INPUT to
the DIAG.3c scope. The assisted-cycle objective is met: the revision is intent-faithful, the reasoning
travels with the decisions, and the one defect found was a propagated count, not a design loss. The ND
is equipped to proceed to the 3c scope independently; recommended next artifacts in order: (1) v2 count
correction; (2) the DIAG.3c scope (deciding 3c.1/3c.2 packaging, per §12/§14); (3) at 3c.2 scope time,
the site-to-category TABLE built from live source — where the 65 will be re-derived authoritatively.
