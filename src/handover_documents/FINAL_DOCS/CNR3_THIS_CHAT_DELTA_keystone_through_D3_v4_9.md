# CNR3 — THIS-CHAT DELTA: keystone through K.1F (current-state companion)

**Version:** v4.9 (DELTA through D.3 — floor-fresh-start recovery committed; CMS07.13)
**Date:** 2026-06-26
**Supersedes:** the v2/v3 DELTA (which covered K.1A–K.1E branch-(c)). This v4.x carries the state
forward across the K.1E.2, K.1E.3, ledger/rules refresh, CMS07.9, Recovery-Step-0, K.1F,
CMS07.10, K.1G, and D.1 commits, and states the immediate next action (branch-(d) D.2 multi-hole recovery).
(v4.4 records D.1 landed: the live getFrame dispatch now handles ALL FOUR branches including recovery.)
(v4.1 adds the K.1G plugin source split.)
**Role:** newest current-state record. Companion to Document B (Document B = format-of-record;
this = freshest delta). **If this conflicts with the repository on build state, the repository
wins** — confirm `CNR3_EDIT_VERSION` and the selftest count from committed source as the first
action.

---

## 1. CURRENT BASELINE (confirm from repo)

```text
Committed/pushed through:  CMS07-D.3-live-floor-fresh-start-recovery-proof
                           (D.1 = first live branch-(d) recovery: reconstruct absent output[N-1]
                            from a pinned anchor at N-2, then compute output[N]. A-safe-1 routing.
                            Proven both configs: recovery 128/147/109 branch=RECOVER anchor=0
                            holes=[1] outcome=computed pin_balance=0; branch-c regression 145/111
                            & 147/109; K.1F cache-hit 147/109; negative control; four-way 49/49.
                            Prior: K.1G plugin source split (no behaviour change); K.1F cache-hit.)
Selftest count:            49/49 PASS  (forced-fail 48/49 exit 1; verbose 49/49)  [unchanged by D.1;
                           D.1 is plugin-only, no cache-core change]
Branch:                    dev_cache_manager
Repo:                      github.com/hydra3333/vapoursynth-cnr3
Controlling CMS:           CMS07.13  (cnr3_cache_manager_design_v7_13.md)
Companion (non-normative): v7.10     (CNR3_CMS_Future_Investigations_and_Open_Questions_v7_10.md)
Production Spec:           v2.9      (§3A register; PDAP / R-PROCESS-20..23)
Filter registration:       fmUnordered, dependency { source, rpGeneral }, no no-cache flag
Default response config:   threshold_8bit=255, strength_8bit=255, curve=narrow (all planes)
                           (CNR3_K1E2_PROOF_DEFAULT_*, vapoursynth-Cnr3.cpp ~L183-184)
```

**CMS07.10 status (reconciled; VERIFY against the repo, do not assume):** the corrected CMS07.10
(four editorial/consistency fixes — §9.7.1 branch-(b) wording aligned to R-LIFECYCLE, 07.10
front-matter summary, companion version pointer, correction-block heading) was committed during the
producing session along with the refreshed handover set (see §7). FIRST ACTION before D.1: CONFIRM the
repo's committed cnr3_cache_manager_design_v7_10.md §9.7.1 branch-(b) reads "no source is needed or
consumed for the result" (corrected), NOT "no source is requested" (pre-fix). If for any reason the
repo still holds the pre-fix version, commit the corrected one before D.1. (This supersedes an earlier
draft note that said the corrected CMS07.10 was merely staged; it was subsequently committed. The
later doc updates — DELTA v4.1/v4.2, and any post-K.1G refresh — may still be uncommitted; verify.)

---

## 2. THE LIVE getFrame DISPATCH — NOW COMPLETE (ALL FOUR BRANCHES LIVE)

**Source layout since K.1G (where D.1 wiring lands):** the live getFrame path is split into
`src/cnr3_arInitial.cpp` (branch-START: present-N cache-hit start, n>2 refusal, predecessor start,
fresh-start start — via `cnr3_arInitial` dispatcher, present-N FIRST), `src/cnr3_arAllFramesReady.cpp`
(branch-tag EXECUTION via `cnr3_arAllFramesReady` switch on frameData branch; trigger-source release;
KDT traces; frameData cleanup), and `src/cnr3_plugin_internal.h` (private shared structs/enums/decls:
Cnr3FilterData, Cnr3LiveGetFrameBranch, Cnr3LiveGetFrameFrameData, Cnr3LiveCacheHitStartResult, and
the helper declarations). `vapoursynth-Cnr3.cpp` keeps registration/create/free + the small
activation-reason dispatcher (`cnr3_get_frame_keystone_live_k1f_proof`). D.1 adds a recovery
`_start_`/`_complete_` pair into these two files alongside the proven branches — NOT a new monolith.
The new .cpp files are in the cnr3 DLL project only; the selftest project compiles neither.

The live dispatch handles ALL FOUR branches (recovery landed in D.1 as the absent-N fall-through):

```text
arInitial dispatch order (A-safe-1; each decision is a find-and-pin, no naked peek):
  1. try-pin output[N]         -> CACHE-HIT           (branch-b)  DONE (K.1F)
  2. else N == 0               -> FRAME0-FRESH-START   (branch-a)  DONE (K.1D)
  3. else try-pin output[N-1]  -> PREDECESSOR-PRESENT  (branch-c)  DONE (K.1E.2/E.3; entry contract
                                  changed in D.1 to accept the already-recorded predecessor pin)
  4. else                      -> RECOVERY             (branch-d)  DONE (D.1, exact-anchor single-hole;
                                  D.1 accept gate restricts to hole_count==1&anchor==N-2 or
                                  hole_count==0&anchor==N-1; other plans clean-refuse pending D.2/D.3)

arAllFramesReady: dispatched by the frameData branch tag set at arInitial
  (Cnr3LiveGetFrameBranch: none / cache_hit_return / frame0_fresh_start /
   predecessor_present_compute), NEVER by re-inspecting frame state -> a concurrent cache
   change cannot cause a different branch to execute than the one planned.
```

---

## 3. WHAT LANDED THIS SESSION (all committed/pushed unless noted)

**K.1E.2** — live frame-1 predecessor-present compute. Golden: source[1]=128/224/32 ->
output[1]=128/161/95.

**K.1E.3** — recursive filtered-predecessor distinction at N=2; CLOSES R-ARCH-06. Golden:
source[2]=128/192/64 -> output[2]=128/163/93, reachable ONLY from cached filtered output[1]=161/95
(source-substitution bug -> 222/34; passthrough -> 192/64; all byte-distinct). Bounded n==1||n==2,
n>2 refusal.

**Ledger/rules refresh** — Document_A v3.4, Document_B v3.4, DELTA v3, Production Spec v2.8
(authored R-PROCESS-23: patch validation must match target environment — canonical-LF base,
compile Debug+Release before green, proof-level honesty, diagnose-failure-class, no
context-narrowing).

**CMS07.9** — additive over 07.8. Made pre-compute adopt-and-skip NORMATIVE in §9.2 recovery
per-hole fill (check-present-and-pin-and-skip before computing a hole, in case a concurrent
activation filled it during the arInitial->arAllFramesReady gap; correctness already from
post-compute first-in-best-dressed §9.3, this adds efficiency under fmParallel). New §9.6.5.
Caught a real fmParallel assumption: AS3-"unreachable" was a plan-time claim presented as
act-time; under fmParallel branch-(a) "hole already present at act-time" IS reachable. Code
confirmed: plan_bounded_recovery_search_and_record_anchor_pin does search+pin+record under one
cache_mutex_ (AS1 atomic, allocation pre-reserved outside the lock).

**Companion v7.9** — FI-04 resolved into CMS §9.7.7; FI-05 (two-instance resource model under
fmParallel — likely genuine design gap, NOT blocking branch-d); FI-06/07 (hot-zone concurrency);
FI-08 (first-in-best-dressed prune as one mutex critical section; count-based guard
CONSIDERED+DEFERRED).

**Recovery-Step-0** (AS4 single-lock batch discharge) — public Cnr3CachePinList::discharge_all
delegates to Cnr3OutputCacheCore::discharge_pin_list taking cache_mutex_ ONCE, walking via
unpin_frame_locked. Selftest 48->49 (case 7 = single-lock structural proof). Cache-core only.

**K.1F** (this session's main work) — live branch-(b) direct cached-output return. See §4.

**CMS07.10** — CORRECTION to §9A.1.1 (R-LIFECYCLE), proven by K.1F. See §4. (Corrected version
staged at end of chat; commit first next session — see §1 note.)

---

## 4. K.1F + R-LIFECYCLE (the API4 lifecycle resolution — important, don't re-derive)

**The question:** can a cache-hit (output[N] already present) return without the arAllFramesReady
phase? Investigated via multiple AI reviews + R76 doc + the R76 vsthreadpool completion path.

**The finding (settled, do not re-open without new authoritative source):**
- **Option A** (return cached frame directly at arInitial): documented only for SOURCE filters;
  NOT established safe for a non-source filter under fmParallel. REJECTED.
- **Option B** (zero-request arInitial->NULL, then return at arAllFramesReady): DISPUTED — a
  getFrame that requests ZERO frames at arInitial and returns NULL is **not guaranteed an
  arAllFramesReady callback** under R76 (zero-pending may be terminal). Not confirmable from
  quotable core source. REJECTED.
- **Option C** (ADOPTED): request exactly ONE real source frame (source[N]) at arInitial as a
  lifecycle TRIGGER to guarantee arAllFramesReady fires; return the cached output there. Valid
  under every reading, strictly inside the documented contract.

**R-LIFECYCLE (now normative in CMS §9A.1.1):** EVERY CNR3 getFrame branch requests >=1 REAL
source frame at arInitial and returns ONLY at arAllFramesReady. The branch-(b) cache hit requests
source[N] as a trigger (retrieved and IMMEDIATELY FREED at arAllFramesReady — a normal owned ref,
not consumed for compute, not stored, freed outside any cache lock). Honest cost: a cache-hit
return can be blocked by a source[N] failure even though output[N] is cached (accepted; output[N]
was produced from that same source in the same graph).

**"need locking" (settled):** means the filter's OWN mutex protecting shared per-instance state
(CNR3's cache_mutex_), a property of internal design, NOT imposed by mode and NOT constraining
fmParallel. CNR3 already satisfies it mode-independently. The fmUnordered doc sentence is advice
to avoid fmSerial, not a constraint blocking fmParallel.

**K.1F IMPLEMENTATION (plugin-only; lookup_frame_and_record_pin / lookup_frame_and_add_ref /
discharge_all — all pre-existing):**
```text
arInitial cache-hit:  lookup_frame_and_record_pin(N, frameData.pin_list)  -- pins output[N] so
                        a concurrent prune cannot evict it across the gap (AS1 rationale applies)
                      record branch=CACHE-HIT, requested_frame=N
                      requestFrameFilter(N, source)  -- the trigger
                      return NULL
arAllFramesReady:     getFrameFilter(N) -> freeFrame immediately (trigger, not consumed)
                      lookup_frame_and_add_ref(N)  -- present by pin; if absent -> invariant
                        violation, surfaced, NOT a garbage return (defensive assert)
                      discharge_all (Step-0 batch discharge; first live use in getFrame)
                      transfer cached ref to caller
```
Field rename this session: frameData `predecessor_pin_list` -> `pin_list` (now shared by the
predecessor and cache-hit branches).

**K.1F PROOF (Debug + Release):** four-way unchanged 49/49; live harness green — cache-hit returns
output[2]=128/163/93 with branch=CACHE-HIT, pixel_compute=0/p11b=0/p11c=0, trigger
requested=1/retrieved=1/consumed=0/released=1, cache_hit_pin_balance=0; regression intact
(1->161/95, 2->163/93 still compute on first request); negative control holds (first/uncached
request does NOT take cache-hit); repeated-frame-0 proves present-N dispatch precedes the n==0 gate.

**HARNESS LESSON (carry to D.1):** CNR3 has a normal downstream VS core cache, so a re-request of
an already-produced frame could be served by the CORE cache and never re-enter CNR3::getFrame
(false pass). Defeat it with `clip.std.SetVideoCache(mode=0)` on the CNR3 node (R76 mode=0 =
always disable). NOTE: SetVideoCache is a side-effecting node method that RETURNS None — call it,
do NOT reassign (`filtered.std.SetVideoCache(mode=0)`, not `filtered = ...`). The CACHE-HIT KDT
line is the definitive self-validating proof: present => getFrame re-entered and branch fired;
bytes-match-WITHOUT-KDT => core cache intercepted => INCONCLUSIVE (not a pass). Harness files:
test_K1F_once_only_harness_AB.vpy / .bat + check_y4m_constant_plane.py (committed with K.1F).

---

## 5. branch-(d) D.1 (exact-anchor single-hole recovery) — DONE; NEXT IS D.2

**STATUS:** D.1 is COMMITTED and proven both configs (see §1). The brief below is retained as the
design record of how D.1 was built and how the D.2-D.5 arc continues. IMMEDIATE NEXT ACTION is now
**D.2 (exact-anchor MULTI-hole, k>=2 — multi-pin discharge load-bearing — plus bounded-window
refusal)**. The D.1 recovery harness (test_D1_once_only_harness_AB.vpy/.bat, reusing the K.1F y4m
checker) exists and must stay green as a D.2 regression. The settled D.1 routing/design (A-safe-1,
no-naked-peek hard floor, dissolved source window, authoritative target return, P.11C deferral) is
the foundation D.2 extends; the D.1 accept gate is widened in D.2 to admit k>=2 holes with the
multi-pin discharge becoming the load-bearing new proof.

**Sequencing (historical, D.1):** the corrected CMS07.10 was confirmed committed, then D.1 was
built, reviewed, proven both configs, and committed.

**D.1 shape (as built):** request output[N], where output[N-1] is ABSENT and output[N-2] is PRESENT (the
anchor). Recovery is the slot-4 absent-N fall-through, reached only when N absent AND N-1 absent,
sitting cleanly on the K.1F-complete dispatch.
```text
arInitial:  H.1A/H.2A plan -> pin anchor at N-2 -> carry plan+pin-list across gap (K.1E.1
            pattern) -> request source set {N-1, N}
            [lands in cnr3_arInitial.cpp as cnr3_start_live_recovery_* alongside the proven
             branch-start functions; add the recovery branch tag to Cnr3LiveGetFrameBranch and
             the cnr3_arInitial dispatcher as the absent-N fall-through (replacing the n>2 refusal)]
arAllFramesReady: fill hole N-1 from the anchor via P.11B (H.3A store), compute output[N] from
            the now-present N-1, return it, batch-discharge the multi-pin list (Step-0)
            [lands in cnr3_arAllFramesReady.cpp as cnr3_complete_live_recovery_*; add a case to
             the cnr3_arAllFramesReady branch-tag switch]
```

**Branch-(d) is COMPOSITION of proven primitives** (H.1A search; H.2A AS1 anchor pin+record under
one lock; H.3A per-hole AS2 consumer; Step-0 batch discharge; P.11B compute; K.1E.1 frameData
pin-gap pattern). getFrame currently has ZERO recovery wiring. Carried plan struct
Cnr3KeystoneRequestPlan already has hole_frame_numbers AND source_request_frame_numbers.

**Owed before the coder scope (designer actions) — ALL COMPLETED for D.1:**
1. COMPUTE the D.1 golden chain — output[N-2] anchor, output[N-1] filled hole, output[N] — each
   byte-distinct from source / passthrough / wrong-anchor, verified against the real response
   tables (threshold 255 / strength 255 / narrow) and the P.11B blend math
   (cnr3_frame_processing.cpp: build_cnr3_weight_table cosine curve;
   cnr3_calculate_combined_blend_weight ~L882; shift-rounding blend ~L866+; shift2=bits<<1),
   the same method that produced 161/95 and 163/93.
2. DRAFT the D.1 coder build scope, folding in the agreed refinements (below).
3. SETTLE the recovery harness construction: needs SetVideoCache(mode=0) (K.1F lesson) AND a way
   to create the "N-1 absent but N-2 present" starting cache state (prune-pressure or seek
   pattern — open construction question).

**Coder-agreed D.1 refinements (apply from D.1):**
- Use FINAL recovery-shaped frameData + KDT vocabulary from D.1 (vectors for holes / source
  requests / per-hole outcomes — NOT single-hole-only scalar fields that D.2 would have to
  reshape). D.1 harness just sets hole_frame_numbers={N-1}, source_request_frame_numbers={N-1,N}.
- KDT fields distinct: instance=<filter_instance_id> and N=<requested_frame> (NOT instance=N).
  recover_branch=exact-anchor|floor-fresh-start; anchor=<frame>; hole_count; holes; source_requests.
- Per-hole outcome stable names from D.1: {computed, adopted-skipped, adopted-post-compute-loser}.
- Five implementation guards: (a) N is the final target, NOT a hole; (b) hole_frame_numbers =
  planned output holes below N; (c) source_request_frame_numbers = all holes PLUS N (plus floor
  later); (d) anchor frames are cache OUTPUTS, not source inputs — do NOT request source[anchor];
  (e) every consumed predecessor pinned (AS1/AS2/AS3) or re-checked/adopted under lock.
- Source-request sets: D.1 {N-1,N}; D.2 {holes...,N}; D.3 {floor...,N}. Requesting a source that
  goes unused (pre-compute adopt-and-skip fired) is fine (VS-LIFECYCLE-01 forbids RETRIEVING an
  unrequested frame, not requesting one that becomes unnecessary).

**Branch-(d) ARC (one step at a time, each harness-proven Debug+Release):**
```text
D.1  exact-anchor SINGLE-hole recovery                         DONE (committed; both configs)
D.2  exact-anchor MULTI-hole (k>=2) + bounded-window refusal    DONE (committed; both configs)
D.3  floor-fresh-start recovery (copyFrame base + walk forward)  DONE (committed; both configs)
D.4  pre-compute adopt-and-skip, synthetically forced (proves CMS07.9 §9.6.5 two-outcomes)
D.5  recovery under live prune pressure
```
Design toward fmParallel + two-instance-interlaced stays the forward goal.

**Branch-(b) live cache-hit RETURN is done (K.1F).** D.1 is strictly the absent-N recovery path;
do NOT fold cache-hit into it (it is already its own committed keystone).

---

## 6. OWED-ITEMS LEDGER (none blocking D.4)

- **D.3 FLOOR-FRESH-START — refuse->floor conversion + materialized-floor invariant (committed; recorded).**
  D.3 wired the CMS §9.5 floor fallback: when the bounded descending search finds no in-window anchor,
  recovery fresh-starts the floor max(0,N-B) (copyFrame(source[floor]), chroma unchanged) then walks
  ascending to N. This CONVERTED the D.2-era no-in-window-anchor REFUSAL into floor-fresh-start for
  reachable in-range requests; genuine refusal now narrows to structural/impossible cases (corrupt or
  non-contiguous plan, n<=0, source/alloc failure). D.2's RUN C bounded-window refusal is therefore a
  TRANSITIONAL proof, superseded (the old "request output[52] after only output[0]" now floor-starts at
  floor=max(0,52-50)=2 and walks forward; it does NOT refuse). The materialized-floor-is-the-foundation
  invariant (once the floor is fresh-started/stored/pinned it IS the validated consumer foundation,
  equivalent to a discovered anchor; the structural hole-wrapper validates presence+contiguity, not
  provenance) is recorded in THREE places: the D.3 patch in-code comment at the anchor relabel, CMS
  §9.5 (CMS07.13), and here. Bounded-resource fact: floor-start may request up to B+1=51 source frames
  {floor..N} (derived, never hardcoded) — bounded, never the whole clip. Proven both configs:
  output[3]=144/113 recover_branch=floor-fresh-start floor=0 floor_outcome=computed hole_count=2
  holes=[1,2] pin_list_size=3 pin_balance=0; floor byte 56/176 (fresh-start chroma-unchanged proof);
  D.2 exact-anchor regression 148/100 (recover_branch=exact-anchor, not floor); D.1 147/109; four-way 49/49.

- **TEST ARTIFACTS / GOLDEN PROVENANCE (housekeeping; add to repo alongside the existing harnesses).**
  The recovery harnesses and their golden-derivation scripts should live in the repo test area as
  regression bases for later phases (D.3+ must keep D.1 and D.2 green):
    * test_D1_once_only_harness_AB.vpy/.bat (D.1 single-hole), test_D2_once_only_harness_AB.vpy/.bat
      (D.2 multi-hole + bounded-window refusal), and test_D3_once_only_harness_AB.vpy/.bat (D.3
      floor-fresh-start RUN A + floor-byte/hole cache-hit RUN B + D.2 exact-anchor regression RUN C +
      D.1 regression RUN D + negative control RUN E + passthrough RUN G). All reuse
      test_K1F_check_y4m_constant_plane.py.
    * D1_golden_chain_derivation.py, D2_golden_chain_derivation.py, and D3_golden_chain_derivation.py
      are the GOLDENS PROVENANCE:
      small Python that INDEPENDENTLY re-derives the expected pixel values (D.1 147/109; D.2 anchor
      72/184 -> holes 148/96, 149/95 -> target 148/100) from the response-table + P.11B blend maths,
      (D.3 floor 56/176 -> holes 144/111, 145/109 -> target 144/113), and self-check by reproducing the
      known K.1E.3 161/95 & 163/93. They are the "answer key with its working shown" that proves the
      harness goldens are independently correct, not circular.
      Keep them with the harnesses; extend the same way for D.3-D.5 chains. NOT in the coder pack /
      not controlling design; they are test provenance.

- **D.2 BOUNDED-WINDOW REFUSAL — code reports only what the bounded search observed (decided, scope v2).**
  D.2's recovery refusal emits a SINGLE honest reason: **no-in-window-anchor** (anchor_found==false for
  the bounded interval [max(0,N-B), N-1], B=50). The code does NOT distinguish "an older anchor exists
  beyond the window" from "no prior output exists at all" — both are identical to a bounded search, and
  telling them apart would require an UNBOUNDED out-of-window search, defeating the purpose of bounding.
  The HARNESS proves the bounded-window-exceeded case BY CONSTRUCTION (Run C: establish output[0], then
  request output[52]; nearest anchor is 52 back > B=50). No cache-core helper or out-of-window search is
  added merely to enrich a reason string. (If an EXISTING primitive distinguishes the two for free, the
  richer label may be emitted, but nothing is to be added for it.) Rationale: diagnostic honesty (a label
  means only what the code knows) + avoid an unbounded scan in a refusal path. Decided designer+coder
  2026-06-27 (D.2 scope v2, coder review correction 2).

- **CMS-GAP RESOLVED (into CMS07.12) — bounded-search reporting semantics now explicit in the CMS.**
  STATUS: DONE. The clarification was added to CMS §9.5 Phase 1 (the search reports only within
  [max(0,N-B), N-1]; no-in-window-anchor does not distinguish out-of-window-anchor from no-prior-output;
  refusal reports no-in-window-anchor only) and the interval upper bound corrected to N-1. No behaviour
  change (D.2 already ships it). Relevant to D.3 (same no-in-window-anchor boundary). Original candidate:** The D.2 refusal decision above rests on a principle that recurs in D.3 (floor-
  fresh-start) and D.5 (prune pressure): *the bounded recovery search reports presence/absence only
  within [max(0,N-B), N-1]; absence within the window is not distinguished from absence of any prior
  output, and no out-of-window search is performed.* This is arguably ENTAILED by "bounded search" and
  may need no CMS text — OR a one-line clarification in CMS §9.1/§9.5 would make the boundary's semantics
  explicit for every recovery phase and would have prevented the D.2 scope's initial over-specification.
  RAISED as a CMS-GAP candidate (charter case b, low bar) for coordinator decision: add the clarifying
  line at the next CMS edit (e.g. bundled with the Doc A regeneration session), defer, or judge it
  already-implied and skip. NOT blocking D.2 (scope v2 stands regardless). Recorded so the principle is
  not silently re-introduced as an overreach in a later phase.

- **DOC-SET REGENERATION — Document A and Document B (deferred to a near-future dedicated session).**
  The current-era handover docs are refreshed (Production Spec v2.10, Role Handover v1.9, Reviewer
  Introduction v3.2, Coder Restart Introduction v6.0, this DELTA) and all carry the new Design
  Alignment and Escalation Charter (full text in Production Spec §3A.5.0 and Role Handover Part 3 §D0).
  STILL OWED:
    * **Document A** is at **v3.4 on the stale K.1D / CMS07.8 / 47-47 baseline** (it self-describes as
      "only a version number bump"; the Production Spec long noted "until Document A is regenerated").
      Needs a full STATE regeneration to **v3.5**: CMS07.8 -> CMS07.10, committed-through K.1D -> D.1,
      47/47 -> 49/49, K.1E-branch-(c)-in-flight -> all-four-branches-live, AND it must faithfully
      reproduce the Production Spec §3A register INCLUDING the charter at §3A.5.0 (per R-PACK-02:
      Document A reproduces §3A; on mismatch §3A wins).
    * **Document B** — confirm the committed current version (a v3.5 with the K.1F update block was
      produced; verify it is the repo's latest and bump to carry the D.1 + K.1G state + charter
      pointer if not already present). Target **v3.5** (or higher).
  ACTION TRIGGER: the **Coder Restart Introduction v6.0 already forward-references Document A v3.5 and
  Document B v3.5** as expected current versions (with a built-in warning that an older Doc A is stale).
  Do this in a near-future dedicated session — it is a large rewrite, deliberately NOT rushed at the
  tail of a long chat (the lesson from prior sudden chat deaths). NOT blocking D.2.

- **SCENE-CHANGE / P.11C deferral (shared across ALL live branches — record kept here so it is
  actioned, not lost).** The live getFrame keystones (branch-c K.1E.3, and branch-d D.1 onward)
  compute via P.11B and DEFER the P.11C scene-change/reset check, emitting
  `p11c_called=0 scene_change_deferred=1`. CMS §9.2 / §6.4 specify the COMPLETE recovery+compute as
  scene-change-aware: a cut detected during compute makes that output a fresh-start (copy source
  chroma, skip the recursive blend) and stores it WITH the checkpoint flag set (a cut frame is the
  ideal recovery anchor, §9.5/§12B). Deferring P.11C is correctness-safe ONLY while inputs contain no
  cuts (true for the synthetic constant-plane harnesses); on real footage a missed cut would blend
  across a scene boundary (wrong output) AND fail to establish the cut-checkpoint that recovery relies
  on. ACTION TRIGGER (action reasonably as soon as safe): wire P.11C UNIFORMLY across branch-a/c/d as
  its own keystone — NOT folded into any single branch — at the FIRST of: (i) before any real-footage /
  non-synthetic test; (ii) before branch-(d) is exercised on content where cuts can occur; (iii) once
  the live dispatch is otherwise complete (post-D.1..D.5) and before the keystone series is declared
  done. It must not be deferred past the point where real video is processed. Owning: CMS §9.2 (compute
  scene-change-aware), §6.4 (cut frame -> checkpoint), §A4 (in-compute accumulation/threshold), P.11C
  (proven in cache-core selftest, awaiting live wiring).
- branch-(d) isolated-pin causal proof.
- K.1E.2/E.3/K.1F proof-default response-table config -> real instance-config option parsing.
- longer sequential run beyond N==2; end-of-run integrity report + abort_on_error (default False)
  + warn-vs-hard-fail severity policy.
- full CMS fmParallel implications review (the CMS07.9 skim was non-exhaustive; FI-05 two-instance
  resource model likely genuine gap).
- fmParallel-phase (companion FI register): FI-05/06/07/08; operational two-instance diagnostics;
  test-tunable hot-zone/prune thresholds; cache-hit fast-path (Option A/B) revisit only if the one
  trigger-fetch on cache-hit is ever measured as significant AND confirmed from quotable core source.

---

## 7. DOCUMENT SET (current versions — for the new-chat reading order)

```text
CMS (design authority)     cnr3_cache_manager_design_v7_10.md            (commit corrected version first)
Companion (non-normative)  CNR3_CMS_Future_Investigations_..._v7_10.md
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_8.md     (§3A; R-PROCESS-20..23)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md         (§2.8 keystone KDT)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v1_7.md   (role + disciplines D1-D16)
Current-state (format)     Document_B_..._v3_4.md  + THIS delta (newest)
Introduction (entry pt)    CNR3_Handover_Introduction_to_new_chat_v3_x   (NEEDS state refresh — see note)
```
**Authority:** CMS -> Production Spec §3A -> diagnostics -> handover pack. Repository wins over any
document on build state.

**Doc-set status (refreshed and committed):** the handover set has been brought current —
Introduction **v3.1**, Role Handover **v1.8**, Document B **v3.5**, Production Spec **v2.9**,
CMS **v7.10** (corrected) + companion **v7.10**, and this DELTA. All committed. The superseded
CMS design docs v7.7.1/v7.8 were pruned (retained in git history). Build state is now committed
through **K.1G** (plugin source split, no behaviour change); count 49/49. CMS housekeeping (non-blocking): the
CMS §8.7 AS4 note still describes the single-lock batch discharge as "owed as recovery-step-0" and
`discharge_all` as "one lock per token" — both now COMMITTED (recovery-step-0). A tiny CMS currency
touch-up (owed -> committed) can ride the next CMS edit; it is description-lag, not an incorrectness,
and does not block D.1.

— End of CNR3 THIS-CHAT DELTA through K.1F, v4.
