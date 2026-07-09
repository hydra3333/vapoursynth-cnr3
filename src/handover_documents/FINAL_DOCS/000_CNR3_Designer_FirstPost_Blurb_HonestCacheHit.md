# CNR3 — DESIGNER FIRST-POST BLURB (analysis track; honest-cache-hit-metrics IN FLIGHT)

*Paste the block below as the first message to a new DESIGNER/REVIEWER (W3D) chat, ahead of attachments.
Maintainer notes at the end (not pasted).*

---

Hello. This chat is continuing from a prior "designer" chat which hit a hard limit. We had been, and will
continue here from where that chat left off, developing a VapourSynth DLL plugin (`vapoursynth-cnr3`)
according to specifications, where the choices in each step get validated against the intent of the
specs and sensibility.

I have Visual Studio 2026 with latest updates (we call vs2026) installed on my PC, with a local git
repository (dev branch `dev_cache_manager`) connected to the GitHub repository having the same dev
branch — https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager — where local VS2026
commits are pushed at the end of every agreed successful phase/subphase (sometimes called steps).

This chat takes the DESIGNER/REVIEWER (W3D) role for the CNR3 project — a VapourSynth DLL plugin
(vapoursynth-cnr3), VS2026, x64 /arch:AVX2, git dev branch dev_cache_manager pushed to
https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager at each agreed step.

You are responsible for the CMS specification and the associated rules in it. 
In general - the coder is a separate entity responsible for responding to scope documents which
you develp and put to it and assessing and generating patch proposal which you assess and
if appropriate approve or provide advice/correction/revised-scopes to the coder.

Three-party discipline: **W3X** = coordinator (me — relays artifacts between chats, runs builds/tests/
commits, holds decisions), **W3D** = you (designer/reviewer — investigate source, write patch scopes,
review coder confirm-reports and DIFFS, issue amendments/decisions, gate commits, and own the .vpy/.bat
test harnesses), **W3C** = coder (a separate memoryless chat — investigates, confirms before patching,
generates patches). You never write production patches; you scope and you review. The coder never
self-approves; you are the diff authority. I relay everything; upload-attached files sometimes arrive
empty in-context — they are still on disk at /mnt/user-data/uploads/ and must be read from there
(standing fallback).

Read all attachments in order; do not act until I prompt you.

Read CNR3_Designer_Reviewer_Role_Handover_v1_17.md first (the role + method + PART-10 playbook), then
CNR3_Handover_Introduction_to_new_reviewer_chat_v3_11.md (its v3.11 currency note is the precise current
state).

Attachments (latest versions), in read order:
  1. CNR3_Designer_Reviewer_Role_Handover_v1_17.md               (the role + method, in depth — read first)
  2. CNR3_Handover_Introduction_to_new_reviewer_chat_v3_11.md    (reviewer orientation; v3.11 note = current state)
  3. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_14.md (standing rules incl. R-PROCESS-26 canonical 4-way)
  4. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_20.md(work plan / current state; top block authoritative)
  5. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_30.md         (detailed current-state delta)
  6. cnr3_cache_manager_design_v7_15.md                          (CMS design authority; UNCHANGED by all diag work)
  7. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v1_8.md(decisions ledger; honesty-filter precedents + A1 seed set)
  8. CNR3_Patch_Scope_HonestCacheHitMetrics_v3.md                (the IN-FLIGHT patch scope — read closely)
  9. CMS07-DIAG_honest-cache-hit-metrics.patch                   (the delivered/approved diff for that patch)
 10. CNR3_CMS_Future_Investigations_and_Open_Questions_v7_18.md  (FI ledger incl. FI-11 offline->A1, FI-14/15)
 11. cnr3_diagnostics_specification_v1_5.md                      (D-SUM programme spec)
 12. test_000_Example_576p50_TESTING_001.vpy / .bat             (the L-series production-case harness — YOUR deliverable)
 13. src.zip                                                     (committed source baseline at CMS07-DIAG.derived-health-ratios)

## WHERE WE ARE (precise)
The IN-PLUGIN DIAGNOSTICS ARC IS CLOSED (DIAG.1..3c.2 + DIAG.4; all 14 D-SUM families + [DSUM-PLANTRACE]
+ [DSUM-HEALTH] committed; latest marker CMS07-DIAG.derived-health-ratios; 56/56; NORMAL baseline; CMS
UNCHANGED at 07.15). Two ANALYSIS-TRACK diagnostics also committed since (prune-rechurn recency-gate;
derived-health-ratios).

IN FLIGHT: **CMS07-DIAG.honest-cache-hit-metrics.** L-series production-case testing (whole-clip linear,
the 99% vspipe->ffmpeg use case) exposed health row #1 (cache_hit_and_supplied_percent) as MISLEADING —
it read 0.000 on a perfectly healthy linear run because it counted only the requested-frame-precached
branch (cache_hit_return), not the predecessor-served-from-cache branch (pred_present) the linear case
uses; and no total-cache-queries counter existed, so a classical hit-rate was uncomputable. Coordinator
rulings: "anything named cache hit to a human should be a cache hit"; "a query is a query and a hit is a
hit regardless of kind." The fix (scope v3): three per-frame health rows (pred_returned_from_cache_percent
/ current_frame_returned_from_cache_percent / cache_hit_percent), two new D-SUM-04 counters
(cache_lookup_queries_total + cache_lookup_hits at BOTH lookup entry points) + a derived cache_lookup_misses
row + a cache_lookup_hit_rate_percent health row. The dying prior coder chat delivered a patch;
designer-review APPROVED bar the one derived misses row; a NEW ChatGPT coder chat is producing the
REPLACEMENT patch (approved patch + that one row). You will diff-review the replacement.

## NEXT STEPS IN ORDER
1. **Diff-review the replacement patch** vs the approved patch: confirm EXACTLY one added row
   (cache_lookup_misses, derived, guarded), the D-SUM-04 gate matched, nothing else moved. The reviewed-
   correct parts (three per-frame rows; the two counters at both entry points with query++ before the find
   / hit++ first statement of the found path; hit-rate row; edit_version bump) must be unchanged.
2. **Proof gate** — coordinator builds + canonical 4-way (R-PROCESS-26; expect 56/56 UNCHANGED). Then the
   DESIGNER harness proofs: re-run L1/L2 vs the ORACLE (L1 linear: pred 99.986 / current 0.000 / cache_hit
   99.986, hit-rate 100.000, misses 0, sum-to-100 with recovery 0.000 + frame0 0.014; L2 shuffle8: 12.143 /
   66.346 / 78.489, recovery 21.511, misses > 0, hit-rate < 100 — divergence from cache_hit_percent is
   EXPECTED, different denominators). Invariants: hits+misses==queries; branch-sum-to-100. If green ->
   commit CMS07-DIAG.honest-cache-hit-metrics -> doc currency touch (DELTA v4.31 etc.).
3. **L1noR / L2noR** — re-run L1 and L2 with -r 1 REMOVED (rrr= empty). THE key production question: does the
   clean linear case stay clean under real fmParallel-style threading, or do recovery/rechurn/recalc appear?
   That gap is A1 question #1.
4. **B-series** boundary/stress as needed (B2 exact/floor straddle, B3 backward revisit, B4 wide shuffle,
   B5 hot-zone cap overflow) to round out the counter behaviour picture.
5. **A1 scope / question-set** — the plan-trace analysis tool (external Python; no plugin gate/proof
   discipline). A FRESH designer chat is recommended for A1 (this is a natural seam). Seed question-set
   banked in Provenance v1.8; A1's input set ALSO now includes the 3 rejected health ratios, the per-context
   lookup decomposition, and the cache_hit_and_supplied lesson.
6. **A2** (fmParallel concurrency churn = the C1-ownership-under-race acceptance gate owed from 3b) then
   **A3** (real-footage 576p50 via A1).
STANDING (do not decay): 3c.2 bail-site->helper state-plumbing confirm at the first A-series REAL induced
failure; FI-11 in-run counter deferred-but-expected; FI-14 (TINY log-string honesty nits) + FI-15 (rechurn
observer split) banked, unscheduled.

## THE CODER RELATIONSHIP (what works — keep doing this)
- **Confirm-before-patch, always.** Scope = proposal; the coder's confirm-report = reconciliation with real
  source; patch only after they agree. That pass routinely catches what a scope missed.
- **Verify the coder's claims COLD against source** (file:line); re-derive, don't accept summaries. The
  designer's own scopes get corrected by this — that is it working.
- **Verify INVOCATION, not just definition** (the D-1 lesson): a defined observer with zero call sites
  compiles and silently breaks a balance. Trace the call graph.
- **Whole-patch deletion scan** on every diff: enumerate every removed line; anything beyond the sanctioned
  set is a finding.
- **R-PROCESS-25:** any touch of a proven line must be PROPOSED first — "behaviourally identical" is the
  DESIGNER'S call.
- **Objective backstops beat argument:** macro-off byte-identical (R-PROCESS-19) and hand-checks against raw
  counters have validated every "provably correct" claim this arc. A claim not so proven is not proven.
- **CODER-CHAT CAUTION (current):** the prior coder chat degraded near its limit (twice produced mangled run
  instructions). Evaluate coder responses against known-good baselines; the canonical 4-way is R-PROCESS-26.
- Scrutiny level: NORMAL-HARD, rising to exceptional for any patch delivered at/after a coder-session limit
  (the honest-cache-hit patch was reviewed WITH EXTRA CARE for exactly this reason).

The full scope and context may not be totally clear until you have read all of the documents.
Note: this is a MID-TASK resume — we are partway through a specific change.

Read all documents in order; do not comment until I prompt you.

After reading, confirm your understanding of: the current committed state (arc closed) and the in-flight
honest-cache-hit patch + what is owed (the one misses row); the L1/L2 oracle and the sum-to-100 /
hits+misses==queries invariants; the forward order (L*noR -> B-series -> A1 -> A2 -> A3); and the
R-PROCESS-24/25/26 rules. Then I will hand you the next artifact (the replacement patch to diff, or an
L-series log).

---

## Maintainer notes (NOT pasted)
VERSION POINTERS (2026-07-09): role handover v1.17 | reviewer intro v3.11 | Doc A v3.14 | Doc B v3.20 |
DELTA v4.30 | design v7.15 | condensed plan v1.10 | diag spec v1.5 | memory spec v3.4 | FI v7.18 |
provenance v1.8 | honest-cache-hit scope v3 + delivered patch. src.zip = committed baseline at
CMS07-DIAG.derived-health-ratios (refresh at each commit).
At the honest-cache-hit commit: refresh src.zip, advance DELTA/Doc B/Provenance to "honest-cache-hit
committed", and flip this blurb's IN-FLIGHT section to the next task (L*noR runs / A1 scope).
For a new CODER chat instead, use 000_CNR3_Coder_FirstPost_Blurb_HonestCacheHit_PendingPatch_v2.md.
