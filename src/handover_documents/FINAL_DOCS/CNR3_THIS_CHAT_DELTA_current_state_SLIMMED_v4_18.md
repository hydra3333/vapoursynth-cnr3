# CNR3 — THIS-CHAT DELTA: current-state companion (SLIMMED, through W.3 + tiny-cache scaffold + AVX2 + marshalling levers 0A/0B) — v4.18

**Version:** v4.18 (SLIMMED). Supersedes v4.17. Records the opening of the **marshalling-optimisation arc** (acting on FI-10) and three committed steps, each proven 56/56 four-way (dev-trace ON) and measured:
(1) **AVX2 + x64-only build** (isolated commit). `/arch:AVX2` (`EnableEnhancedInstructionSet=AdvancedVectorExtensions2`) on Release+Debug of BOTH projects (plugin + selftest); Win32/x86 removed from both `.vcxproj` and the `.slnx`. AVX2 is a documented HARD requirement (Haswell 2013 / Excavator-Zen 2015+; hard-faults on older CPUs) — in build_config header, README, release notes. Proven NEUTRAL: 56/56 with AVX2 on, and the flag alone changed NOTHING in the profile (~50% marshalling unchanged) because the per-sample `cnr3_load_native_plane_sample` CALL is an auto-vectorisation WALL — confirming Lever 3 (typed pointers) is what unlocks the latent AVX2 benefit.
(2) **Lever 0A — staged native luma passthrough** (committed). Removed the full-res luma native->int->native round-trip; native active-row copy into `staged_y`, committed at the existing all-or-nothing Y/U/V gate. Value-preserving (P.11B unchanged). MEASURED: total CPU **-28%** (93,914->67,780 samples, 3500f -r 1 AVX2 Release); `stage_scalar_plane_to_native_bytes` 15,739->5,297; one fewer int alloc/frame. Win came from the REPACK side, bigger than predicted (luma = largest plane).
(3) **Lever 0B — direct scalar->native staging** (committed). Removed the redundant inner `resolved_bytes` buffer + second pass in U/V staging via a one-pass staging-only converter; left `cnr3_copy_scalar_buffer_to_native_plane` UNTOUCHED (Option A). Per-sample validation preserved. Value-preserving (P.8A+P.11B unchanged). MEASURED: total flat within noise (+0.16%, a CLEANUP); `stage_scalar_plane_to_native_bytes` 5,297->4,731; `copy_scalar_buffer_to_native_plane` off the U/V staging hot path; one fewer alloc/staged plane.
**POST-0B hot path (sets up Lever 3):** staging/repack side cleaned; the remaining ~50% is the UNPACK side — `cnr3_load_native_plane_sample` (~16,546 self) called per-sample inside `cnr3_copy_native_plane_to_scalar_buffer` (chroma + downsample input). That per-sample-call wall is exactly what Lever 3 (typed-row-pointer, inlined) eliminates and what unlocks AVX2. **NEXT: `/Qvec-report:2` evidence run (coordinator, at machine — prerequisite for Lever 3), then the Lever 3 scope.** No CMS/invariant change in any of the above. FI-10 (Future Investigations v7.15) remains the controlling finding. (Prior v4.17 note retained as history:) v4.17 (SLIMMED). Supersedes v4.16. Adds two post-W.3 developments, both committed in one snapshot: (1) the **TINY-100 diagnostic-cache scaffold** — a compile-time toggle `CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY` (shipped OFF) that selects a pre-computed small-but-safe cache profile so eviction fires on a ~200-frame live run instead of ~1300; it is the first concrete diagnostics-arc enablement step (the fixture the D-SUM telemetry will read). Proven three ways: toggle-OFF four-way **56/56** (was 55 + a new profile-agnostic protection-under-eviction test = +1), toggle-ON tiny selftest exit 0 with 13 visible skips + tiny protection test passing, and a designer-owned single-run live harness (`test_TINY_live_eviction_proof`) showing `profile=tiny-100` + cap_trigger + ckpt_trigger with detached>0. A `profile=%s` marker was added to the live KDT line (dev-trace-only). NO CMS design change (production constants unchanged, behind a compile guard); the selftest count moves 55->**56** for the added protection test. (2) A **profiling finding** banked as **FI-10** (see Future Investigations v7.15): a VS2026 profile of the NORMAL build on a sequential real-footage encode measured native<->scalar plane MARSHALLING at ~50% of per-frame cost, denoise math <10%, cache manager <3% — OPEN investigation only, candidate typed-row-pointer rewrite is a separate future arc. Also banked there: the earlier ~3 fps scare was the tiny-diag pruning cadence, NOT the cache architecture (normal build ~46 fps sequential). Build note: `cnr3.vcxproj` Release/Debug gained `DebugInformationFormat=ProgramDatabase` and Release gained `EnableCOMDATFolding=false` for profiling symbol resolution (codegen-neutral; does not affect shipped behaviour). (Prior v4.16 note retained as history:) v4.16 (SLIMMED). Supersedes v4.15. Brings the ledger current through the **W.3 CLOSURE**: the §7.5
combined live store-and-prune helper is implemented, four-way **55/55**, and the designer eviction-proof live A/B
harness PASSED — the live cache-pressure wiring arc **W.1→W.2→W.3 is COMPLETE**. Bumps controlling CMS to
**CMS07.15** (records the §7.5 store-status return contract surfaced by W.3; additive, no behaviour change). Sets the
post-W.3 sequence to **DIAGNOSTICS ARC NEXT** (coordinator decision: the D-SUM telemetry precedes the real-footage
campaign, because the live harness proves eviction SAFE but is blind to eviction-POLICY health — over-pruning,
thrash, hot-zone efficacy). This W.3-closeout pass advances the whole handover set in lockstep to the TARGET versions
in §1/§6 below; the repo is currently one bump behind on several (Role Handover v1.14, Reviewer Intro v3.7), so apply
the set as a BATCH. (Prior v4.15 note retained as history:) v4.15 (SLIMMED). Supersedes v4.14. (v4.14's TITLE was bumped from v4.12 but its BODY was
never refreshed past P.11C.5 — baseline, phase index, active phase, and doc versions all still read the
P.11C.5/CMS07.13 era, and the inner stamps stayed at v4.12. This v4.15 is the real catch-up: it brings the
ledger current through the Step 0 closure, the CMS07.14 bump, W.1, and W.2, and reconciles the title/inner/
end version stamps to agree.) This is the live per-phase ledger: a one-line committed-phase INDEX, the one
don't-re-derive technical finding kept in full, the ACTIVE/NEXT phase in full, and the open owed-items. Full
per-phase detail (golden chains, proof records, prior-phase briefs) lives in the `dev_cache_manager` branch
git history (prior DELTA versions v4.0-v4.14 + the test-artifact harnesses/derivation scripts) and Document
A's build-state note; it is committed to main at project end. Nothing durable is lost — it is migrated, not
truncated.
**Date:** 2026-06-30

---

## 1. CURRENT BASELINE (confirm from repo)

```text
Committed/pushed through:  CNR3-OPT-Lever0B-direct-scalar-native-staging  (marshalling arc: AVX2 build + Lever 0A luma passthrough + Lever 0B staging cleanup; on top of CMS07-DIAG-tinycache-scaffold)
Selftest count:            56/56 PASS  (forced-fail 55/56 exit 1; verbose 56/56)  [unchanged by AVX2/0A/0B — all value-preserving]
Build target:              x64-ONLY, /arch:AVX2 HARD REQUIREMENT (both cnr3.vcxproj + cnr3_cache_core_selftest.vcxproj, Release+Debug; Win32/x86 removed; .slnx x64-only). Documented in build_config header/README/release notes. Hard-faults on pre-AVX2 CPUs.
Marshalling arc status:    Lever 0A (luma passthrough) DONE -28% total; Lever 0B (staging cleanup) DONE flat-but-tidier. NEXT: /Qvec-report:2 evidence run, then Lever 3 (typed-row-pointer). Levers 1/2 optional per measured residual.
Tiny diag scaffold:        CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (build_config.h, shipped OFF/commented). ON: cache_profile=tiny-100, 13 visible skips, exit 0; live harness test_TINY_live_eviction_proof PASS.
Controlling CMS:           CMS07.15  (cnr3_cache_manager_design_v7_15.md)  [UNCHANGED by AVX2/0A/0B — implementation optimisation only, no design/invariant change]
Production Spec:           v2.15  (TARGET; repo v2.14 — currency-only refresh: CMS07.15 / 55-55 / diagnostics-next; no rule change)
Document A:                v3.11  (TARGET; repo v3.10 — build-state note advanced to W.3/55-55)
Document B:                v3.10  (TARGET; repo v3.9 — top UPDATE block advanced to W.3-done/diagnostics-next; authoritative current-state doc)
Role Handover:             v1.15  (TARGET; repo has v1.14 already — v1.14 added harness-ownership but did NOT advance state; v1.15 advances state to W.3-done)
Reviewer Intro:            v3.8  (TARGET; repo has v3.7 already — v3.7 added startup/harness guards; v3.8 advances state AND flips the stale 'footage-before-diagnostics' line)
Coder Restart Intro:       v6.6  (TARGET; repo v6.5 — next-action pointer advanced to diagnostics arc)
Future Investigations:     v7.15  (FI-10 ADDED — native<->scalar plane marshalling ~50% of per-frame cost, MEASURED, OPEN investigation only; filename re-aligned to CMS07.15. FI-01/02/03/05/06/07/08 still open for fmParallel)
Diagnostics spec:          v1.5  (unchanged — reviewed at diagnostics-arc kickoff)
MANIFEST:                  v3.9  (TARGET; repo v3.8 — re-enumerates the set to the versions above)
Branch:                    dev_cache_manager
```
The repository is the authority — confirm CNR3_EDIT_VERSION and the selftest count from committed source.

## 2. COMMITTED-PHASE INDEX (one line per phase; full detail in dev-branch git history)

The live getFrame dispatch is FEATURE-COMPLETE across all four branches; the branch-(d) recovery arc is
COMPLETE. Per-phase golden chains and proof records are in the dev-branch git history (prior DELTA
versions + test-artifacts) and reproducible from the `D*_golden_chain_derivation.py` scripts.

```text
K.1A-K.1D    request-plan structures + KDT trace -> first REAL output frame (copyFrame fresh-start)   [git]
K.1E.2/.3    live predecessor-present compute; CLOSES R-ARCH-06 (recursive filtered-pred, 163/93)      [git]
K.1F         live direct cached-output return (branch-b cache-hit; Option-C lifecycle trigger)         [git; §3 below]
K.1G         plugin source split (no behaviour change)                                                 [git]
Recovery-Step-0  AS4 single-lock batch discharge (discharge_all takes cache_mutex once; 48->49)        [git; CMS §8.7]
D.1          exact-anchor SINGLE-hole recovery (plugin-only) — DONE                                    [git; Doc A]
D.2          exact-anchor MULTI-hole (k>=2) + bounded-window refusal (plugin-only) — DONE              [git; Doc A]
             (RUN C bounded-window refusal is TRANSITIONAL — superseded by D.3 floor-fresh-start)
D.3          floor-fresh-start recovery (plugin-only) — DONE                                           [git; Doc A]
             (materialized-floor-is-the-foundation invariant recorded in CMS §9.5 / CMS07.13)
D.4          pre-compute adopt-skip + first-in-best-dressed PRIMITIVES (selftest; 49->51) — DONE       [git; Doc A]
D.5          recovery-pin-survives-real-prune-pass (selftest, paired control; 51->52) — DONE           [git; Doc A]
             >>> branch-(d) RECOVERY ARC COMPLETE (D.1-D.5). Only deferred confidence: real concurrent
             >>> (fmParallel) scheduling, bounded to the fmParallel validation phase.
P.11C.2      live scene-change config + scdthr->threshold helper + central store-request/checkpoint   [git]
             skeleton + not-applicable KDT (frame-0/floor). Detection NOT enabled (branch-c/d deferred
             to P.11C.3/.4/.5). Threshold uses verified vsCnr2 full-frame shape + P.2A-style proportional
             round-to-nearest depth scaling (NOT vsCnr2 <<(depth-8)). 52/52 unchanged. COMMITTED.
P.11C.3      branch-c (predecessor-present) scene detection ENABLED: live _with_scene_change call +    [git]
             force_checkpoint=scene_change_detected. Proven via external .vpy (live default scdthr=10.0,
             threshold=1402): C-control diff_total=64/samples=16/detected=0/blend (byte 109 between
             100..160); C-cut diff_total=2040/samples=2/detected=1/reset (byte ==current 160/80)/
             store_as_checkpoint=1. Four-way 52/52. frame-0 not_applicable boundary confirmed. COMMITTED.
P.11C.4      branch-d (RECOVERY) scene detection ENABLED: per-hole + target _with_scene_change +         [git]
             grid-OR-detected checkpoint store; floor store + floor anchor-record LEFT grid-only
             (classified-site patch, not pattern-replace). Holes report ACTUAL resulting_slot_is_checkpoint;
             target reports _expected (status-only wrapper); adopted-skip reports not_run. Atomicity
             inherited (one-lock store+flag+pin, D.5 composition). Four-way 52/52. Proven via recovery
             .vpy adapted from D3 floor-fresh-start trigger (threshold=5606, 16x16): D-control all
             detected=0 (256/64); D-cut-at-hole hole2 detected=1/6160/10/reset/checkpoint=1(actual),
             target no-cut blend byte 255/151/83 (degeneracy fix proven); D-cut-at-target target
             detected=1/6120/10/reset/checkpoint_expected=1, reset byte 255/160/80. Both Debug+Release.
             COMMITTED.
P.11C.5      scene-cut checkpoint FOUND AS RECOVERY ANCHOR (arc finale). KDT-only: print            [git]
             anchor_is_checkpoint=%d in the live exact-anchor recovery trace (from already-populated
             recovery_plan.anchor_is_checkpoint); trace-only, no behavioural change. New cache-core
             composition selftest (52->53) proves the CACHE HALF: an UNPINNED NON-GRID frame stored as
             the sole checkpoint (frame 165) survives a real bounded prune by CHECKPOINT CLASS while an
             ordinary non-checkpoint (frame 1) is evicted (total_pin_count=0 throughout -> survival by
             class, NOT pin), then read-only bounded recovery for frame 167 anchors exactly on 165 with
             anchor_is_checkpoint=true and holes={166} (166 absent by construction). Composes with
             P.11C.4 (live scene detection feeds the checkpoint store route). Four-way 53/53 / 53/53 /
             52/53 forced-fail / 53/53, both Debug+Release. COMMITTED. >>> P.11C SCENE-CHANGE ARC CLOSED.
Step-0       JOINT CMS SENSIBILITY/GAP REVIEW for hot-zone + prune LIVE WIRING — CLOSED (review only,    [register]
             no code). 13 findings AGREED/RESOLVED (x_CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md). Key
             rulings: SR-C-04=(B) add an INDEPENDENT checkpoint-flag-count retention trigger (CMS §6.3 prose
             unchanged); SR-D-01 prune trigger in a NEW combined locked helper (not in store_owned_frame_locked);
             SR-D-02 / SR-C-02 lifecycle split — AS2 consumer inputs PIN-protected, target/frame-0 outputs
             HOT-ZONE-protected (NOT pinned); SR-D-07 six-step combined-helper order ratified; SR-C-05 retirement
             lazy-in-prune; SR-C-06 temporary KDT folds into D-SUM-11. ARCHITECTURE CORRECTION: there is NO
             checkpoint pool — one unified slots_ vector; checkpoint is an is_checkpoint FLAG on a slot.
CMS07.14     CMS bump out of Step 0 (cnr3_cache_manager_design_v7_14.1.md). Adds §7.4 (independent            [doc]
             checkpoint-retention trigger), §7.5 (six-step combined live store-and-prune wiring contract),
             §7.6 (arInitial hot-zone observation prerequisite). Now the controlling design authority.
W.1          INDEPENDENT CHECKPOINT-RETENTION TRIGGER (cache-core; §7.4). Adds the checkpoint-flag-count      [git]
             retention trigger so a checkpoint-heavy cache prunes by checkpoint class even when total slots
             stay under the capacity trigger (cut-heavy content: flagged count can exceed CHECKPOINT_MAX_RETAIN=48
             while slots stay under the capacity trigger of 165). Selftest 53->54 (w1_checkpoint_retention_trigger).
             Four-way 54/54 / 54/54 / 53/54 forced-fail / 54/54. COMMITTED CMS07-W.1-checkpoint-retention-trigger.
W.2          HOT-ZONE OBSERVATION AT arInitial (DLL live wiring; §7.6). One common                            [git]
             record_hot_zone_observation(n) at the top of cnr3_arInitial (after alloc null-check, before the
             cache-hit lookup) — one pre-dispatch point all four branches + the refusal path pass through;
             observation needs only n (SR-D-03). OBSERVE-ONLY: nothing consumes hot-zone state until W.3.
             Temporary HOT-ZONE-OBSERVED KDT (SR-C-06, status via cnr3_status_name). Selftest count UNCHANGED
             at 54 (cnr3_arInitial.cpp is DLL-project-only). A/B harness (test_W2_hot_zone_observation_AB.*):
             HOT-ZONE-OBSERVED status=ok on all four branches (frame-0, predecessor-present, cache-hit,
             recovery floor-fresh-start), byte-identical to the pre-W.2 build (observe-only confirmed), and
             180 observations / 180 activations = exactly one per activation. COMMITTED
             CMS07-W.2-hot-zone-observation-arInitial.
             W.3          COMBINED LIVE STORE-AND-PRUNE HELPER (§7.5; eviction goes LIVE; first live consumer of W.2).   [git]
             Wires §7.2 capacity + §7.4 checkpoint-retention triggers into the live arAllFramesReady path via
             ONE combined locked helper running the SR-D-07 six-step order (store/adopt -> set is_checkpoint
             -> pin-if-AS2-consumer -> retire stale zones -> prune decide/detach -> unlock+free); brings hot-zone
             RETIREMENT (lazy, in the prune pass). By-value store wrappers (by-ref impl, nullable pin_list*);
             wrapper-level summary init; public hole guard reproduced before lock; one-hold/no-gap (detached
             victims freed after unlock). AS2 store NORMALIZES duplicate->ok and reports via
             duplicate_existing_slot (store-status return contract recorded CMS07.15 §7.5). Selftest 54->55
             (discriminating aggregate). Four-way 55/55 / 55/55 / 54/55 forced-fail / 55/55. Designer
             eviction-proof live A/B harness PASS: byte-identical under eviction; cap+ckpt triggers fired AND
             detached victims (non-vacuous); recovery + AS2 floor/hole exercised live. COMMITTED
             CMS07-W.3-combined-live-store-prune-helper.
             >>> LIVE CACHE-PRESSURE WIRING ARC COMPLETE (W.1 + W.2 + W.3). NEXT ARC: DIAGNOSTICS (D-SUM).
```

## 3. K.1F + R-LIFECYCLE (the API4 lifecycle resolution — important, don't re-derive)

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

## 4. ACTIVE / NEXT PHASE — W.1 + W.2 + W.3 DONE (live cache-pressure wiring arc COMPLETE); NEXT = DIAGNOSTICS ARC

>>> LIVE CACHE-PRESSURE WIRING ARC. After P.11C closed (scene detection wired uniformly across branch-a/c/d,
feature-complete getFrame), the Step 0 joint CMS review (CLOSED, 13 findings) established the wiring plan and
bumped the CMS to 07.14. The arc is W.1 -> W.2 -> W.3:

```text
W.1  DONE/committed  §7.4 independent checkpoint-retention trigger (cache-core). Four-way 54/54.
W.2  DONE/committed  §7.6 hot-zone observation at arInitial (DLL). One pre-dispatch call, observe-only,
                     byte-neutral, HOT-ZONE-OBSERVED KDT proven on all four branches. 54/54 (count unchanged).
W.3  DONE/committed  §7.5 COMBINED LIVE STORE-AND-PRUNE HELPER (four-way 55/55 + eviction-proof live A/B harness PASS). Where eviction goes LIVE and the
                     W.2 observation finally gets a consumer. Wires the §7.2 capacity + §7.4 checkpoint-
                     retention triggers into the live arAllFramesReady path and executes the SR-D-07 six-step
                     locked order (store/adopt -> set checkpoint flag -> pin-if-AS2-consumer -> retire stale
                     zones -> prune decide/detach -> unlock+free), and brings in hot-zone RETIREMENT (SR-C-05,
                     lazy in the prune pass) — the thing W.2 deliberately excluded. RELIES on W.2: produced
                     output[N] is prune-safe only because W.2 observed N into its own active hot zone.
```

W.3 is the HIGHEST-CONSEQUENCE phase in the arc — the first that can evict a frame the pipeline still needs —
so its scope reads the FULL arAllFramesReady store path + the cache-core prune/retire helpers before any
patch (not the spot-reads that sufficed for observe-only W.2), and it warrants the prior-designer parallel
cross-check (their standing offer was specifically for W.3). THIS WAS DONE: the W3OD + W3C three-way design
review closed at r8 FINAL (16 findings, all closed); the coder build scope converged over 8 source-verified
revisions (v0.1→v0.8); the delivered patch was reviewed against source and is faithful; four-way 55/55; and the
designer eviction-proof live A/B harness PASSED (byte-identical output under live eviction, triggers proven to
fire and detach victims, recovery + AS2 floor/hole exercised live). The one unknown-unknown surfaced — the AS2
duplicate-status normalization asymmetry — is resolved into CMS07.15 §7.5.

SEQUENCE AFTER W.3 (coordinator decision, 2026-06-30): **DIAGNOSTICS ARC NEXT** -> first REAL-FOOTAGE
validation (the 576p50 campaign) -> fmParallel arc (the end goal). This REVERSES the earlier footage-before-
diagnostics ordering, and deliberately so: the W.3 eviction-proof harness proves the live cache evicts
*safely* (no returned frame corrupted) but is BLIND to eviction-*policy* health — it cannot see over-pruning,
prune thrash, whether hot zones protect the right slots, or recovery-storm churn (a byte-identical pass is
consistent with both a healthy and a thrashing cache). The D-SUM telemetry (esp. D-SUM-10 prune safety,
D-SUM-11 hot-zone, D-SUM-12 recovery/hole-fill) is the instrument that turns a real-footage run from
"did not crash" into a measurable policy verdict — so diagnostics land FIRST and the footage campaign runs
instrumented. Diagnostics design is settled (cnr3_diagnostics_specification_v1_5.md + memory spec v2;
condensed 4-phase plan v1.3 DIAG.1-.4); the diag source files are SHELLS — what is owed is IMPLEMENTATION,
observe-only (R-PROCESS-19). The doc-set refresh / Document B deep tidy is THIS pass (the W.3-closed seam).
NEXT ACTION: kick off the diagnostics arc — first step is the Claude-owed 2-line-per-family menu so the
coordinator picks the core D-SUM subset (provenance doc §4.3), then DIAG.1 (framework + one reference family
+ the observe-only macro-off proof). The diagnostics provenance/plan docs need their stale state headers
refreshed to sit AFTER this arc before kickoff; their design bodies are durable and survive intact.

--- historical detail of the now-closed P.11C arc follows ---

P.11C wires scene-change handling uniformly across branch-a/c/d before the first REAL-footage test.
Scene-change is currently DEFERRED uniformly (scene_change_deferred=1) because the D-series proofs run
on synthetic constant-plane test footage that has NO cuts. Real footage has cuts, so P.11C must wire
detection in before real video: a deferred P.11C would (a) blend chroma across a cut (visibly wrong),
and (b) fail to set the cut-checkpoint recovery relies on. **A detected cut PROMOTES to a checkpoint
(CMS §6.4 / §9.5) = an exact, longer-retained recovery anchor found naturally by the descending search**
— so P.11C INTERLOCKS with the recovery machinery just completed. Sequence: P.11C uniform wiring ->
first real-footage validation. P.11C touches the pixel pipeline AND the checkpoint/recovery interaction;
it is its own phase, not part of any D-phase. NEXT ACTION: designer-owed P.11C scope.

## 5. OWED-ITEMS LEDGER (W.3 CLOSED; next arc = diagnostics)

**v4.16 STATUS UPDATE (supersedes the W.3-pending items below; older text retained per the layered-update
convention).** W.3 (§7.5 combined live store-and-prune helper) DONE/committed — four-way 55/55 + eviction-proof
live A/B harness PASS; the live cache-pressure wiring arc W.1→W.2→W.3 is COMPLETE. `execute_bounded_prune_pass`
and `retire_decay_eligible_hot_zones` are now WIRED into the live path. The AS2 production-vs-pinned store-status
RETURN contract surfaced by W.3 is RESOLVED into CMS07.15 §7.5 (additive, no behaviour change). REMAINING OWED,
now carried into the DIAGNOSTICS ARC (next): the 14-family D-SUM telemetry (condensed 4-phase plan v1.3); the
end-of-run integrity report; the `abort_on_error` parameter; the warn-vs-hard-fail severity policy; and the
eviction-POLICY-health question the live harness cannot answer (over-prune / thrash / hot-zone efficacy /
recovery churn -> D-SUM-10/11/12). The K.1E3 trace-only scaffold + `:1573` stale-comment cleanup folds into the
diagnostics arc too. DOC-SET: this W.3-closeout pass advances CMS->v7.15, DELTA->v4.16, Production Spec->v2.15,
Document A->v3.11, Document B->v3.10, Role Handover->v1.15, Reviewer Intro->v3.8, Coder Intro->v6.6, Future
Investigations->v7.14, MANIFEST->v3.9 (apply as a batch).

**v4.15 STATUS UPDATE (supersedes the resolved items below; older text retained as history of record, per
the project's layered-update convention).**
- **STEP 0 joint CMS review: CLOSED.** 13 findings AGREED/RESOLVED (`x_CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md`;
  process in `x_CNR3_Step0_Joint_Review_PROCESS_v1_1.md`). The "SPEC RELIABILITY / prune-trigger contract" item
  and the ">>> STEP 0 (banked decision)" sub-item below are DONE. Ruling SR-C-04=(B). No checkpoint pool —
  unified `slots_`, `is_checkpoint` flag.
- **CMS07.14: DONE** — the bump out of Step 0 (§7.4 trigger, §7.5 six-step combined store-and-prune contract,
  §7.6 arInitial observation prerequisite). Now controlling.
- **LIVE CACHE-PRESSURE WIRING: W.1 (§7.4 trigger) + W.2 (§7.6 observation) DONE/committed.** The audit item
  below ("the last missing FUNCTIONALITY") now reduces to **W.3** — the §7.5 combined live store-and-prune
  helper (live prune-trigger + retirement) — as the remaining live wiring. record_hot_zone_observation is now
  WIRED (W.2); execute_bounded_prune_pass / retire_decay_eligible_hot_zones remain unwired until W.3.
- **DOC-SET REGENERATION (Document A / Document B): DONE.** Document A regenerated v3.4 -> **v3.10**; Document
  B at **v3.9**. The "DOC-SET REGENERATION — Document A and Document B" item below is closed.
- **TEST ARTIFACTS: add `test_W2_hot_zone_observation_AB.vpy/.bat`** to the regression-base list below — the
  W.2 A/B four-branch observation + byte-neutrality harness (one `[0,50,2000]` scenario under `-r 1`; reuses
  the K.1F `SetVideoCache(mode=0)` core-cache-defeat lesson; greps HOT-ZONE-OBSERVED per branch).
- **NEW — HOUSEKEEPING (banked, NOT actioned): stale K.1E3 refusal scaffold + comment, superseded by live
  branch dispatch.** `build_config.h:76` defines `SCAFFOLD_CMS07_K1E3_REFUSE_AFTER_FRAME2_BEFORE_RECOVERY=1`,
  but its only active effect is the trace function `cnr3_trace_live_after_frame2_not_yet_implemented` (a print,
  NOT a behavioural refusal): the live `cnr3_arAllFramesReady` dispatch (~:1597) switches on
  `request_data->branch` with recovery a live case (`cnr3_complete_live_recovery`), so N>2 reaches recovery and
  the scaffold path is not taken. The `arAllFramesReady.cpp:1573` "frames after 2 are refused until recovery
  wiring" comment is STALE NARRATIVE from the K.1E.3 era. SAFE — no current-behaviour impact, **LIVE-RATIFIED by
  W.2's N=2000 floor-fresh-start recovery firing through `branch=RECOVER floor=1950`**. ACTION (future
  cleanup/diagnostics arc, NOT now): prune the trace-only scaffold + fix the `:1573` comment so a future chat
  does not re-discover it as a scare. Recorded so it is neither lost nor re-litigated.
- **NEW — DIAGNOSTICS-ARC ENABLEMENT DONE: TINY-100 cache scaffold COMMITTED.** `CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY`
  (build_config.h, shipped OFF) selects a pre-computed small-but-safe cache profile (ceiling 100, back-radius 15,
  interval 3, 2 hot zones, etc. — all 9 independent knobs wrapped per-constant; derived constants + static_asserts
  untouched so the tiny profile re-proves the same safety chain at compile time). It makes capacity + checkpoint
  eviction fire on a ~200-frame live run instead of ~1300, giving the diagnostics arc a short-run fixture. Also
  added: `CNR3_CACHE_PROFILE_NAME` marker (selftest heading + live KDT `profile=%s` token, dev-trace-only); a
  profile-AGNOSTIC protection-under-eviction selftest (derives trigger from constants, proves pinned + hot-zone
  frames survive a real prune that detaches another slot — runs in BOTH profiles, +1 to the count). 13 production-
  GEOMETRY selftests are visibly skip-passed under the toggle (their hardcoded frame/zone/distance expectations are
  production-tuned; the normal build proves them at 56/56). The single-run designer harness `test_TINY_live_eviction_proof`
  (.vpy + .bat, golden 576p50, selftest-precondition gate then live run) is the regression artifact — add to the base list.
  This is the FIRST concrete diagnostics-arc enablement step: the fixture the D-SUM telemetry will read.
- **NEW — PROFILING FINDING BANKED (FI-10, investigation only, NOT actioned): native<->scalar plane marshalling
  ~50% of per-frame cost.** VS2026 profile of the NORMAL build on a sequential real-footage encode (576p25, -r 1)
  showed `cnr3_load_native_plane_sample` / `cnr3_copy_native_plane_to_scalar_buffer` / `cnr3_stage_scalar_plane_to_native_bytes`
  (the std::vector<int> unpack + repack) at ~50% of per-frame time, denoise math <10%, whole cache manager <3%.
  Stable across 200/3500 frames and cache-on/off (sequential access has no reuse; this IS the real encode cost).
  SUB-FINDING (multi-thread run, same sequential clip WITHOUT -r 1): the VS thread pool issues requests OUT OF
  ORDER, so ~half the frames hit `cnr3_complete_live_recovery` (~52%) even on sequential footage — recovery is
  cheap in logic (<1%) but RE-RUNS the ~50% marshalling to rebuild each missing predecessor, so real parallel
  cost is (1 + recovery_rate) x the per-frame marshalling. Whether the ~50% recovery rate is inherent or a tunable
  cache-retention/reorder-window mismatch is a DIAGNOSTICS-ARC question (recovery churn = D-SUM-12). Candidate
  levers recorded in FI-10 easiest->hardest: (1) per-ACTIVATION buffer reuse (removes allocation churn; must be
  per-getFrame-call scratch, NOT per-instance/global, or it serializes fmParallel + the two field-stream instances
  — coordinator-raised constraint); (2) fuse unpack/process/repack passes; (3) full typed-row-pointer in-place
  rewrite (~1.5-2x, the P.11B deferred optimization). All a SEPARATE R-PROCESS-21 arc touching proven P.7A-P.11B
  code — scoped only if/when opened. Recorded in full in Future Investigations v7.15 as FI-10. Also banked there:
  the ~3 fps concern was the TINY-100 pruning cadence, NOT the cache architecture (normal build ~46 fps sequential).
  Build-config note: `cnr3.vcxproj` gained `DebugInformationFormat=ProgramDatabase` (Debug+Release) and
  `EnableCOMDATFolding=false` (Release) for profiling symbol resolution — codegen-neutral, does not change shipped behaviour.
- **NEW — SELFTEST-ROBUSTNESS GOTCHA (diagnostics-arc owed): `keystone_request_plan_dev_trace_proof` is
  CONDITIONAL on the `CNR3_KEYSTONE_DEV_TRACE` macro.** With dev-trace commented out (as done for the profiling
  runs), that one test returns `lifecycle_violation` and the suite reports 54/56 FAIL — NOT a regression, purely
  the test needing dev-trace compiled in. This nearly masqueraded as an AVX2 regression during the /arch:AVX2
  bring-up (all P-series MATH tests passed, which is what proves AVX2 neutral; only the dev-trace test failed).
  So "56/56" is currently CONDITIONAL on dev-trace being ON — not a fixed target. IMMEDIATE discipline: run all
  proofs with `CNR3_KEYSTONE_DEV_TRACE` ON. PROPER FIX (diagnostics arc, R-PROCESS-21): make the test SKIP-PASS
  with a visible "SKIPPED (dev-trace not compiled in)" line when the macro is off (same pattern as the tiny-profile
  geometry skips), so the gate is robust to build flags and cannot emit a misleading failure from a diagnostic
  toggle. Recorded so the next proof does not re-diagnose it from scratch.

--- owed-items below are retained as history of record; resolved ones are marked DONE in the block above ---

- **CANDIDATE (not actioned) — production helper for adopt-or-compute outcome plumbing.** Surfaced
  during D.4 scoping: the floor and hole live paths duplicate the adopt-or-compute DECISION choreography
  inline, though the ownership-critical PRIMITIVES are already shared (lookup_frame_and_record_pin;
  AS2 store summary duplicate_existing_slot). A helper consolidating the decision is a POSSIBLE future
  cleanup — to be decided ON ITS OWN MERITS (R-PROCESS-21: proven-code change needs proposal + approval
  + selftest), justified by single-source-of-truth / fmParallel-readiness, NOT by testability, and only
  if the floor/hole logic drifts or fmParallel review shows a need. The floor/hole compute/store bodies
  differ enough (fresh-start vs predecessor-blend; generic store vs planned-hole wrapper; scalar vs
  indexed outcome) that a premature helper risks a parameter-heavy abstraction obscuring the D.3 safety
  story. NOT actioned now; recorded so it is neither lost nor rushed.

- **CANDIDATE (not actioned) — Document B deep tidy (option 2), timed BEFORE the fmParallel arc.** Document
  B v3.5.1 currently uses the layered convention: the v3.5.1 UPDATE block at the top is the authoritative
  current state (D.5 / recovery complete / P.11C next), and the older body (restart-era §5 next-phase=H.2A,
  the .txt-rename transition state, the §11 CMS07.2/28-selftest snapshot, the pixel-arc §8 work plan) is
  retained under explicit SUPERSEDED banners as history of record. This is correct and complete for
  handover (option 1, chosen). The deeper tidy (option 2) — promote the durable scaffolding (working
  method, invariant disciplines, do-not-implement) into clean current sections and move the genuinely-dead
  restart-era instances into a single consolidated "Historical / superseded restart-era record" appendix,
  so live state and archive are cleanly separated rather than banner-interleaved — is DEFERRED to the
  P.11C -> fmParallel seam. Rationale: the fmParallel arc is a major new concern (the deferred concurrent-
  scheduling confidence from D.4/D.5) and deserves a clean current-state Document B to start from; the
  recovery+pixel era closes at P.11C/first-real-footage, which is the natural boundary for the restructure.
  Do it as its own focused pass (like this handover refresh), NOT mid-handover and NOT during fmParallel.

- **FULL DOC-SET REFRESH — timed at the P.11C-closed / FIRST-REAL-FOOTAGE-PROVEN seam (coordinator: "must
  happen soon").** When P.11C.5 commits and first real-footage validation passes, do ONE focused
  documentation pass that: (a) refreshes the whole handover set (CMS, Production Spec, Documents A/B,
  DELTA, Reviewer/Role/Coder-Restart intros, Future Investigations) to the "P.11C arc CLOSED + real footage
  PROVEN" state; (b) folds in the FORWARD PLAN for the diagnostics arc -- the condensed 4-phase plan in
  CNR3_Diagnostics_Arc_Condensed_Plan_v1_0.txt (DIAG.1 framework+D-SUM-01+observe-only proof; DIAG.2
  cache-core family batch; DIAG.3 getFrame/recovery/return batch; DIAG.4 memory + close) so the next arc
  has a recorded entry point; (c) executes the Document B deep tidy (option 2, above) as part of the same
  pass since it shares this exact seam. This is the natural boundary: recovery+pixel+scene era closes, the
  diagnostics + fmParallel eras begin from a clean current-state doc set. Do it as its own focused pass,
  NOT mid-arc. NOTE: diagnostics could optionally run PARTLY before real footage (D-SUM-01/03/14 aid
  first-footage debug; observe-only so it blocks nothing) -- placement is a coordinator decision at the seam.

- **LIVE CACHE-PRESSURE WIRING — the last missing FUNCTIONALITY (verified against the P.11C.5 src.zip,
  this session).** AUDIT FINDING (ground truth, not reconstruction): in the committed P.11C.5 live getFrame
  path (arInitial.cpp + arAllFramesReady.cpp), the cache-pressure capabilities have ZERO live callers:
  execute_bounded_prune_pass=0, record_hot_zone_observation=0, retire_decay_eligible_hot_zones=0,
  merge_closest_active_hot_zones=0, remove_unpinned_noncheckpoint_frames_bounded=0,
  calculate_cache_prune_trigger_decision=0. store_owned_frame_locked APPENDS without consulting the prune
  trigger (grows unbounded; only returns capacity_exceeded at the vector hard max). So the live cache
  currently NEVER prunes and NEVER records hot-zone observations. The LOGIC is fully built + proven
  (selftests: prune hysteresis/victim/composite, D.5, P.11C.5; hot-zone lifecycle tests) -- but the WIRING
  into the live path is the last functional gap. Everything else audited is wired or test/diag-only by
  design (lookup/store/recovery/pin-discharge all WIRED; total_pin_count/hot_zone_count/slot_count are
  diagnostic observers; non-pinning plan_bounded_recovery_search is the selftest variant). NOTHING ELSE
  functional appears missing. Coordinator lean (this session): wire hot zones THEN pruning, THEN real-clip
  runs (option B), on the basis the prune componentry is already proven.

- **SPEC RELIABILITY FOR THE WIRING — policy reliable, LIVE-TRIGGER CONTRACT needs a designer+coder review
  pass BEFORE coding (this session).** Assessment of cnr3_cache_manager_design_v7.x (CMS) for the wiring task:
  RELIABLE AS-IS: hot-zone OBSERVATION wiring point is specified -- CMS §5.7 "Hot-zone update at arInitial,
  not arAllFramesReady"; hot-zone lifecycle §5.3-5.6 (slide/spawn/merge, decay sequence, exact-cheap
  retirement test); prune RETENTION policy §6.3 (candidate iff frame!=0 AND pin_count==0 AND outside every
  hot zone; evict greatest-hot-zone-distance first; soft MIN/MAX_RETAIN; frame 0 never pruned) -- which is
  exactly what execute_bounded_prune_pass already implements; prune SAFETY (§5.5 decay-makes-prune-safe;
  store-and-pin one atomic so a gap cannot let prune evict the just-stored frame). NOT YET PINNED DOWN AT
  WIRING LEVEL: the live PRUNE-TRIGGER TIMING -- exactly WHEN the live store path invokes
  execute_bounded_prune_pass (after each over-ceiling store? batched? at request classification?) and how
  that composes safely with the active pin_list and the arInitial->arAllFramesReady gap. The CMS describes
  prune firing "by capacity pressure / count-based soft trigger" but does not nail the live call-site at
  implementation level. Much of the policy is written FOR fmParallel ("once multiple requests are in flight",
  "under fmParallel scatter") -- so the SINGLE-ACTIVATION regime now is SIMPLER than the eventual concurrent
  case. RECOMMENDED FIRST STEP (when resumed): a focused designer+coder review (CMS-clarification + approach
  analysis, like the P.11C.5 read-first) on the live prune-trigger contract -- confirm trigger point, confirm
  single-activation safety, and EXPLICITLY scope it as "single-activation wiring now; concurrent prune
  revisited in the fmParallel arc." Hot-zone observation wiring needs less review (§5.7 already specifies it).
  Sequence under coordinator's option B: (review prune-trigger contract) -> hot-zone observation wiring ->
  prune wiring -> real-clip runs -> diagnostics -> fmParallel. "Proven componentry" != "proven wiring": the
  prune PASS is proven; the live TRIGGER and its lifecycle safety are the actual wiring work (same
  component-vs-wiring distinction as the K-phases).

  >>> STEP 0 (banked decision; absorbs the coder handover-review enhancement, this session): the immediate
  next action is broadened from "review the prune-trigger contract" to a **joint CMS SENSIBILITY / GAP
  REVIEW for hot-zone + prune live wiring, BEFORE any wiring patch**. Do NOT assume the CMS is reliable
  as-is merely because the componentry is proven: first review whether the CMS is still sensible and
  complete against the post-P.11C.5 implementation state. The prune-trigger contract (above) is the
  load-bearing PART of that review, not the whole of it. Provisional sequence, subject to the review
  outcome: **Step 0 CMS sensibility/gap review -> (if confirmed) hot-zone observation/retirement wiring ->
  live prune-trigger wiring -> real-clip validation (+ diagnostics/telemetry placement in the approved
  order) -> fmParallel.** A CMS clarification or version bump MAY come out of Step 0; if so it is a
  legitimate output of the review, not a precondition skipped. This is the consistent next-action recorded
  across the refreshed pack (Production Spec, Document B, Coder Restart Intro, Role Handover, Reviewer Intro).

- **TEST ARTIFACTS / GOLDEN PROVENANCE (housekeeping; add to repo alongside the existing harnesses).**
  The recovery harnesses and their golden-derivation scripts should live in the repo test area as
  regression bases for later phases (D.3+ must keep D.1 and D.2 green):
    * test_D1_once_only_harness_AB.vpy/.bat (D.1 single-hole), test_D2_once_only_harness_AB.vpy/.bat
      (D.2 multi-hole + bounded-window refusal), and test_D3_once_only_harness_AB.vpy/.bat (D.3
      floor-fresh-start RUN A + floor-byte/hole cache-hit RUN B + D.2 exact-anchor regression RUN C +
      D.1 regression RUN D + negative control RUN E + passthrough RUN G). All reuse
      test_K1F_check_y4m_constant_plane.py.
    * D.4 and D.5 have NO separate harness or golden script: their proofs are cache-core selftest cases
      shipping in the selftest target itself. D.4 = two cases (present-frame adopt-skip primitive +
      first-in-best-dressed duplicate; 49->51). D.5 = one case (recovery-pin-survives-bounded-prune,
      paired control/protected; 51->52). Status/ownership/identity proofs, not pixels.
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
- **CLIP-TEST HARNESS depends on the diagnostics (confirmed 2026-06-27).** The real-footage clip-test
  harness (e.g. test_000_Example_576p50.vpy/.bat — runs 576p50 through the live plugin to NUL/encode) is
  of little verification value WITHOUT the D-SUM end-of-run summaries + selectively-gated concise per-frame
  telemetry in place: a bare run only shows it did not crash, not whether pin balance held, recovery fired
  correctly, integrity stayed clean, or scene-change/checkpoint-promotion behaved across thousands of real
  frames. Therefore the large clip-test CAMPAIGN is sequenced AFTER the diagnostics arc. P.11C's proof does
  NOT use this harness — P.11C is proven on SYNTHETIC footage with constructed cuts (KDT-observable: cut ->
  reset-used -> output[K] stored WITH checkpoint flag -> a later recovery finds it as an anchor), exactly as
  D.1-D.5 were proven on synthetic footage; the REAL-FOOTAGE validation of P.11C folds into the later
  campaign once diagnostics exist.

- **DIAGNOSTICS ARC — sequenced AFTER P.11C, BEFORE the real-footage campaign (decided 2026-06-27).**
  The D-SUM diagnostics framework is fully DESIGNED (cnr3_diagnostics_specification_v1_5.md: §2.3 the
  per-summary COMPUTE/PRINT compile-time gate pattern with the paired #error "cannot print without
  compute" cross-check, print-subordinate-to-compute; §4 the 14-family catalogue D-SUM-01..14; R-PROCESS-19
  compute-disabled observe-only proof obligation; the FAIL/WARN/INFO severity model) and
  cnr3_memory_diagnostics_spec_v2.md (D-SUM-02). What is OWED is the IMPLEMENTATION: the src.zip shells
  carry only the D-SUM-11 hot-zone COUNTER MODEL (Cnr3CacheHotZoneDiagnosticStats + observers) and NO
  end-of-run formatting/printing for any family; memory-diag is an explicit placeholder.
  SEQUENCE (big-picture aligned): (1) P.11C scene-change calc FIRST (synthetic-proven, like D.1-D.5;
  its real-footage validation folds into the later campaign). (2) THEN the diagnostics arc — implement the
  D-SUM families with their compute/print gates + per-family R-PROCESS-19 observe-only proofs, including
  D-SUM-14 (scene-change/recursive-reset/checkpoint-promotion telemetry — NOT bundled into P.11C) and
  D-SUM-02 (memory, via salvage from the archived deprecated memory-diag code in GitHub). The diagnostics
  give BOTH the verifiable real-run telemetry (ownership/pin-balance D-SUM-04, integrity D-SUM-05, prune
  D-SUM-10, recovery D-SUM-12) AND the selectively-gated concise per-frame watch telemetry (hot-zone D-SUM-11,
  prune, hole-filling) needed to observe behaviour under fmParallel. (3) THEN first verifiable real-footage
  run + the large 576p50 campaign. (4) THEN the fmParallel arc.
  FIRST STEP of the diagnostics arc (Claude-owed): produce a concise 2-line summary of EACH of the 14
  D-SUM families (purpose + what it gates/observes) so the coordinator can choose the core implementation
  subset vs deferred. The end-of-run integrity report + abort_on_error (default False) + warn-vs-hard-fail
  severity policy (below) are PART OF this diagnostics arc, not separate.
  CODER PREP (recorded; action at diagnostics-arc kickoff, NOT during P.11C): the diagnostics-arc coder
  package includes cnr3_diagnostics_specification_v1_5.md + cnr3_memory_diagnostics_spec_v2.md + a pointer
  to the ARCHIVED deprecated memory-diag code, framed as ORIENTATION + SALVAGE REFERENCE — the memory-diag
  is mostly salvageable but is DEPRECATED (predates CMS07 / the D-SUM gate framework / R-PROCESS-19), so it
  is adapt-to-the-current-gate-pattern-and-prove-observe-only, NOT paste-in, and per R-PROCESS-08 /
  R-ARCH-05/07 the coder reads early but does NOT implement until the diagnostics phase is scoped+approved.

- longer sequential run beyond N==2; end-of-run integrity report + abort_on_error (default False)
  + warn-vs-hard-fail severity policy.
- full CMS fmParallel implications review (the CMS07.9 skim was non-exhaustive; FI-05 two-instance
  resource model likely genuine gap).
- fmParallel-phase (companion FI register): FI-05/06/07/08; operational two-instance diagnostics;
  test-tunable hot-zone/prune thresholds; cache-hit fast-path (Option A/B) revisit only if the one
  trigger-fetch on cache-hit is ever measured as significant AND confirmed from quotable core source.

---

## 6. DOCUMENT SET (current versions — new-chat reading order)

```text
CMS (design authority)     cnr3_cache_manager_design_v7_15.md             (CMS07.15)
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_15.md     (§3.2 context master; §3A register + charter §3A.5.0)
Document A                Document_A_CNR3_Project_Context_and_Standing_Rules_v3_11.md   (context + standing rules)
Document B                Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_10.md  (current-state; top UPDATE block authoritative)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md          (subordinate)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v1_15.md
Reviewer Intro            CNR3_Handover_Introduction_to_new_reviewer_chat_v3_8.md
Coder Restart Intro       CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_6.md
Future Investigations     CNR3_CMS_Future_Investigations_and_Open_Questions_v7_14.md    (companion; no FI item changed by W.3)
Current-state (this)      THIS slimmed DELTA (live per-phase ledger)
```
**Authority:** CMS -> Production Spec §3A -> diagnostics -> handover pack. Repository wins over any
document on build state. **Reading order:** Reviewer/Coder intro -> CMS07.15 -> Document A v3.11 ->
Document B v3.10 (top block) -> this DELTA.
*(Versions confirmed against the FINAL_DOCS set this session; confirm CNR3_EDIT_VERSION + selftest count from repo.)*

— End of CNR3 THIS-CHAT DELTA (slimmed), v4.16.
