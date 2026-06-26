# CNR3 — THIS-CHAT DELTA: keystone through K.1F (current-state companion)

**Version:** v4.1 (DELTA through K.1G + CMS07.10)
**Date:** 2026-06-26
**Supersedes:** the v2/v3 DELTA (which covered K.1A–K.1E branch-(c)). This v4.x carries the state
forward across the K.1E.2, K.1E.3, ledger/rules refresh, CMS07.9, Recovery-Step-0, K.1F,
CMS07.10, and K.1G commits, and states the immediate next action (branch-(d) D.1 recovery).
(v4.1 adds the K.1G plugin source split.)
**Role:** newest current-state record. Companion to Document B (Document B = format-of-record;
this = freshest delta). **If this conflicts with the repository on build state, the repository
wins** — confirm `CNR3_EDIT_VERSION` and the selftest count from committed source as the first
action.

---

## 1. CURRENT BASELINE (confirm from repo)

```text
Committed/pushed through:  CMS07-K.1G-plugin-source-split-no-behaviour-proof
                           (K.1G = source-organisation-only split of the live getFrame path;
                            no behaviour change; proven four-way 49/49 + K.1F live harness, both
                            configs. The K.1F cache-hit return is the last behavioural keystone.)
Selftest count:            49/49 PASS  (forced-fail 48/49 exit 1; verbose 49/49)  [unchanged by K.1G]
Branch:                    dev_cache_manager
Repo:                      github.com/hydra3333/vapoursynth-cnr3
Controlling CMS:           CMS07.10  (cnr3_cache_manager_design_v7_10.md)
Companion (non-normative): v7.10     (CNR3_CMS_Future_Investigations_and_Open_Questions_v7_10.md)
Production Spec:           v2.8      (§3A register; PDAP / R-PROCESS-20..23)
Filter registration:       fmUnordered, dependency { source, rpGeneral }, no no-cache flag
Default response config:   threshold_8bit=255, strength_8bit=255, curve=narrow (all planes)
                           (CNR3_K1E2_PROOF_DEFAULT_*, vapoursynth-Cnr3.cpp ~L183-184)
```

**NOTE on a pending doc fix:** the corrected CMS07.10 (four editorial/consistency fixes —
§9.7.1 branch-(b) wording aligned to R-LIFECYCLE, 07.10 front-matter summary, companion version
pointer, correction-block heading) was staged at end of this chat and is to be committed FIRST
next session (overwrite the committed v7.10 with the corrected one; no companion change needed).
If the repo's committed v7.10 still says "no source is requested" in §9.7.1 branch-(b), it is the
pre-fix version and the corrected one should replace it before D.1.

---

## 2. THE LIVE getFrame DISPATCH — NOW COMPLETE EXCEPT RECOVERY

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

The live dispatch handles all four branches except recovery's absent-N fall-through:

```text
arInitial dispatch order (present-N FIRST):
  1. output[N] present        -> CACHE-HIT          (branch-b)  DONE (K.1F)
  2. else N > 2               -> clean refusal (after-frame2-before-recovery-wiring)
  3. else N == 1 || N == 2    -> PREDECESSOR-PRESENT (branch-c)  DONE (K.1E.2/E.3)
  4. else N == 0              -> FRAME0-FRESH-START  (branch-a)  DONE (K.1D)
  (recovery branch-d will become the absent-N fall-through, replacing the n>2 refusal)

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

## 5. IMMEDIATE NEXT ACTION — branch-(d) D.1 (exact-anchor single-hole recovery)

**Sequencing:** (1) commit the corrected CMS07.10 (§1 note). (2) Then D.1.

**D.1 shape:** request output[N], where output[N-1] is ABSENT and output[N-2] is PRESENT (the
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

**Owed before the coder scope (designer actions):**
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
D.1  exact-anchor SINGLE-hole recovery                         <- NEXT
D.2  exact-anchor MULTI-hole (k>=2; multi-pin discharge load-bearing) + bounded-window refusal
D.3  floor-fresh-start recovery (copyFrame base + walk forward)
D.4  pre-compute adopt-and-skip, synthetically forced (proves CMS07.9 §9.6.5 two-outcomes)
D.5  recovery under live prune pressure
```
Design toward fmParallel + two-instance-interlaced stays the forward goal.

**Branch-(b) live cache-hit RETURN is done (K.1F).** D.1 is strictly the absent-N recovery path;
do NOT fold cache-hit into it (it is already its own committed keystone).

---

## 6. OWED-ITEMS LEDGER (none blocking D.1)

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
through **K.1G** (plugin source split, no behaviour change); count 49/49.

— End of CNR3 THIS-CHAT DELTA through K.1F, v4.
