# CNR3 — THIS-CHAT DELTA: current-state companion (SLIMMED; analysis/instrumentation sub-arc — four patches committed, selector held, marker regression open) — v5.0

**Version:** v6.0 (2026-07-15). Supersedes v5.0. Five arcs committed since: the fmParallel diagnosis, the
plan-retry mitigation, the HALF-500 profile, the chroma-desaturation FIX, and the OPTION PARSER. The plugin
now has a real user-facing surface and an evidence-decided ship configuration. Committed marker at this
DELTA: **CMS07-FEATURE.cnr2-descriptive-option-parser**. Canonical 4-way **57/57** (config-dependent).
CMS UNCHANGED at 07.15 throughout.

*** v6.0 DELTA (2026-07-15) — what changed, in order ***

### THE SHIP DECISION (new — this is now a project fact)
`fmParallelRequests` + `CNR3_CACHE_PROFILE_HALF` + `CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**.
Measured on the 3000-frame clip, HALF profile, all modes:

```text
mode                 fps    frames_computed   duplicates   verdict
fmUnordered           95           3000            0        clean but slowest
fmParallelRequests   337           3001            1        CLEAN AND FASTEST -> SHIP
fmParallel           126           5342         2342        78% overcompute for 2% speed over Requests
```
End-to-end encode on the ship config: 3000 frames, **fps=274, speed=5.47x realtime**, x264 CRF18, filter not
the bottleneck (282 fps filter-side under encode back-pressure). W3X ships a built DLL via PyPI; ~99.999% of
users never rebuild, so these compile-time gates ARE the product configuration.

### 1. fmParallel: DIAGNOSED, not fixed
Predecessor-in-flight redundant-recompute race. Correctness ALWAYS holds (x=0). Waste scales with in-flight
depth. fmUnordered = 0 waste; fmParallelRequests = 0 waste at every depth (serialises compute); fmParallel =
defect lives in overlapped compute. Self-corrected THREE times before the mode-verified data landed — an
early -r ladder was accidentally run fmUnordered and nearly produced a false "clean at <=4" conclusion,
caught only by verifying `filter_mode=` in the log. **Verify the compiled mode from the LOG, never from the
filename or the -r value.** Deep research banked: reservation/in-flight registry = fix #1 (PARKED).

### 2. plan-retry-bias — a measured BIAS, not a cure
Gated dump/sleep/re-plan. Swept S(10..250ms) x threads(1/2/4/8/24), ~95 cells, T=24 band repeated:
```text
S=10: ~47000 dup, 62 fps     S=30: ~4700 dup, 178 fps (fps peak)
S=40: ~3900 dup, 156 fps     S=50: ~2100 dup, 127 fps (efficiency knee -> CHOSEN)
S=70: ~1900 dup, 100 fps
```
Reproducible within a few % across runs. Throttle-vs-adopt guard PASSED (fps ROSE into the sweet spot =
genuine adoption, not throttling). Duplicates plateau ~2000, never ~0 -> bias confirmed.
**R-ARCH-08: ONLY under fmParallel.** Under fmParallelRequests: 8903 dumped plans, 8903 useless sleeps, 2993
plans hitting the cap, dumped-holes 136043 vs kept-holes 4852 (holes did NOT shrink) -> 2.7x throttle
(122 vs 337 fps) AND worse duplicates (139 vs 1). Turning it OFF made the ship mode 2.7x faster and cleaner.

### 3. HALF-500 — validated, shipping
Gated three-way TINY/HALF/NORMAL. HALF = NORMAL except ceiling 500 + MAX_HOT_ZONES 3 (the CR4-preserving
companion: 228 protected, CR4 wants >=456, 500 fits). Derived: MAX_PROTECTED 348->228, GRID_FLOOR 25->15
(the grid-floor change was missed by BOTH the designer scope and the coder review; caught by cold-verifying
the formula — `zones * (BACK/INTERVAL)`).
Controlled A/B, 3000 frames, distinct markers:
```text
                                          NORMAL-1000   HALF-500
frames_recently_evicted_then_re_requested       0           0     <- DECISIVE: neither re-churns
frames_evicted                               1904        2456     (HALF evicts ~29% more — all COLD)
slot_count_max                               1101         551     (HALF ~half the memory)
fps                                         125.5       126.4     <- identical
```
Conclusion: 500 gives up nothing measurable. (Took three false starts — twice the "FULL" reference build was
still HALF because the macro edit had not been rebuilt; caught by reading `active_ceiling` in the log.)

### 4. CHROMA DESATURATION — found, fixed, validated
Root cause: K.1E.2 PROOF PLACEHOLDERS (threshold=255/strength=255/all-narrow) live in the shipping config
since the pixel-layer rebuild; the deferred swap never happened. threshold=255 -> the blend never shuts off
across real chroma changes -> recursive averaging toward neutral -> brown/red goes grey. **The math never
regressed; only the parameter surface did.** Fixed to Y 35/192/wide, U/V 47/255/narrow — subsequently
CONFIRMED to be cnr2's own defaults exactly (source-surveyed from AviSynth-vsCnr2).
```text
frames 8/9 (same clip)      before      after
class                       BUG         SUBSTANTIVE (declassified)
U MAD / V MAD               3.65/3.52   1.08/1.15
red/brown magnitude delta   -6.59       -1.82   (~3.6x better)
desaturation-bug frames     2           0
957-frame interlaced clip:  3 residual bug frames (0.3%), hard-cut/flashing content only
```
**Every structural gate stayed green throughout** (placeholder-build vs placeholder-build). Only a human
eyeball caught it -> R-PROCESS-33 (quality gate) ratified; the Python chroma-delta analyser is now standing
test kit. Also: the analyser labelled frame 8 a "chroma-inclusive scene candidate" and both coder and
designer read the label as the phenomenon — W3X looked at the frames: no scene change. Instruments classify;
footage arbitrates.

### 5. THE OPTION PARSER — the user surface exists
Eleven descriptive options: y/u/v_threshold, y/u/v_strength, y/u/v_curve ("wide"/"narrow"), scene_threshold
(0.0-100.0), scene_chroma (bool). Parsed at create, strictly validated (throw), applied, echoed live:
`response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow scene_threshold=10.0 scene_chroma=false`.
Preceded by a cnr2 source survey (the option surface authority is CNR3_cnr2_option_mapping_and_spec_v6.md)
and a GAP ANALYSIS (R-PROCESS-31) that cold-resolved three unknowns: curve enum already WIRED; threshold==0
already SAFE (centre-only); scene_chroma PLUMBING-ONLY. Naming is DESCRIPTIVE by W3X decision — cnr2's
ln/lm/un/um/vn/vm/mode are reference-only, and `mode="oxx"` became three explicit curve params (which
incidentally DISSOLVED the cnr2 permissive-parsing compatibility question entirely).
Gate: 57/57; no-options == defaults; explicit-defaults == no-options (byte-identical response_config);
non-defaults reflect all 11 (`y=12/100/narrow u=23/200/wide v=34/210/wide scene_threshold=5.5
scene_chroma=true`); **R-PROCESS-19 anchor: no-args output byte-identical to the interim build
(595,336,353 bytes, fc /b clean)**.
DEFERRED at commit: invalid-option throws (item 8) and threshold==0 (item 9) — both discharge against the
rider build; README user docs (its own patch).

### KEY FACTS A NEW CHAT MUST NOT RE-DERIVE
- **Selftest count is 57 and config-dependent** (56 + PLANTRACE's diag3c2 test). Under HALF two hot-zone
  tests SKIP; total stays 57. Locate any change in code — never hand-wave.
- **Y is NOT filtered.** The Y response is a luma-change GUARD that gates chroma blending via cross-plane
  weighting (`weight_u = table_y[diff_y] * table_u[diff_u]`). This is the least obvious semantic in the
  filter and cnr2's docs never said it.
- **threshold==0 is VALID** (cnr2 range compatibility) and already special-cased centre-only; never divides
  by zero. Do not "fix" it to 1..255.
- **scene_chroma=false is cnr2 parity** and is the knob for chroma-only cuts / flashing lights.
- **Ship config gates are the product** (built DLL via PyPI); do not change them as a side effect.

*** end v6.0 delta ***


**Version:** v5.0 (2026-07-12). Supersedes v4.30. An analysis/instrumentation sub-arc ran to make the cache's
behaviour under out-of-order requests fully legible. FOUR patches, all through the full gate (canonical 4-way
56/56 / forced-fail 55/56 e1; R-PROCESS-19 macro-off byte-identical S8=497,668,851; L1/L2 oracles; CMS
UNCHANGED at 07.15). Committed marker at this DELTA: **CMS07-DIAG.frame-lifecycle-bail-counters**.

**COMMITTED (in order):**
1. **CMS07-DIAG.intent-counted-lookups** — intent counting replaces uniform lookup counting (count only when
   the probe outcome is uncertain and changes behaviour). Cnr3LookupCountPolicy{none,full,hit_only} defaulted
   on the two lookup primitives; six sites opt in (site1 hit_only, site2 full, walk full-except-N-1-hit_only,
   adopt hit_only, store-dup hit_only). Old L1 oracle 66.664 RETIRED. New L1 EXACT 7279/7279/0=100.000;
   L2 12109/7279/4830. Identity: site-1 hits == D-SUM-12 frames_cache_hit (L1 0, L2 4830). hits+misses==queries
   holds by construction.
2. **CMS07-DIAG.lookup-site-breakdown** — 11 per-site D-SUM-04 rows (invocations/looks/hits/misses) + legend +
   purpose lines + print-only self-check (per-site sums == merged totals; never wired to selftest). 10a/10b
   store nesting handled (no AS2 double-count); selftest routes count nothing (proven: selftest prints all 11
   rows at 0/0/0).
3. **CMS07-DIAG.frame-lifecycle-bail-counters** — five origins (frame0/floor/ordinary_target/recovery_hole/
   recovery_target); events a/b/e/x/f each total+5 origins, all independently counted (COUNT never compute);
   spine b==f+e+x; 19 self-checks OK. L2: b = floor1+ord884+hole4829+target1566; prod-tie 2450, AS2-tie 4830.

**IN FLIGHT (approved, HELD):** CMS07-SCAFFOLD.filter-mode-selector — compile-time selector top of
build_config.h (fmUnordered/fmParallelRequests/fmParallel; exactly-one #error guard; mode suffixed onto
CNR3_EDIT_VERSION -> `...selector:fmUnordered`; filter_mode= provenance line). Default fmUnordered = identical.
4-way PASS; byte-identical satisfied BY INSPECTION (both change sites cold-traced; token resolves to fmUnordered;
fprintf stderr-only). HELD for the marker fix (same cnr3_create_filter site).

**OPEN — run-log marker regression:** plugin no longer prints its edit_version/CMS07 marker to the run log.
Cold-verified: CNR3_EDIT_VERSION exists in ONE source place only (selftest summary), already absent at the
earliest snapshot (honest-cache-hit-metrics); NOT caused by any of the four patches. Root defect: gate checks
selftest + frame bytes but never run-log emission content (now R-PROCESS-27). Fix: bisect older GitHub trees
(verify by marker) to pin removal + capture exact original line; restore emission at cnr3_create_filter; add
emission-presence gate. Selector commits with/after it.

**KEY MEASURED FINDING (L1noR, fmUnordered no -r 1):** prefetch reordering routes ~half a linear clip through
recovery; EVERY hole ADOPTED not recomputed (b=7280, recovery_hole computed=0, bail-before recovery_hole=3739;
zero wasted recompute). e/x stay 0 — single-threaded fmUnordered cannot race; the race arms await real
fmParallel (the whirl / A2).

**FORWARD:** pin+restore marker -> commit selector -> sample runs (200-frame TINY baseline; shuffled; first
fmParallel whirl; PROOF=NORMAL, EXPERIMENT=TINY) -> A1 build (spec v0.4; Q-B = the original D-SUM-01
out_of_order=0 question) -> A2 (fmParallel churn = C1-under-race) -> A3 (real 576p50 via A1).

**DOC SET:** DELTA v5.0 / Doc B v4.0 / Doc A v4.0 (adds R-PROCESS-27) / Provenance v2.0 / FI v8.0 / Taxonomy
Findings v06 / A1 spec v0.4 / Role Handover v2.0 / Reviewer Intro v4.0 / CMS design v7.15 (UNCHANGED).

---

*(v4.30 detail retained below as history.)*


**Version:** v4.30. Supersedes v4.29. **THE IN-PLUGIN DIAG ARC IS CLOSED.** Three commits since v4.29, all pushed on
dev_cache_manager:

1. **DIAG.4 COMMITTED + PUSHED** — marker `CMS07-DIAG.4-memory-dsum02-and-arc-close`. Memory D-SUM-02 implemented
(was a stub): five snapshot points (baseline @cnr3_create / periodic @arAllFramesReady every
CNR3_MEMORY_DIAG_FRAME_INTERVAL=1000 / pre-cleanup / post-cleanup / summary @cnr3_free_filter); Cnr3MemoryStats
accumulates 13 dynamic metrics (min/avg/max) + 3 peaks + baseline; anchor [DSUM02-SNAPSHOT|SUMMARY]; pure two-gate
#ifdef (CNR3_DIAG_COMPUTE/PRINT_DSUM02_MEMORY). SEVEN misleading/duplicate metrics REMOVED from print+accumulation
(commit_limit, system_total_pagefile, system_total_virtual, perf_physical_total, system_avail/used_pagefile,
process_pagefile_usage) — dynamic set 16->13; retained only as raw sampled fields. The ONE production change
(ungated, R-PROCESS-21/25): explicit `data->output_cache.clear()` in cnr3_free_filter before delete, status-captured
+ (void)'d, never aborts teardown, fail-safe (declines if any slot pinned) — enables post-cleanup measurement; pins
proven zero at teardown (D-SUM-04 balance==0 same run). Memory spec advanced to **v3.4**. ARC CLOSED: **all 14 D-SUM
families live** + end-of-run summaries + plan-trace present. Selftest **56/56** (memory block is compute-gated).

2. **Prune-rechurn recency-gate COMMITTED + PUSHED** (ANALYSIS-TRACK; marker CMS07-DIAG.prune-rechurn-recency-gate).
The misleading D-SUM-10 `frames_evicted_then_re_requested` (counted ANY ring match regardless of eviction age ->
dominated by intended far-revisits; read 481 on S9) is REPLACED by `frames_recently_evicted_then_re_requested`,
gated on `eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP = 3*BACK_RADIUS` (45 TINY / 150 NORMAL). Constant chosen
from the EMPIRICAL VALLEY: across S9/S9c/S9d/S9e every evict-then-refetch event is either recovery-local (gap<=50)
or far-revisit (gap>=101), 51-100 empirically empty; 3xBACK_RADIUS sits in the valley — catches recovery-local,
rejects far revisits. Full ring scan / histogram / top-thrashers UNCHANGED (the counter is a free rider gating on
the already-computed eviction_gap). Proven: S9/S9c/S9d=0, S9e=40. This is the honesty-filter precedent.

3. **Derived-health-ratios COMMITTED + PUSHED** (ANALYSIS-TRACK; marker CMS07-DIAG.derived-health-ratios). One
additive end-of-run `[DSUM-HEALTH]` block, 8 derived ratios computed from already-final D-SUM counters (no new
measurement). Rows: cache_hit_and_supplied_percent, cache_miss_recovery_plan_percent,
recovery_plan_holes_filled_percent, return_to_vs_success_percent, recent_rechurn_vs_evicted_percent,
frames_per_prune_event (average), recalc_multiple_vs_recalced_frames_percent, recalced_frames_vs_total_percent.
Per-ratio gating (all-operands-live); disabled families emit "(source D-SUM-NN disabled)"; zero denominator emits
"n/a" (distinct from 0.000). Wiring Option A (fresh final QUIESCENT snapshots at teardown via existing helpers;
clear() is D-SUM-10 counter-neutral). Proven: macro-on hand-check all 8 == raw counters (S8 576p50); n/a vs 0.000
both shown; macro-off gates-out; macro-off BYTE-IDENTICAL frame output (fc /b, 497,668,851 bytes each); 56/56.
These 8 came from an (a)/(b) COUNTER CLASSIFICATION pass; THREE candidate ratios were REJECTED as misleading and
routed to A1: recovery-search-hit (~100% "plan formed"), hole-source-retrieval (cross-lifecycle), recalc-and-stored
(cross-family = A1 L-rate).

**FORWARD = external ANALYSIS TRACK only.** A1 (plan-trace analysis tool; absorbs FI-11 offline; may run parallel;
its input set now includes the 3 rejected ratios above) / A2 (fmParallel = C1-under-race gate owed from 3b) / A3
(real 576p50 via A1). No in-plugin DIAG steps remain. CMS design UNCHANGED at 07.15 (all three commits are
diagnostic-only; no rule/constant/AS-scope change). FI-11 in-run counter deferred-but-expected.

STANDING (do not decay): (1) the 3c.2 live bail-site->helper STATE-PLUMBING is diff-verified but confirm at the
first A-series REAL induced failure. (2) CODER-CHAT CAUTION — a coder chat produced two consecutive mangled
run-instruction sets (invented run-folders / non-canonical paths) after a long reliable run; evaluate coder
responses against known-good baselines until stable. The canonical 4-way is now a Document A standing rule
(R-PROCESS-26). (3) Two TINY log-string cleanup candidates banked (misleading "SKIPPED under ...TINYCACHE..."
message; stale D-SUM-14 "tiny profile interval=3" note printing on NORMAL runs) — observation-clarity backlog, not
scheduled.

(Prior v4.29 banner retained as history:)

**Version:** v4.29. Supersedes v4.28. **DIAG.3c.2 COMMITTED + PUSHED** (dump-on-bail + failure detail; the DIAG.3c
plan-trace family is now COMPLETE). Marker CNR3_EDIT_VERSION -> CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail.
3c.2 adds, on the FAILURE paths only: Set 5 fail_reason (16 categories, assigned per bail SITE by source location,
never message-parsed); Set 4 local X=not_reached / E=error_here (plan-trace-local codes, production
Cnr3LiveRecoveryHoleOutcome enum unchanged); a once-guarded bail dump sharing the 3c.1 clean-end dump path.
Per-site ADDITIVE FAILED-record writes at all 65 cnr3_set_filter_error sites (arInitial 14 / arAllFramesReady 50 /
top-level 1). Recovery X-derivation (X = unreached holes + target; empty for non-recovery) via the shared inline
helper cnr3_live_plantrace_make_failed_result_from_request (plugin_internal.h, gated), called by BOTH the live AR
FAILED path AND the induced-bail selftest (single source of truth, no drift). Proof gate CLOSED, verified cold:
all-on four-way 57/57 x3 + 56/57 forced-fail; macro-off four-way 56/56 x3 + 55/56 (whole family compiles out; the
65 sites revert to set_filter_error+return nullptr); INDUCED-BAIL selftest (arInitial minimal INVALID_LIFECYCLE
E=n X=[] no-O; recovery SOURCE_RETRIEVAL_FAILED E=31 X=[32,33] adopted_skipped=[30], once-guard = no clean-end
duplicate); A/B plugin byte-identical S1/S7/S8, legend-once (X/E line once), 2f no-FAILED-on-clean. Decisions:
M1(a) per-site writers + shared builders (NOT centralize via set_filter_error); M2 explicit once-guarded bail dump
(no reliance on free_filter); master gate only (no bail sub-gate); E=ACTUAL failing frame (not forced to n);
AI-06 mixed site resolved by control-flow category split (recovery-refusal=15 / discharge-fail=8), never message-
parsing. Findings resolved through the cycle: D3C2-1 (helper defs must be gated -> confirmed, macro-off proves it),
D3C2-2 (indentation cleanup), duplicate X/E legend (v3 overlay), selftest getFrame-TU link failure (v5 shared-helper
refactor). STANDING (do not decay): the live AR/arInitial -> helper STATE-PLUMBING (does the bail site pass correct
live request_data) is diff-verified across the 65 sites but not runtime-invoked in the unit test; confirm at the
first A-series REAL induced failure. Files (8): cnr3_arInitial.cpp, cnr3_arAllFramesReady.cpp, vapoursynth-Cnr3.cpp,
cnr3_diagnostics.{h,cpp}, cnr3_plugin_internal.h, cnr3_cache_core_selftest.cpp, cnr3_build_config.h (marker).
**NEXT: DIAG.4** — memory D-SUM-02 (currently a stub in cnr3_memory_diagnostics.cpp) per
cnr3_memory_diagnostics_spec_v2.md; salvage-assessment of the superseded CMS06-era memory-diag code IN-SCOPE;
whole-framework observe-only capstone proof; ARC CLOSE. This is the LAST in-plugin DIAG step. Then the external
analysis track A1/A2/A3. Standing: C1-under-race gate (A2); FI-11 in-run counter deferred-but-expected. CMS 07.15.

(Prior v4.28 banner retained as history:)

**Version:** v4.28. Supersedes v4.27. **DIAG.3c.1 COMMITTED + PUSHED** (observe-only per-output-frame plan-trace
capture + clean-end dump; the getFrame plan-trace family). Marker CNR3_EDIT_VERSION ->
CMS07-DIAG.3c.1-plantrace-clean-end-capture. The single 3c.1 commit FOLDS the emission-format refinement (spec
v2.3) into the capture patch. Proof gate CLOSED, verified cold: (1) plugin macro-off BYTE-IDENTICAL (fc /b,
plantrace-off vs on) PASS on S1/S7/S8 — the R-PROCESS-19 observe-only exit gate; (2) four-way selftest
all-on / macro-off / restored-all-on = 56/56 x3 + 55/1 forced-fail (invariant_violation) per config,
[DSUM-PLANTRACE] present all-on / absent macro-off / present restored; (3) content sanity on the single-line
format — S1 control (10 O + 10 R paired, FRAME0 + PRED_PRESENT, branch-specific sources=[n], branch-derived
pinned=[n-1]/frame0=[]), S8 all four branches well-formed (RECOVERY_EXACT anchor-pinned + holes/sources;
RECOVERY_FLOOR floor + empty pinned; the shuffle-driven recover-then-cache-hit chain visible), BEGIN/END record
counts match (S1 20/20, S8 302/302), truncated=0, window respected, no unpinned=, post_compute_discarded present.
CAPTURE (unchanged from the approved capture patch): O = arInitial RAII guard (enter_tick true-entry,
published-success only, bail-safe); R = four arAllFramesReady branch-local captures (copy-before-discard,
success-path only, no bail touch); enter_tick outside the diag mutex, action_seq inside (fmUnordered-safe
tie-break); windowed preallocated buffer + reserve_failed tripwire, no ring; branch-specific sources /
branch-derived pinned, no pin-list accessor. EMISSION (spec v2.2 -> v2.3): one physical ASCII line per record,
fixed schema (empty=[]); pad only enter_tick/exit_tick/seq/frame, role-list/codes unpadded; ONE natural-order
block (action_seq order), the three in-plugin sort views REMOVED (VIEW_* sub-gates deleted); legend once,
column-aligned, O-item/R-item; per-instance BEGIN/END schema=3c1v1 (window, records, truncated=reserve_failed);
Set 4 L -> post_compute_discarded (source enum adopted_post_compute_loser unchanged); U/unpinned DROPPED
(near-universal proven-safe discharge; pin accounting is D-SUM-04/12 — supersedes the B.3.1 unpinned-fact half,
R-PROCESS-25 approved). Decisions banked: D3C1-A..F (confirm cycle); D3C1E-1 (emission v1 delta left unused
helper params after the U removal -> v2 delta fixed by param removal + call-site updates). NOT in 3c.1 (-> 3c.2):
dump-on-bail, Set 4 X/E, Set 5 fail_reason, the 65 bail-site writes. **NEXT: DIAG.3c.2** (invasive; R-PROCESS-
21/25; site-to-category TABLE over the 65 bail sites as its foundation) -> DIAG.4 memory D-SUM-02 (arc close) ->
analysis track A1 (plan-trace tool, absorbs FI-11 offline) / A2 (fmParallel = C1-under-race gate) / A3 (real
576p50 campaign). Standing: C1-ownership-under-race REQUIRED FUTURE GATE (A2); FI-11 in-run counter deferred-but-
expected. CMS design UNCHANGED (07.15).

(Prior v4.27 banner retained as history:)

**Version:** v4.27. Supersedes v4.26. **DIAG.3b COMMITTED + PUSHED** (D-SUM-06 source-lifecycle + D-SUM-07
temp-output-lifecycle + D-SUM-09 return-transfer + D-SUM-14 scene-reset). Proof gate CLOSED and verified cold:
(1) SIX-config R-PROCESS-19 matrix GREEN — 28 selftest runs (7 configs x four-way), block-presence exact (each
disabled family's [DSUM-SUMMARY] absent in its config, present otherwise, all-off none, restore all four),
56/56 PASS on normal/verbose, 55/1 forced-fail single named induced failure, zero count anomalies. (2) S-series
real-run (-r 1, S1/S3/S7/S8 = 10/120/800/800 frames, no bail) — all THREE balances (source_frame_release_balance,
temporary_output_balance, lookup_ref_balance) == 0 on all four; failure-category fields
(same_activation_request_violations, partial_acquire_failures, promotion_mismatches) == 0; D-SUM-07 equation
created == stored+released+transferred holds EXACTLY (S8: 932 = 653+0+279); global sweep of ALL families found
zero non-zero balance/error/violation anomalies; normal profile (near-grid discriminating 0/5/30/30 < detections).
Committed with a SANCTIONED marker-only build_config touch: CNR3_EDIT_VERSION ->
CMS07-DIAG.3b-lifecycle-return-scene (3b adds no gates, so the marker is the sole build_config change); commit
message notes C1-C4, C-ALIAS, D-2.
**C1-OWNERSHIP-UNDER-RACE = REQUIRED FUTURE GATE (not a note):** under -r 1 no first-in-best-dressed race occurs,
so temporary_outputs_released == 0 and duplicate_computed_but_discarded == 0 on all four — the D-SUM-07 RELEASED/
discard arm (where a double-free or ambiguous owner would surface, exactly what C1/C-ALIAS reason about) is NOT
exercised. C1 MUST be re-accepted on the fmParallel / -r >1 run; treat that run as the C1 acceptance gate.
Create/store/transfer site-completeness IS proven under real churn. **PLAN-TRACE SPEC now v2.2 (CONTROLLING for
DIAG.3c):** v2 full revision (6 findings resolved incl. the un-named 6th = branch-derived pinned; 8 locked
decisions with reasoning bound inline) -> v2.1 (fidelity finding D-V2-1: bail-site total is 65 CALL sites not 66 —
AR raw grep 51 includes the DEFINITION at cnr3_arAllFramesReady.cpp:526; AR call sites = 50; 14+50+1=65) -> v2.2
(view (a) sort key = (enter_tick ASC, action_seq ASC), phase dropped as a sort term; the enter_tick-OUTSIDE-lock /
action_seq-INSIDE-lock capture invariant with the fmUnordered failure mode bound to each half). **DIAG.3c.1 SCOPED
(scope v2) and HANDED TO CODER** (spec v2.2 + scope v2 + post-3b src) — coder now assessing. 3c SPLIT: 3c.1 =
buffered plan/result capture + clean-end dump, observe-only (R-PROCESS-19), touches NO bail site; 3c.2 = dump-on-
bail + E/X + Set 5 failure-reason writes across the 65 bail sites (invasive, R-PROCESS-21/25, site-to-category
TABLE from live source as its foundation). DESIGNER RECOMMENDATION: commit 3c.1 standalone then 3c.2 next;
combine-vs-split is the coordinator's call at the 3c.1 boundary. FI-11 in-run ring<->recovery correlation counter
DEFERRED-BUT-EXPECTED — keep visible. CMS design UNCHANGED (07.15). Then DIAG.4 memory. The "3b committed" doc-touch
(this DELTA v4.27 / Doc B v3.17 / Provenance v1.5) advances the record to here.

(Prior v4.26 banner retained as history:)

**Version:** v4.26. Supersedes v4.25. **DIAG.3a COMMITTED + PUSHED** (D-SUM-03/12/13; v2 patch after defect D-1 —
the recovery-plan destroy observer was defined but never invoked; fixed by one gated call in
cnr3_discard_frame_data_with_cache before `delete request_data`, covering all ~51 teardown paths). Proofs: four-way
56/56; five-config matrix; S-series recovery_plan_balance==0 under churn (S8: 171 created+destroyed); saturation
false. RECOVERY-RATE BASELINE: S1 0% / S3 27.5% / S7 0.375% / S8 21.4% — churn is ARRIVAL-DISORDER-driven (shuffle),
not seek-driven (jumps). **DIAG.3b (D-SUM-06 source-lifecycle + D-SUM-07 temp-output-lifecycle + D-SUM-09
return-transfer + D-SUM-14 scene-reset): confirm report accepted (decisions C1-C4 + C-ALIAS), PATCH APPROVED with
process finding D-2 (an unauthorized-but-pure two-line condition hoist in the frame-0 path; retro-sanctioned;
propose-first boundary restated — now ratified as R-PROCESS-25 in Document A v3.13, alongside R-PROCESS-24
flush-per-line). Four-way all-on PASS (56/56 etc). NOTE the synthetic-vs-real reading discipline: the selftest
reference emitters DELIBERATELY feed non-zero failure-category values (same_activation_request_violations 1,
partial_acquire_failures 1, promotion_mismatches 1) to prove the writers — these are hardcoded fixtures, NOT
anomalies; the REAL-RUN acceptance requires those fields == 0 in the S-series. STILL OWED for DIAG.3b commit:
the SIX-config R-PROCESS-19 matrix + the S-series real-run acceptance (three balances == 0 under churn; the three
failure-category fields == 0; D-SUM-07 balance must close even with duplicate_computed_but_discarded > 0 on S7/S8
— the C1-semantics test).** DIAG.3 sub-sequence: 3a committed / 3b proof-gate / 3c = plan-trace (spec-v2 first,
likely 3c.1 capture + 3c.2 dump-on-bail split). Then DIAG.4 memory. CMS design UNCHANGED (07.15). Key context
consolidation: CNR3_Ring_and_PlanTrace_Design_Rationale_and_Intent_v1.md now carries the full ring/FI-11 and
plan-trace design rationale (required reading for a new designer; primary input to the 3c spec v2).

---
(Prior v4.25 banner retained as history:)


**Version:** v4.25 (SLIMMED). Supersedes v4.24. **DIAG.2b is COMMITTED + PUSHED** (was "active" in v4.24). DIAG.2b delivered D-SUM-04 (two narrow provable balances: slot pin balance + lookup-ref handoff invariant acquired==released_by_cache_core+transferred, + total_pin_count cross-check + ownership_errors tripwire; global VSFrame ref balance deliberately NOT claimed per FI-12), D-SUM-05 (central cache_state_invariants_hold_locked() observed via the designer-approved CNR3_DSUM05_FAIL(tag) macro on the 18 return-false sites, macro-off byte-identical; + structural samples), D-SUM-08 (store outcomes at the combined store/prune wrapper; store_failures excludes Cnr3Status::duplicate per amendment A1; AS2 checkpoint promotions only per FI-13). Amendments A1-A5 applied. PROOFS: all-on four-way 56/56; R-PROCESS-19 five-config macro matrix PASS (each family compiles out independently + combined, four-way identical, family block absent when off); S-series -r 1 (S1/S3/S7/S8) pin_balance==0 AND lookup_ref_balance==0 UNDER CHURN (S7/S8: ~950 stores, 168 evictions each), D-SUM-05 violations==0, store_failures==0. No build_config gate change, no new TU. Const observation via mutable diagnostic members (Option A). **NEXT: DIAG.3a** scoped (D-SUM-03 recovery-search + D-SUM-12 recovery-plan/rate + D-SUM-13 recalculation — the recovery/churn trio answering FI-11), with a proposed DIAG.3/3a-3b batch split; DIAG.3b = D-SUM-06/07/09/14. CMS design UNCHANGED (07.15). New coder session (memoryless ChatGPT) onboarded mid-arc; two consecutive strong deliverables (DIAG.2b confirm + patch).

---
(Prior v4.24 banner retained as history:)
# CNR3 — THIS-CHAT DELTA: current-state companion (SLIMMED, marshalling arc + DIAGNOSTICS ARC through DIAG.2a) — v4.24

**Version:** v4.24 (SLIMMED). Supersedes v4.23. Marshalling-optimisation arc CLOSED at ~-80% (unchanged from v4.23). **NOW ADVANCES the record into the DIAGNOSTICS ARC**, which is ACTIVE. Committed + pushed since v4.23: (1) **selftest skip-pass fix** — the two CNR3_KEYSTONE_DEV_TRACE-conditional keystone tests returned lifecycle_violation when KDT is undefined and the binary run loop (no skip category) counted them as failures, so the default (KDT-off) config was silently 54/56; fixed to emit a skip line + return ok, restoring an honest default-config 56/56 with KDT-on assertions untouched. (2) **DIAG.1** — the D-SUM diagnostics framework + D-SUM-01 (request-arrival/order reference family) + the R-PROCESS-19 observe-only proof (macro-on 56/56 four-way with [DSUM-SUMMARY]; macro-off byte-identical, bodies compiled out). Per-instance diagnostics-only mutex, snapshot-outside-lock, [DSUM-SUMMARY] tag (distinct from [KDT-SUMMARY]). (3) **DIAG.2a** — D-SUM-11 hot-zone summary writer (existing gated observe hooks untouched) + D-SUM-10 prune/eviction/re-churn telemetry: Cnr3CachePruneDiagnosticStats, evict-then-re-requested re-churn counter, derived-capacity recently-evicted ring (k=16 -> 16000 normal / 1024 tiny, saturation-reported), gap histogram [DSUM10-GAP-HISTO], top-N thrashers [DSUM10-TOP-THRASH], bounded ring dumps ([DSUM10-RING-WINDOW]/[DSUM10-RING-FINAL] default-on, [DSUM10-RING-FULL] default-off). Observe-only (mutable diagnostic member Option A for the const lookup path; consistent with mutable cache_mutex_). Gate matrix PASS (10on/11on, both-off, 10off/11on, 10on/11off); real VS2026 four-way; S-series -r 1 (S1/S3/S7/S8) telemetry sane, ring never saturates. **DIAG.2a FINDING (banked -> FI-11):** D-SUM-10 re-churn hooks the predecessor-lookup path; S7/S8 showed the costly evict-then-rebuild churn flows through the recovery/anchor path (plan_bounded_recovery_search_and_record_anchor_pin), which D-SUM-10 does NOT hook — that is D-SUM-12's (recovery-rate) job in DIAG.3. **ACTIVE: DIAG.2b** (D-SUM-04 ownership-balance + D-SUM-05 cache-integrity + D-SUM-08 store/duplicate) — v2 scope issued after the coder's pre-patch inventory review caught that a global VSFrame ref balance is unprovable in-scope (RAII releases via Cnr3OwnedFrameRef reset/dtor/transfer_to_caller); D-SUM-04 re-sited to two narrow provable balances (pin balance + lookup-ref handoff invariant) per the build_config gate comment's own field names; D-SUM-05 hooks the central cache_state_invariants_hold_locked(); D-SUM-08 reads Cnr3CombinedStoreAndPruneSummary at the wrapper, AS2 promotions only. CMS DESIGN UNCHANGED (still 07.15). CNR3_EDIT_VERSION + selftest count are authoritative from committed source.

**DEFERRED ITEMS (recorded; see Future Investigations FI-11..FI-13):** FI-11 recovery-path re-churn (-> D-SUM-12, DIAG.3); FI-12 global/OwnedFrameRef-primitive ref balance (RAII releases unhookable in DIAG.2b; needs primitive instrumentation, separate exercise); FI-13 production-duplicate checkpoint-promotion signal (not exposed in Cnr3CombinedStoreAndPruneSummary; DIAG.2b counts AS2 promotions only). Standing (pre-existing): Lever B allocation pooling (~587 leaf, fmUnordered lifetime proof); fmParallel concurrency churn test (num_threads>1, after -r 1 baseline); R-PROCESS-2x flush-per-line rule (Document A ratification); plan/result plan-trace family (drafted, awaiting coder cross-check, DIAG.3).

---
(Prior v4.23 note retained as history:)
# CNR3 — THIS-CHAT DELTA: current-state companion (SLIMMED, through marshalling levers 0A/0B/3a.1/3b.1/3a.2 + validation policy + A-lite + C1 + Repack + F/3c + Staging + E) — v4.23


**Version:** v4.23 (SLIMMED). Supersedes v4.22. Extends the marshalling-optimisation arc with Lever Staging (scalar->native staging inline) and Lever E (scene-change local-accumulator), and CLOSES the arc: Tier-2 chroma-unpack fusion investigated and declined (Path C), Lever D investigated and declined (PATH-B-only). All steps value-preserving (56/56 four-way, P-series unchanged) and profiled; CMS UNCHANGED (still 07.15 — implementation optimisation only, no design/invariant change). Cumulative vs pre-lever baseline: **93,914 -> ~18,660 samples (~-80%)**. **The marshalling-optimisation arc is substantially COMPLETE at ~-80%**; the only remaining headroom is Lever B (allocation pooling, ~587 leaf) which needs an fmUnordered lifetime/thread-safety proof and is a coordinator judgement call vs pivoting to the parked diagnostics work.
- **Lever 3a.1** (committed): typed native->scalar unpack inside `cnr3_copy_native_plane_to_scalar_buffer` — removed the per-sample `cnr3_load_native_plane_sample` CALL (hoisted invariants; 8-bit `row[x]`, 16-bit unaligned-safe memcpy; widen-on-load; range check retained; existing buffer + publish loop kept). **-37% total** (67,891 -> 42,748); `load_native_plane_sample` eliminated from the hot path. NOT vectorised — vec-report `506` shifted from the call to the per-sample range-check BRANCH.
- **Lever 3b.1** (committed): inlined scalar-domain downsample tap-average in `cnr3_downsample_luma_plane_to_chroma_grid` — removed 3 calls/sample (tap_coordinates, plane_sample_at x4, downsample_luma_sample). ASYMMETRIC x1/y1 clamp (`x1=x0+1 regardless of ssw`; `y1=y0+ssh`) and `(tl+tr+bl+br+2)>>2` preserved exactly. Flat total within noise (4-run mean ~42,440); downsample FUNCTION dropped ~1,600 samples but too small a slice to move the total. LESSON: call-elimination on small leaves doesn't move the total.
- **VALIDATION POLICY** (recorded, CNR3_Validation_Policy_recorded_v1): vec-report proved the per-sample VALIDATION BRANCH is now the vectorisation blocker. Audited by provenance; posture ADOPTED: **DEFEND at source boundary (Tier 1: keep, hoist shape — VHS/analogue captures can emit out-of-range glitches, VS API does not enforce per-sample range), TRUST validated intermediates downstream (Tier 2: removable via production-private paths, relying on the no-bypass-of-Tier-1 invariant), response-table outputs bounded BY CONSTRUCTION (Tier 3: removable with provenance guard), FINAL output clamp ALWAYS stays.** CNR2 trusts input (evidence, not license).
- **Lever 3a.2** (committed): Tier-1 range-check HOIST on the big ~10k unpack leaf — 8-bit check dropped as type-guaranteed (`uint8_t` always <= 255 = 8-bit peak); 16-bit hoisted pre-scan (reject > peak before publish); branch-free conversion loop = the vectorisation target. First patch aiming vectorisation at the BIG leaf. DONE (committed). 16-bit conversion loop VECTORISED (C5001 — first vectorised hot loop in the codebase); P.8A/P.9A/P.11B unchanged (Tier-1 guarantee preserved in branch-free shape). Profile FLAT within noise (4 runs vs ~42,400 -> ~41,850 mean) on a CONFIRMED 8-bit clip (YUV420P8). The vectorised loop (C5001) is the 16-BIT conversion path, which an 8-bit profile never runs -> the flip is real but untested on the production case. PRODUCTION READING: VHS/analogue input is overwhelmingly 8-bit, so the 8-bit path is what matters, and on 8-bit 3a.1 already removed the per-sample call. The residual 8-bit unpack cost is the COPY (native byte -> int buffer -> read back) + the per-frame std::vector<int> allocation (~1,500-2,200 samples/run of resize alone). These are MEMORY/ALLOCATION costs, not arithmetic -> vectorisation cannot reduce them. So the next 8-bit levers are buffer ELIMINATION (native->native / buffer-free / pass fusion) and allocation POOLING, NOT more SIMD. (16-bit: the vectorised loop MAY pay on a P16 clip; profile one when convenient, but it is the secondary case.)
- **Lever A-lite** (committed): row-pointer/__restrict form on the 8-bit path of `cnr3_copy_native_plane_to_scalar_buffer` + a dedicated 8-bit publish loop that writes `scalar_plane` rows directly (removes the per-sample `cnr3_write_plane_sample` call). 16-bit path + validation UNCHANGED. Value-preserving (P.8A/P.9A/P.11B unchanged, 56/56). Scoped as a throwaway DIAGNOSTIC (does the 8-bit copy vectorise under restrict?) but became a real WIN: vec-report stayed reason `501` (NOT vectorised) yet the profile dropped **-7.4%** (3 runs vs ~41,850 -> ~38,750 mean, all below baseline, outside noise); the unpack leaf ~halved (~7k -> ~3.3k combined). MECHANISM: per-sample index-arithmetic + publish call-overhead removal (same CLASS as 3a.1), NOT SIMD and NOT copy-avoidance. **CORRECTS the 3a.2-era 'memory-bound' inference above:** the 8-bit copy was partly OVERHEAD-bound, not purely memory-bound — removing the overhead paid. METHOD LESSON: run the cheap diagnostic rather than trust the inference — 'didn't vectorise' != 'flat'. Cumulative ~-59% (93,914 -> ~38,750).
- **Lever C1** (committed): direct native-luma -> scalar chroma-grid downsample (BUFFER ELIMINATION). `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` now reads native luma taps DIRECTLY at the downsample coordinates and computes the chroma grid in one pass, eliminating the full-resolution `source_luma_scalar` int buffer + its resize + the inner `cnr3_copy_native_plane_to_scalar_buffer` call. Runs TWICE/frame (current + previous luma), so both materialisations go. P.4A tap geometry reproduced EXACTLY (asymmetric x1 = x0+1 regardless of ssw; y1 clamp; (tl+tr+bl+br+2)>>2; all four subsampling shapes; edge clamps). Tier-1 source guarantee preserved (16-bit OR-accumulate pre-pass rejects before publish; 8-bit type-guaranteed). Shared scalar `cnr3_downsample_luma_plane_to_chroma_grid` contract UNCHANGED. Value-preserving (P.4A/P.9A/P.7A/P.8A/P.11B unchanged, 56/56 — the coordinate-equivalence of the inline bounds/clamp checks vs the old tap-coordinate helper is PROVEN by P.4A passing, not just argued). Profile (4 runs vs post-A-lite ~38,750): total **~29,250 mean = -24.5%**, tightly clustered (spread ~74 samples); the bridge leaf collapsed ~9.5k -> ~1.2k. One fused loop auto-vectorised (C5001, bonus). BIGGEST single win of the arc. **Cumulative ~-69% (93,914 -> ~29,250).**
- **Lever Repack** (committed): `cnr3_commit_staged_native_active_samples` rewritten to one `std::memcpy` per row of `width*storage_bytes` (active samples only; native padding untouched), replacing the per-sample offset-calc + inner bit-depth branch. UB-safe (no unaligned uint16 cast — v1 had one, fixed in v2). staged_bytes is destination-stride-pitched by construction (documented). Value-preserving (P.8A padding-preservation + P.11B unchanged, 56/56). Profile (3 runs vs post-C1 ~29,250): total ~28,000 = **~-4%**, consistent. Cleaner + UB-safe + modest win.
- **Lever F/3c** (committed): inline + hoist the chroma blend loop `cnr3_process_chroma_plane_from_downsampled_luma` (the ~6,640 TOP leaf). Inlined the per-sample call chain (blend_from_response_tables / blend_chroma_sample / plane_sample_at / write_plane_sample) into a row-pointer fused loop; hoisted sample_peak/shift/shift1/shift2/table-geometry/table-pointers. Blend arithmetic reproduced BIT-EXACTLY (int64 throughout; weight=y_res*c_res -> previous_filtered, (shift-weight) -> current_source; shift1 round-half-up; signed-diff lookup, out-of-range->0), CROSS-VERIFIED by independent designer + coder derivations vs P.3A/P.5A (this is the verification that caught GAIS's 32-bit-accumulator error — both derivations confirmed int64). Tier-1 no-partial-output preserved via an up-front validation pre-pass; Tier-2 input + Tier-3 response per-sample checks removed from the fused loop per policy (provenance invariants documented in comments); shared blend primitives' contracts UNCHANGED. Value-preserving (P.3A/P.5A/P.6A/P.11B unchanged, 56/56). Profile (3 runs vs post-Repack ~28,000): total ~23,600 = **-15.7%**, consistent; the ~6,640 blend leaf collapsed off the top children (~4,260 call-chain folded into the inlined loop). Response-table gather blocks full vectorisation as expected; win is call-chain elimination. **Cumulative ~-75% (93,914 -> ~23,600).**
- **Lever Staging** (committed): inline the scalar->native STAGING conversion. `cnr3_stage_scalar_plane_to_native_bytes` replaced its delegation to the per-sample `cnr3_convert_scalar_plane_into_native_staging_bytes` (which called `cnr3_store_native_plane_sample`/`cnr3_plane_sample_at` per sample) with an inlined staging-private direct converter: hoisted storage_bytes+sample_peak, row-pointer, 8-bit direct byte write, 16-bit unaligned-safe memcpy, writing straight into the staged buffer. Reject-before-narrow reproduced EXACTLY (`cnr3_store_native_plane_sample` REJECTS out-of-range, does not clamp — verified). Padding preserved. Shared `cnr3_copy_scalar_buffer_to_native_plane` UNTOUCHED (0B Option-A discipline). SOURCE CORRECTION: the staging path had NO redundant resolved_bytes buffer (that lives only in the shared primitive, not the production staging caller) — so the win was call-elimination only, not buffer-elimination. Value-preserving (P.8A active+padding+no-partial-output, P.11B unchanged, 56/56). Profile (3 runs vs post-F/3c ~23,600): total ~19,470 = **-17.5%**, consistent; the staging leaf collapsed ~4,753 -> ~492. **Cumulative ~-79%.**
- **Lever E** (committed): scene-change local-accumulator + row-pointer hygiene in `cnr3_detect_scene_change_from_scalar_planes`. Inlined the per-sample `cnr3_plane_sample_at` (x6) and `cnr3_abs_int64` (identical `(v<0)?-v:v`); row pointers for the 6 scalar input planes; accumulate into a LOCAL int64 + local samples_examined, written back to stats before EVERY return. **Per-sample threshold trip point preserved EXACTLY** (check stays per-sample, not per-row) so the reported diff_total at any exit is the identical partial sum. `cnr3_add_scene_diff` KEPT (checked-add overflow semantics). Value-preserving (P.11B strict-diff_total-trip-point proof unchanged, 56/56). Not a vectorisation lever (vec-report 506; win is call-inlining like A-lite). Profile (post-Staging ~19,470, 2 clean runs + 1 noisy outlier): ~18,660 = **~-4%**; scene-change leaf dropped off the top children. **Cumulative ~-80% (93,914 -> ~18,660).**
- **Lever D** (investigated, DECLINED — PATH-B-only): exact SIMD downsample. Independent designer+coder investigation agreed: the scalar `cnr3_downsample_luma_plane_to_chroma_grid` is BYPASSED in production (C1's native path is hot), so SIMD-ing it is a non-win; the hot native loop doesn't auto-vectorise (asymmetric x1 clamp + tap indexing, 506); a P.4A-exact auto-vectorising reshape would need enough storage/subsampling/edge specialisation to become explicit-intrinsics/kernel work (Path B). Disproportionate for a ~1.3k leaf at -80%. CLOSED as skip-unless-explicit-SIMD-arc-deliberately-opened. VPAVGB remains REJECTED (measured +0.375-code recursion-compounding bias).
- **Tier-2 chroma-unpack fusion** (investigated, DECLINED — Path C): the ~1,874 chroma-unpack leaf is LOAD-BEARING for scene-change detection + scene-reset (they consume the same 4 scalar chroma buffers), not a free-standing materialisation like C1's luma bridge. A blend-only native-read fusion would either fork the code on scene_config (Path A — entangles an optimisation with scene-detection semantics) or require a full native scene/reset rewrite (Path B — large, P.11C proof surface). Neither proportionate at -80%. NOT pursued.
- **Arc status: SUBSTANTIALLY COMPLETE at ~-80%** (93,914 -> ~18,660). Twelve committed value-identical levers (AVX2, 0A, 0B, 3a.1, 3b.1, 3a.2, A-lite, C1, Repack, F/3c, Staging, E). Remaining leaves are all entangled or diminishing-returns: `cnr3_copy_native_plane_to_scalar_buffer` ~1,341 (chroma unpack — Path C, scene-detection-coupled), `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` ~1,245 (C1, D declined), `cnr3_allocate_scalar_plane_storage` ~587 (allocation), ntdll ~7,878 (framework/kernel — not ours). **ONLY remaining headroom = Lever B (allocation pooling, ~587 leaf)** — reuse per-frame scalar buffers instead of alloc/free each frame; NOT a hot-loop change but a buffer-LIFETIME change needing an fmUnordered thread-safety proof (one activation per instance may make an instance pool safe, but must be PROVEN not assumed). Coordinator judgement call: pursue B for the ~587-sample win, or declare the arc done at -80% and pivot to parked diagnostics work (D-SUM menu, selftest skip-pass fix, end-of-run integrity report, abort_on_error param, warn-vs-hard-fail policy). Parked: diagnostics D-SUM menu, selftest skip-pass fix.
(Prior v4.18 note retained as history:) v4.18 (SLIMMED). Supersedes v4.17. Records the opening of the **marshalling-optimisation arc** (acting on FI-10) and three committed steps, each proven 56/56 four-way (dev-trace ON) and measured:
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
Committed/pushed through:  CMS07-DIAG.derived-health-ratios  (latest). Prior: CMS07-DIAG.prune-rechurn-recency-gate; CMS07-DIAG.4-memory-dsum02-and-arc-close. IN-PLUGIN DIAG ARC CLOSED (all 14 D-SUM families live).
Selftest count:            56/56 PASS  (forced-fail 55/56 exit 1; verbose 56/56)  [health block is plugin-teardown-only, absent from selftest orchestrator; prune/health commits did not change the count]
Build target:              x64-ONLY, /arch:AVX2 HARD REQUIREMENT (both projects, Release+Debug; Win32/x86 removed; .slnx x64-only). Hard-faults on pre-AVX2 CPUs.
Diagnostics framework:     ALL 14 D-SUM families live + [DSUM-HEALTH] derived-ratios block + [DSUM-PLANTRACE] plan-trace. Master pattern: per-family CNR3_DIAG_COMPUTE_DSUMxx + CNR3_DIAG_PRINT_DSUMxx (print derived from compute); stderr-only; observe-only (R-PROCESS-19 macro-off byte-identical). Committed at NORMAL baseline (plantrace off, TINY off).
Analysis track (forward):  A1 plan-trace tool (external Python; absorbs FI-11 offline; may run parallel; input set includes the 3 health-ratios rejected in-plugin) / A2 fmParallel = C1-under-race gate (owed from 3b) / A3 real 576p50 via A1. NO in-plugin DIAG steps remain.
Validation posture:        RECORDED (CNR3_Validation_Policy_recorded_v3): defend-at-source(T1)/trust-downstream(T2)/bounded-by-construction(T3)/final-clamp-always.
Tiny diag scaffold:        CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (build_config.h, shipped OFF/commented). ON: cache_profile=tiny-100 (ceiling 100 / BACK_RADIUS 15 / FWD 3 / MAX_HOT_ZONES 2); the lever to force pruning on small runs (S9-series). NORMAL: ceiling 1000 / BACK_RADIUS 50 / FWD 10 / MAX_HOT_ZONES 5.
Controlling CMS:           CMS07.15  (cnr3_cache_manager_design_v7_15.md)  [UNCHANGED — all three recent commits are diagnostic-only; no design/invariant/constant/AS-scope change]
Production Spec:           v2.16  (repo)
Document A:                v3.13 -> TARGET v3.14 (adds R-PROCESS-26 canonical 4-way; coder-caution note)
Document B:                v3.19 -> TARGET v3.20 (top UPDATE block: arc CLOSED; forward = A1/A2/A3)
Provenance:                v1.7 -> TARGET v1.8 (banks DIAG.4 + prune-rechurn recency-gate + (a)/(b) classification + 3 rejected ratios)
Condensed Plan:            v1.9 -> TARGET v1.10 (DIAG.4 done; in-plugin plan COMPLETE)
Role Handover:             v1.16  (repo; + coder-caution + harness-ownership already present)
Reviewer Intro:            v3.9 -> currency touch (arc-closed; doc-set versions)
Coder Restart Intro:       v6.8 -> currency touch (edit_version marker; selftest 56; 4-way canonical)
Future Investigations:     v7.17  (+ 2 TINY log-string cleanup candidates)
Diagnostics spec:          v1.5  (unchanged); Memory diagnostics spec: v3.4 (advanced at DIAG.4)
Branch:                    dev_cache_manager
```
The repository is the authority — confirm CNR3_EDIT_VERSION (expect CMS07-DIAG.derived-health-ratios) and the selftest count (56) from committed source.

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
CMS (design authority)     cnr3_cache_manager_design_v7_15.md             (CMS07.15; UNCHANGED)
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_16.md     (§3.2 context master; §3A register + charter §3A.5.0)
Document A                Document_A_CNR3_Project_Context_and_Standing_Rules_v3_14.md   (context + standing rules; +R-PROCESS-26)
Document B                Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_20.md  (current-state; top UPDATE block authoritative)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md          (subordinate); Memory spec cnr3_memory_diagnostics_spec_v3_4.md
Provenance                z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v1_8.md  (decisions + findings ledger)
Condensed Plan            CNR3_Diagnostics_Arc_Condensed_Plan_v1_10.txt  (in-plugin plan COMPLETE)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v1_16.md
Reviewer Intro            CNR3_Handover_Introduction_to_new_reviewer_chat_v3_9.md
Coder Restart Intro       CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_8.md
Future Investigations     CNR3_CMS_Future_Investigations_and_Open_Questions_v7_17.md    (companion)
Current-state (this)      THIS slimmed DELTA v4.30 (live per-phase ledger)
```
**Authority:** CMS -> Production Spec §3A -> diagnostics -> handover pack. Repository wins over any
document on build state. **Reading order:** Reviewer/Coder intro -> CMS07.15 -> Document A v3.14 ->
Document B v3.20 (top block) -> this DELTA v4.30 -> Provenance v1.8.
*(Versions confirmed against the FINAL_DOCS set this session; confirm CNR3_EDIT_VERSION [expect
CMS07-DIAG.derived-health-ratios] + selftest count [56] from repo.)*

— End of CNR3 THIS-CHAT DELTA (slimmed), v4.30.
