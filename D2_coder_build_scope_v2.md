CNR3 — D.2 branch-(d) exact-anchor MULTI-hole recovery + bounded-window refusal. BUILD SCOPE v2 (coder review incorporated).
v2 folds in the coder's two required corrections: (1) search interval is [max(0,N-B), N-1] (N is the
target, never an anchor candidate); (2) the refusal path emits no-in-window-anchor only -- the code does
NOT distinguish out-of-window-anchor from no-prior-anchor (that would need an unbounded search; the
harness proves the bounded-window case by construction). No cache-core change for a reason string.

GOVERNANCE: this scope is written under the CNR3 Design Alignment and Escalation Charter (Production
Spec §3A.5.0 / Role Handover §D0). If any step below conflicts with a CMS rule (RULE-DEVIATION) or
reveals a gap the CMS does not cover (CMS-GAP), STOP and raise it rather than coding around it.

PURPOSE
Generalise the proven D.1 single-hole recovery to k>=2 holes, and add the bounded-window refusal. D.2
is mostly an ACCEPT-GATE WIDENING plus a refusal proof: the multi-hole fill loop and the multi-pin
discharge machinery ALREADY EXIST and are exercised by D.1 (D.1 already discharges two pins: anchor +
one filled hole). D.2 makes the larger pin-list and the multi-hole fill the load-bearing proof, and
proves the "nearest anchor beyond the back-radius -> clean refuse" path.

This scope is FOR REVIEW. Nothing is approved to build until the plan, golden values, the accept-gate
widening, the refusal boundary, and the interleaving analysis are agreed.

=========================================================================================
WHAT D.2 CHANGES (small, by design — built on the proven D.1 path)
=========================================================================================
1. ACCEPT-GATE WIDENING (the core change). D.1's gate (cnr3_d1_recovery_plan_is_accepted) hardcodes:
       (hole_count==1 && anchor==N-2) || (hole_count==0 && anchor==N-1)
   D.2 widens the accepted exact-anchor family to k>=1 contiguous holes with the matching anchor:
       accept iff anchor_found && anchor_pin_recorded
               && hole_count == K  (K>=0)
               && anchor_frame_number == N-(K+1)   [exact-anchor: holes are exactly N-K .. N-1]
               && holes are the contiguous set { N-K, ..., N-1 } (verify, do not assume)
   i.e. D.1's two clauses become the K==1 and K==0 instances of one rule. D.2's PROOF case is K==2
   (anchor==N-3, holes {N-2,N-1}); K==0 (zero-hole degenerate) and K==1 (D.1) remain accepted and MUST
   still pass (regression). NON-exact / non-contiguous plans still clean-refuse (see refusal below).
   The gate rename should reflect generality (e.g. cnr3_exact_anchor_recovery_plan_is_accepted).

2. BOUNDED-WINDOW REFUSAL (the new proof). The H.2A planner searches the bounded interval
   [max(0, N-B), N-1], B = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS = 50 (compile-time constant;
   checkpoint-agnostic — a present output beyond N-B is NOT found, proven by the H.1A selftest). The
   requested frame N is the TARGET and is never an anchor candidate (the descending search looks below N).
   The planner returns anchor_found==false whenever there is NO present output in the window
   [max(0,N-B), N-1]. CRITICAL (coder correction): the code CANNOT cheaply tell apart "an older anchor
   exists beyond the window" from "no prior output exists at all" -- both look identical to a bounded
   search (anchor_found==false), and distinguishing them would require an UNBOUNDED out-of-window
   search, which is exactly what the bounded window exists to avoid. Therefore:
     - The code emits a SINGLE honest refusal reason: no-in-window-anchor (a.k.a.
       bounded-recovery-window-no-anchor). It reports only what the bounded search actually observed.
     - The HARNESS proves the bounded-window-exceeded case BY CONSTRUCTION (Run C establishes output[0],
       then requests output[52]; we know from setup the nearest anchor is 52 back, > B=50).
     - The floor-fresh-start case (genuinely no prior output) is D.3, also refused here; it is NOT
       separately labelled by the code in D.2 (same anchor_found==false signal).
   Do NOT add a cache-core helper or an out-of-window search merely to enrich the reason string. (If an
   EXISTING primitive already distinguishes out-of-window-anchor from no-prior-anchor for free, the
   richer label may be emitted -- but nothing is to be ADDED for it. Per the charter, wanting the code
   to distinguish these with no supporting primitive is a CMS-GAP/scope issue to raise, not code around.)
   The refusal is clean (frameData discharged + deleted, clear setFilterError), no partial state.

3. NOTHING ELSE STRUCTURAL. The completion (cnr3_complete_live_recovery) ALREADY loops over
   recovery_plan.hole_frame_numbers (ascending), already does pre-compute adopt-and-skip per hole,
   already stores each hole via the AS2 hole-store recording a pin, already computes the target from
   the last filled hole, already does the authoritative target return, and already AS4-batch-discharges
   the whole pin_list on every exit path. With K==2 this loop runs twice and the pin_list holds three
   pins (anchor + 2 holes). NO new loop, NO new discharge path. Verify the existing loop handles K==2
   with no off-by-one and the discharge count matches pin_list size.

=========================================================================================
GOLDEN CHAIN (computed + verified against the real response tables + P.11B; the derivation script
reproduces the known D.1 147/109 and K.1E.3 163/93 as self-checks)
=========================================================================================
Config: 8-bit YUV420, threshold=255, strength=255, narrow, luma const 128 (y_response=254 every
sample). Standard D.2 case k=2: harness frames 0..3, N=3; anchor=output[0] (N-3), holes=output[1],
output[2] (N-2, N-1), target=output[3].

Source (constant planes, luma 128), distinct from the D.1 and K.1E.3 sets (so D.2 cannot pass on any
stale cache state):
      source[0]=128/72/184   source[1]=128/208/40   source[2]=128/176/72   source[3]=128/128/144

Chain:
      anchor  output[0] = 128/72/184    (fresh-start: copyFrame(source[0]))
      hole-1  output[1] = 128/148/96     (filled: blend(source[1], prev=output[0]))
      hole-2  output[2] = 128/149/95     (filled: blend(source[2], prev=output[1]))
      target  output[3] = 128/148/100    (computed: blend(source[3], prev=output[2]))  D.2 GOLDEN

PROOF STRATEGY -- IMPORTANT, read before relying on bytes:
At threshold=255 the recursive output moves only ~1 LSB frame-to-frame, so some target discriminators
are only 1 LSB apart (e.g. the skip-hole2 bug -- target computed from output[1] instead of output[2] --
gives 147/101 vs the correct 148/100). The exact-byte y4m checker DOES catch 1 LSB, but the target byte
alone is a fragile primary proof. D.2 correctness is therefore proven by, in priority order:
  (1) KDT MECHANISM (load-bearing): branch=RECOVER recover_branch=exact-anchor anchor=0 hole_count=2
      holes=[1,2] source_requests=[1,2,3] hole=1 outcome=computed hole=2 outcome=computed
      pin_list_size=3 pin_balance=0. This proves the TWO-hole fill and the THREE-pin batch discharge --
      the actual D.2 load-bearing thing.
  (2) HOLE BYTES via cache-hit follow-up: after the recovery populates output[1] and output[2], a
      follow-up request for frames 1 and 2 returns them as CACHE-HITs with the FILLED values
      output[1]=148/96 and output[2]=149/95 (well separated from each other and from wrong-fill) --
      proves the holes were filled correctly and IN ORDER, with comfortable byte margins.
  (3) TARGET byte output[3]=148/100 (corroborating).
Discriminators (target): passthrough(source[3])=128/144; wrong-pred(source[2])=163/86;
skip-both(prev=anchor)=... (well separated); skip-hole2(prev=output[1])=147/101 (1 LSB -- why (2) carries
the proof). All byte-distinct, but (1)+(2) are the primary proof, not (3).

=========================================================================================
SCOPE (plugin-side; cache-core primitives all EXIST -- no cache-core change expected)
=========================================================================================
arInitial -- recovery start (cnr3_start_live_recovery, in cnr3_arInitial.cpp):
  - call plan_bounded_recovery_search_and_record_anchor_pin(N, CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
    pin_list, out_plan) -- UNCHANGED from D.1 (the planner is already general; it bounds the interval
    and catalogues however many contiguous holes it finds).
  - apply the WIDENED accept gate (item 1). If accepted: record branch=recovery, carry the whole
    recovery_plan, derive source_request_frame_numbers = {N} U {all hole sources} from
    recovery_plan.hole_frame_numbers (ALREADY derived this way in D.1; for K==2 it yields {N-2,N-1,N}),
    request each, return NULL.
  - if NOT accepted: CLEAN REFUSAL -- discharge+delete frameData, setFilterError. The reason string is
    no-in-window-anchor for the anchor_found==false case (the code reports only what the bounded search
    observed; it does NOT distinguish out-of-window-anchor from no-prior-anchor -- see refusal note
    above). A separate reason (e.g. non-exact-or-non-contiguous-plan) applies when an in-window anchor
    WAS found but the plan fails the exact-anchor/contiguity gate.

arAllFramesReady -- recovery completion (cnr3_complete_live_recovery): UNCHANGED in structure. The
  existing ascending hole-fill loop runs K times; pre-compute adopt-and-skip per hole; AS2 hole-store
  per computed hole; target computed from the last filled hole (output[N-1]); authoritative target
  return; AS4 batch discharge of the (K+1)-entry pin_list on every exit path. VERIFY: loop indexing for
  K==2, and that the discharge count == pin_list_size == 3 (anchor + 2 holes).

KDT: the recovery trace is already vector-driven (holes=join(hole_frame_numbers),
  source_requests=join(...), per-hole outcome lines). For K==2 it should naturally emit hole_count=2
  holes=[1,2] source_requests=[1,2,3] and two "hole=N outcome=computed" lines. ADD/CONFIRM
  pin_list_size in the trace so the three-pin discharge is visible. The refusal path should emit a
  distinct KDT (e.g. NOT-YET / REFUSED branch=bounded-window-exceeded) so the harness can prove the
  refusal fired for the right reason (not a crash, not a silent wrong-branch).

FIVE GUARDS (unchanged from D.1; re-verify under K>=2):
  1. N (requested_frame) is the final TARGET, computed after holes filled -- not a hole.
  2. hole_frame_numbers = the planned contiguous output holes below N (K==2: {N-2,N-1}).
  3. source_request_frame_numbers = {N} U {source per hole}, DERIVED (K==2: {N-2,N-1,N}); never hardcoded.
  4. anchor frames are cache OUTPUTS, not source inputs -- do NOT request source[anchor].
  5. every consumed predecessor is pinned (anchor pin / hole-store pin) or obtained via a fresh
     lookup_frame_and_add_ref compute-ref released after use; the (K+1)-pin list is AS4-discharged.

=========================================================================================
MANDATORY fmParallel INTERLEAVING ANALYSIS (record in the D.2 design note; D.2 runs single-threaded,
per the charter the concurrency reasoning is recorded at design time, not deferred)
=========================================================================================
Carry the D.1 six-case analysis, and ADD the multi-hole specifics, each shown benign by citing the
existing mechanism:
  1. output[N] appears after cache-hit miss -> authoritative target return frees the loser, returns winner.
  2. a hole becomes present between planning and its compute -> pre-compute adopt-and-skip pins+adopts
     it (per-hole outcome adopted-skipped), skips compute. With K>=2 this can happen on a SUBSET of the
     holes -- the loop must handle a mix of computed and adopted-skipped holes in one plan.
  3. a LATER hole's predecessor (an earlier hole just computed by THIS activation) is the dependency
     chain -- confirm the ascending order guarantees each hole's predecessor is present (pinned/stored)
     before it is computed; this is the multi-hole ordering invariant.
  4. overlapping recovery activations fill the same hole(s) or target -> AS2 first-in-best-dressed: one
     winner, losers freed, each activation discharges exactly its own pins.
  5. anchor pruned before/after planning -> H.2A atomic does not select a pruned anchor; once pinned it
     is held across the gap (the pin is load-bearing).
  6. error/early-return after partial multi-hole planning or partial fill -> frameData discharge-before-
     delete releases EVERY recorded pin (anchor + any holes filled so far); no leak, no double-discharge.
     (This is more load-bearing at K>=2 than K==1 -- a mid-loop error must discharge the partial pin set.)

=========================================================================================
HARNESS / ACCEPTANCE (coordinator-side; built on the D.1 harness pattern)
=========================================================================================
SYNTHETIC source as above. Core-cache defeat: filtered.std.SetVideoCache(mode=0). vspipe -r 1.

RUN A -- multi-hole recovery (THE PROOF): request output frames 0 and 3, SKIP 1 and 2
  (filtered[0:1] + filtered[3:4]) -> when output[3] is requested, output[1] AND output[2] are ABSENT,
  output[0] is PRESENT (anchor at N-3). Expect branch=RECOVER hole_count=2 holes=[1,2]
  source_requests=[1,2,3] both holes outcome=computed pin_list_size=3 pin_balance=0; recovered
  output[3]=148/100.
RUN B -- hole-byte verification (the robust check): a render that, after recovery, re-requests frames
  1 and 2 so they return as CACHE-HIT with the filled values output[1]=148/96, output[2]=149/95.
  (Construct so recovery populates them first, then they are re-requested -- e.g.
  filtered[0:1] + filtered[3:4] + filtered[1:2] + filtered[2:3], checking the cache-hit positions.)
RUN C -- bounded-window refusal (THE NEW PROOF): a clip where the only present output is > 50 frames
  back from the target. e.g. render frame 0 (fresh-start anchor), then request frame 52 with 1..51
  absent (filtered[0:1] + filtered[52:53]); nearest anchor=0 is 52 back (> B=50) -> bounded-window
  refusal. Expect a clean refusal KDT (reason=no-in-window-anchor), non-zero exit or a documented
  refusal signal, and NO partial output / NO crash. The bounded-window-exceeded nature is proven BY
  CONSTRUCTION (we established output[0] then requested 52; nearest anchor is 52 back > B=50), NOT by a
  code-emitted "exceeded" label.
RUN D -- D.1 regression (k==1) and degenerate (k==0): the D.1 harness must still pass unchanged
  (recovery 147/109, branch=RECOVER anchor=0 holes=[1] hole=1 outcome=computed pin_balance=0).
RUN E -- negative control: sequential 0,1,2,3 all present -> frame 3 takes PREDECESSOR-PRESENT, NOT
  recovery.
RUN F -- passthrough: no [KDT].

PASS REQUIRES (both Debug AND Release):
  - RUN A: KDT mechanism exactly as above (hole_count=2, both computed, pin_list_size=3, pin_balance=0);
    recovered output[3]=148/100.
  - RUN B: cache-hit hole bytes 148/96 and 149/95 (the robust correctness proof).
  - RUN C: clean bounded-window refusal, correct reason, no partial state, no crash.
  - RUN D: D.1 single-hole regression green (147/109) -- REQUIRED (the accept gate changed).
  - K.1F cache-hit + branch-c regressions green (routing unchanged from D.1, but re-confirm).
  - four-way selftest 49/49 both configs (cache-core unchanged; if any cache-core change is needed,
    STOP and flag -- R-PROCESS-21 + charter).
  - RUN E negative control + RUN F passthrough clean.

=========================================================================================
PROCESS / QUESTIONS FOR THE CODER
=========================================================================================
- Plugin-side only: cnr3_arInitial.cpp (accept-gate widening + refusal reason), cnr3_arAllFramesReady.cpp
  (verify the existing loop + discharge under K==2; add pin_list_size to KDT if not present),
  cnr3_plugin_internal.h (gate rename / any vocab), cnr3_build_config.h (edit marker). Cache-core
  primitives reused unchanged. Expect NO cache-core change; if one is needed, STOP and flag.
- R-PROCESS-23 delivery: canonical-LF patch on current committed HEAD (post-D.1). Designer read-first
  before commit.
QUESTIONS:
  1. Confirm the widened accept gate is exactly "exact-anchor, contiguous holes, anchor==N-(K+1)" and
     that K==0/1 remain accepted (D.1 + degenerate regression).
  2. Confirm the existing completion loop + AS4 discharge handle K==2 with no off-by-one and
     discharge_count==pin_list_size==3.
  3. Confirm the refusal path emits no-in-window-anchor for anchor_found==false (NOT a distinction the
     code cannot cheaply make), with a separate reason for in-window-anchor-but-failed-gate, and that
     both are leak-clean. If you find an existing primitive that distinguishes out-of-window-anchor from
     no-prior-anchor for free, say so; otherwise do not add one.
  4. Confirm RUN C's >50-frame construction actually triggers anchor_found==false (the planner returns
     no in-window anchor), not some other path -- and whether a long synthetic clip is the right way or
     you see a cleaner way to force "nearest anchor beyond B".
  5. Any cache-core entry point you find missing for the K>=2 composition (we expect none).
