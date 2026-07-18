# CNR3 — Handover Introduction & Role Description for the Reviewer

*** v6.0 CURRENCY NOTE (2026-07-18) — READ FIRST; supersedes all older notes below where they differ ***

COMMITTED / PUSHED: **`CMS07-RELEASE.production-config`** on branch **`main`**.
`dev_cache_manager` has been MERGED INTO MAIN AND RETIRED — all work continues on main (open a fresh
feature branch for the next big arc). **THE PLUGIN IS SHIPPABLE**: user surface, README, and the
GitHub Actions build/release pipeline are complete and proven. CMS remains UNCHANGED at 07.15.

**WHAT LANDED SINCE THE v5.0/v6.0/v8.0/v3.0 GENERATION (2026-07-15 -> 2026-07-18), in commit order:**

1. **`CMS07-RIDER.option-error-messages`** — self-explanatory option errors. Range errors echo the
   received value; type errors read `incorrect value type, expected <expectation>`; the expectation text
   is built ONCE per option kind and shared by BOTH paths (anti-drift). Float echo via
   `std::to_chars` shortest round-trip; curve echo via a `value_size`-bounded byte loop (no libc scan),
   sanitised to printable ASCII (control bytes -> `?`), truncated at 32 with an in-quotes `...` marker;
   `scene_chroma` reports `expected 0 or 1.` via an optional expectation-override on the int helper.
   **NOTABLE: a NEW coder chat's confirm-before-patch pass found THREE defects in the DESIGNER's scope
   before writing a line** — `%g` hid the near-miss (100.0001 renders as "100"), `%.32s` was an
   out-of-bounds read on non-NUL-terminated map data (ASan-proven), and raw echo broke the one-line
   guarantee. Designer rulings D1/D2/D3 amended the scope; the patch then implemented them exactly.
   Gate: 4-way; **17/17 runtime cases**; byte-identical frames (595,336,353 bytes, fc /b clean).
   DISCHARGED the parser commit's deferred gate items 8 (invalid-option throws) and 9 (threshold==0
   SUCCEEDS, showing y=0/192/wide).
2. **`CMS07-FIX.provenance-flush`** — `std::fflush(stderr)` after the provenance emissions so the
   identity lines survive abnormal termination. Also settled the ERROR-DOMAIN MAP: **`mapSetError` for
   creation errors, `setFilterError` for frame-processing errors, stderr for diagnostics/status** —
   the error path is not a stream and needs no flush.
3. **`CMS07-DOC.cnr2-descriptive-options-readme`** — README.md SHIPPED. **PQ-2 CLOSED.** Option docs,
   cnr2->CNR3 equivalence table, progressive + interlaced usage (the proven `split_into_fields` /
   `reweave_fields` helpers in Appendix A, with `SetFieldBased` added; metadata-poor `.avi` warning),
   errors section, and a mechanism-by-mechanism cache/technical section.
4. **`CMS07-CHORE.build-hygiene-nominmax-r78-headers`** — NOMINMAX in both vcxproj configs and the CI
   cl line (windows.h min/max macros vs ~20 std::min/max uses; memory_diagnostics includes windows.h and
   was safe only by include-ordering luck); its local defines wrapped in `#ifndef` guards. Vendored
   headers refreshed to the **R78 release** set + VERSION.txt. **`VS_USE_LATEST_API` deliberately NOT
   defined**: the header self-versions and CNR3 compiles at the default **API 4.0 profile** = maximum
   core compatibility. Declaring 4.2 would REFUSE to load on older cores for zero gain; CNR3 uses no
   4.1/4.2 features and is property-transparent (every output frame is `copyFrame(source)`, so `_Range`
   and all other props flow through regardless of compiled API level).
5. **`CMS07-RELEASE.production-config`** — **THE MASTER DIAGNOSTICS GATE.**
   `CNR3_DIAG_MASTER_PERMIT_DIAGS` is the single production switch. EVERY dev-instrumentation gate is
   INDIVIDUALLY wrapped in `#if defined(CNR3_DIAG_MASTER_PERMIT_DIAGS) ... #endif` — all 15 D-SUM
   families, PLANTRACE, the keystone scaffolds, AND the startup provenance emission.
   **CONTAINMENT RULE (W3X, mandatory): every NEW diagnostic gate MUST get the same individual wrap.**
   Per-item wrapping is deliberate versus one long region: the pattern is visible at every site so
   nothing can be added outside it by accident, and there is NO central #undef list to drift.
   Deliberately UNWRAPPED (production-meaningful): filter mode, cache profile, plan-retry + knobs,
   scdthr. The dead `SCAFFOLD_CMS07_K1E3_REFUSE_AFTER_FRAME2_BEFORE_RECOVERY` is now trebly commented
   AND wrapped. Stale filter-mode doc comments corrected (fmParallelRequests is the SHIPPING DEFAULT
   with its measured numbers inline; fmUnordered demoted to proven-fallback).
   **CONSEQUENCE — A SHIP BUILD'S LOG IS SILENT** (no `edit_version=`, no `filter_mode=`, no
   `response_config:`, no D-SUM). W3X ruling: ship quiet like other DLLs. Those lines return in dev.
   **THE CANONICAL SELFTEST COUNT IS NOW CONFIG-DEPENDENT IN TWO DIRECTIONS: 56/56 with the master OFF**
   (forced-fail 55/56 e1); **57/57 with the master ON** (56 base + the PLANTRACE-gated diag3c2 test;
   forced-fail 56/57 e1). LOCATE any changed count in code; never hand-wave.

**CI / RELEASE PIPELINE IS LIVE:** `.github/workflows/build-windows-x64-release.yml` — triggers on
`workflow_dispatch` and on Release creation. Builds Release x64 with an EXPLICIT `cl` command (W3X ruling:
visibility over msbuild's single-source-of-truth; a FLAG MAP comment ties every cl flag to its vcxproj
setting). Source list = `src\*.cpp` MINUS `cnr3_cache_core_selftest_main.cpp` (wildcard, so it can never
fossilize — note `cnr3_cache_core_selftest.cpp` IS part of the DLL). Flags mirror Release|x64 exactly:
`/O2 /Ot /Ob2 /Oi /GL /arch:AVX2 /GS /sdl /fp:precise /Gy /Zc:inline /MD /MP /std:c++20 /permissive-
/DNOMINMAX` + link `/LTCG /OPT:REF /OPT:NOICF /DEBUG`. Security (`/sdl`, `/GS`) KEPT by decision;
`/fp:precise` explicit and never `/fp:fast`; `/OPT:NOICF` deliberate (speed over size). Hard-fails if
`VapourSynthPluginInit2` is not exported. Emits BUILD_INFO.txt (marker/commit/ref/UTC/sha256). Zip =
[cnr3.dll, README_CNR3.md, SHA256SUMS.txt, BUILD_INFO.txt]; PDB as a SEPARATE artifact; the zip attaches
to the Release on a release trigger. First `workflow_dispatch` run PASSED (40s; VS2026 Enterprise v145 on
`windows-latest` = WS2025 since the June 2026 image migration). **No selftest in CI by W3X decision**
(maintainer's responsibility).

**PARKED / NEXT:** the **RESERVATION TABLE** is the last big architectural arc — fully designed and
banked in **CNR3_PROPOSAL_Reservation_Table_v1_0.md** (self-contained pickup doc; a coder-annotated
v1_1 also exists in FINAL_DOCS). Two BIG steps: **A** = planner selector + shared-engine hookification +
`#error` guards, proven by BYTE-IDENTITY; **B** = the reservation path (registry >= 2x numThreads,
intent-mark at arInitial vs hard claim at compute-start, collapse-at-F, 10s bounded awaits that LOUD
CLIP-FAIL on timeout, fetch avoidance, RAII deregistration on every exit), proven by behaviour
(L -> ~0, duplicates -> ~0) and benchmarked against 337 fps to decide ship-or-park.
**Scope A's FIRST mandatory deliverable: cold-verify arInitial's ACTUAL pass/mutex structure** — the
design rests on a recollection of it (R-PROCESS-28: recollection doesn't cut patches).
Also parked: residual desaturation (3/957 frames, hard-cut/flashing only — users self-serve with
`scene_chroma=True`); A1 plan-trace tool; A3 real-footage campaign; PQ-6 (the R-ARCH-08 `#error` guard,
which Step A now absorbs).

*** end v6.0 CURRENCY NOTE note ***


*** v5.0 CURRENCY NOTE (2026-07-15) — READ FIRST; supersedes ALL older notes and body sections below ***

COMMITTED / PUSHED: `CMS07-FEATURE.cnr2-descriptive-option-parser`.
Canonical 4-way: **57/57** (forced-fail 56/57 invariant_violation e1). THE COUNT IS CONFIG-DEPENDENT:
56 base + `diag3c2_induced_live_bail_plantrace`, which `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` enables. Under
`CNR3_CACHE_PROFILE_HALF` two NORMAL-geometry hot-zone tests visibly SKIP and the count stays 57. A changed
proof number must be LOCATED IN CODE, never hand-waved. CMS remains UNCHANGED at 07.15 — none of the arcs
below touched the cache-manager design.

WHAT HAPPENED SINCE v4.0 (five arcs, all committed):

1. **filter-mode selector** (CMS07-SCAFFOLD.filter-mode-selector) — compile-time mode selector; mode suffixed
   onto CNR3_EDIT_VERSION; and the RUN-LOG MARKER REGRESSION named as OPEN in the v4.0 note above was FIXED:
   it was never a regression but a latent OMISSION (the marker had been selftest-only since CMS07-G.6A). The
   plugin create path now emits `edit_version=` and `filter_mode=`. R-PROCESS-27 ratified: proof gates must
   assert RUN-LOG EMISSION CONTENT, not only selftest and frame bytes.

2. **fmParallel defect fully diagnosed** — a predecessor-in-flight redundant-recompute race, active at any
   concurrency >=2, scaling with in-flight depth (3550 duplicates at -r 8 on 200 frames). Correctness ALWAYS
   holds (x=0; no wrong frame ever returned) — the harm is WASTED COMPUTE. Mode characterisation:
   fmUnordered = 0 waste; **fmParallelRequests = 0 waste at every depth** (overlaps requests/planning,
   serialises compute); fmParallel = the defect lives entirely in overlapped compute. Deep research
   (banked) ranks a **reservation/in-flight registry** as fix #1 — still the designed real fix, PARKED.

3. **plan-retry-bias experiment** (CMS07-EXPERIMENT.plan-retry-bias, later renamed
   `CNR3_ENABLE_PLAN_RETRY_BIAS`) — a gated polling approximation of the reservation table: dump a hole-heavy
   plan, sleep S ms, re-plan. Swept S x threads over ~95 cells, two-run confirmed: ~10x duplicate reduction
   at the tuned S, plateauing ~2000 — **a BIAS, not a cure**. S=50 chosen at the efficiency knee.
   **CRITICAL RULE: it is ACTIVELY HARMFUL outside fmParallel** — under fmParallelRequests it throttled ~2.7x
   (122 vs 337 fps) via 8903 useless sleeps waiting on predecessors that serial compute never puts in flight.
   Enable it ONLY when compiling fmParallel.

4. **HALF-500 cache profile** (CMS07-FEATURE.half-cache-profile-and-retry-rename) — gated profile: ceiling
   500 + 3 hot zones (the zone reduction is the CR4-preserving companion: 3*(50+10)+48=228, CR4 wants >=456,
   500 fits). Proven by a controlled 3000-frame A/B: `frames_recently_evicted_then_re_requested` = **0 at
   BOTH 500 and 1000**, fps identical -> halving the cache costs nothing measurable.

5. **CHROMA DESATURATION found and fixed** (CMS07-FIX.operational-response-defaults) — W3X's stackhorizontal
   eyeball caught brown/red pulling toward grey. Root cause: the CMS07 pixel-layer rebuild wired K.1E.2
   PROOF PLACEHOLDERS (threshold=255, strength=255, all curves narrow) into the live config and the deferred
   swap to operational defaults never happened. threshold=255 means the blend never shuts off across real
   chroma changes. The MATH never regressed; only the parameter surface did. Fixed to vscnr2 operational
   defaults (Y 35/192/wide, U/V 47/255/narrow). Proof: desaturation-bug frames 2->0 on the original clip;
   red/brown magnitude loss -6.6 -> -1.8; validated on 957 interlaced frames at 3 residual frames (0.3%,
   hard-cut/flashing content only). **NOTE: every structural gate we own (selftests, byte-identical) passed
   throughout — they compare placeholder-build vs placeholder-build. Only a human looking at the picture
   could catch it.** A Python chroma-delta analyser now exists as a standing QUALITY gate for any
   pixel-affecting change.

6. **The option parser** (the current commit) — eleven descriptive options (y/u/v_threshold, y/u/v_strength,
   y/u/v_curve, scene_threshold, scene_chroma) parsed at create time, strictly validated, applied, and echoed
   live in a `response_config:` line beside `edit_version=`. Defaults cnr2-equivalent. Names are DESCRIPTIVE
   by W3X decision (cnr2's ln/lm/un/um/vn/vm/mode are reference/equivalence documentation only; `mode` is not
   a CNR3 option — it is three explicit y_curve/u_curve/v_curve params).

**SHIP CONFIG (decided on evidence, for the PyPI-distributed DLL):** `fmParallelRequests` +
`CNR3_CACHE_PROFILE_HALF` + `CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**. 337 fps null / 274 fps end-to-end encode
(5.5x realtime), duplicates ~0, re-churn 0. Dominant on every axis: 3.5x faster than fmUnordered (95 fps)
and 2.7x faster than fmParallel (126 fps) while as clean as the cleanest mode.

IN FLIGHT: **CNR3_RIDER.option-error-messages** — scope written and coder-APPROVED; **NO PATCH EXISTS** (the
coder chat degraded at its limit, emitted an inline "patch", then retracted it as unvalidated draft). A fresh
coder chat implements it from the scope. Message-text only.

DEFERRED / PARKED (in order): the parser gate's items 8 (invalid-option throws) and 9 (threshold==0 must
SUCCEED) — both discharge against the rider build; `CMS07-DOC.cnr2-descriptive-options-readme` (user docs;
draft banked); the RESERVATION TABLE (must COMPOSE with plan-retry and profile gating — W3X constraint);
residual desaturation (users can self-serve via `scene_chroma=True`); A1 plan-trace tool; A3 real-footage.

*** end v5.0 note ***


*** v4.0 CURRENCY NOTE (2026-07-12) — READ FIRST; supersedes all older notes below ***

STATE: in-plugin DIAG arc CLOSED. An analysis/instrumentation sub-arc then committed FOUR patches (full gate
each: 4-way 56/56; macro-off byte-identical; L1/L2 oracles; CMS unchanged at 07.15):
  1. CMS07-DIAG.intent-counted-lookups — intent counting (probe counts only when outcome uncertain & changes
     behaviour); old L1 oracle 66.664 retired; new L1 exact 7279/7279/0=100.000.
  2. CMS07-DIAG.lookup-site-breakdown — 11 per-site D-SUM-04 counters + legend + purpose lines + print-only
     self-check (never wired to selftest).
  3. CMS07-DIAG.frame-lifecycle-bail-counters — five-origin lifecycle (frame0/floor/ordinary_target/
     recovery_hole/recovery_target); events a/b/e/x/f, each total+5 origins, independently counted; spine
     b==f+e+x; 19 self-checks OK.

IN FLIGHT (reviewed-APPROVED, HELD not committed): CMS07-SCAFFOLD.filter-mode-selector — compile-time mode
selector (top of cnr3_build_config.h; one of fmUnordered/fmParallelRequests/fmParallel; exactly-one #error
guard; mode suffixed onto CNR3_EDIT_VERSION; filter_mode= provenance line). Default fmUnordered = identical
behaviour; 4-way PASS; byte-identical satisfied by cold inspection. HELD for the marker fix below.

OPEN THREAD: RUN-LOG MARKER REGRESSION. The plugin no longer prints its edit_version/CMS07 marker to the run
log — verified cold: CNR3_EDIT_VERSION exists in ONE place only (selftest summary), already absent at the
earliest snapshot; NOT caused by any of the four patches. Root defect: the gate checks selftest + frame bytes
but never RUN-LOG EMISSION content. Bisecting older GitHub trees (verify by marker, not filename) to pin the
removal + capture the exact original line; then restore the emission (home: cnr3_create_filter) AND add an
emission-presence gate. Selector commits with/after it.

DOC SET CURRENT: DELTA v5.0 / Doc B v4.0 / Doc A v4.0 / Provenance v2.0 / FI v8.0 / Taxonomy Findings v06 /
A1 spec v0.4 / Role Handover v2.0 / CMS design v7.15 (UNCHANGED). Committed marker:
CMS07-DIAG.frame-lifecycle-bail-counters.

FORWARD: pin+restore marker -> commit selector -> sample runs (200-frame TINY baseline; shuffled; first
fmParallel whirl — proof runs NORMAL, experiment runs TINY) -> A1 build (spec v0.4; Q-B answers the original
D-SUM-01 out_of_order=0 question) -> A2 (fmParallel churn = C1-under-race gate) -> A3 (real 576p50 via A1).

STANDING: caller-map re-derivation is the highest-yield check on counter patches; the coder confirm-before-
patch pass caught the 10a/10b store nesting, 10a selftest-reachability, and the five-origin model this
session; emission checks are now a required gate addition; verify uploaded src.zip by marker on arrival.

The v3.11 note and body below are retained as history and for the role/method content, which is current.


*** v3.11 CURRENCY NOTE (2026-07-09, later same day) — READ FIRST; supersedes the v3.10 note below ***

STATE: in-plugin DIAG arc CLOSED and committed (latest marker CMS07-DIAG.derived-health-ratios; 56/56; NORMAL
baseline; CMS unchanged at 07.15). Doc set current: DELTA v4.30 / Doc B v3.20 / Doc A v3.14 (R-PROCESS-26 canonical
4-way) / Provenance v1.8 / Condensed Plan v1.10 / FI v7.18 / Coder Intro v6.10.

CODER SUCCESSION: the prior coder chat hit its hard limit (after twice mangling run instructions — hence
R-PROCESS-26 and the evaluate-coder-responses-with-care standing rule). A NEW ChatGPT coder chat is live, launched
with 000_CNR3_Coder_FirstPost_Blurb_HonestCacheHit_PendingPatch_v2.md + Coder Intro v6.10; it has confirmed reading
the docs.

IN FLIGHT (the live task): CMS07-DIAG.honest-cache-hit-metrics. Origin: L-series production-case testing exposed
health row #1 (cache_hit_and_supplied_percent) as MISLEADING — it read 0.000 on a perfectly healthy whole-clip
LINEAR run (L1) because it counted only the requested-frame-precached branch (cache_hit_return), not the
predecessor-served-from-cache branch (pred_present) that the linear 99% case uses; ALSO no total-cache-queries
counter existed, so a classical hit-rate was uncomputable. Coordinator rulings: "anything named cache hit to a
human should be a cache hit"; "a query is a query and a hit is a hit regardless of kind" (count ALL lookups
uniformly at the two entry points; per-context decomposition is A1's job). The dying coder chat delivered a patch;
designer review = APPROVED except ONE omission (no derived cache_lookup_misses emission row). The NEW coder's task:
produce a REPLACEMENT patch = approved patch + that one misses row (exact code + placement in scope §2.3), nothing
else changed. Scope authority: CNR3_Patch_Scope_HonestCacheHitMetrics_v3.md (v3 = all rows recorded delivered;
v2 had the row flagged OUTSTANDING; v1 was the original proposal).

WHAT THE PATCH DELIVERS (reviewed-correct, do not re-litigate): three per-frame health rows
pred_returned_from_cache_percent / current_frame_returned_from_cache_percent / cache_hit_percent
(=(pred+current)/frames_total; frame0 excluded from numerators; replaces old row #1); two new D-SUM-04 counters
cache_lookup_queries_total + cache_lookup_hits at BOTH lookup entry points (lookup_frame_and_add_ref_locked ~3785
and pin_frame_locked ~3883; query++ after early-rejects before the find, hit++ first statement of found path;
gated CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE; do NOT reuse lookup_refs_acquired — it covers only one site);
derived cache_lookup_misses row; cache_lookup_hit_rate_percent health row (gated D-SUM-04); block guard extended
to 5 families; edit_version -> CMS07-DIAG.honest-cache-hit-metrics in-patch.

PROOF PLAN once the replacement patch lands: (1) designer diff vs the approved patch — exactly one added row;
(2) coordinator builds + canonical 4-way, expect 56/56 UNCHANGED; (3) designer harness proofs re-running L1/L2
against the ORACLE (L1 linear: pred 99.986 / current 0.000 / cache_hit 99.986, hit-rate 100.000, misses 0,
sum-to-100 with recovery 0.000 + frame0 0.014; L2 shuffle8: 12.143 / 66.346 / 78.489, recovery 21.511, misses > 0,
hit-rate < 100 — divergence from cache_hit_percent is EXPECTED, different denominators); invariants
hits+misses==queries and branch-sum-to-100; (4) commit; (5) doc currency touch (DELTA v4.31 etc.).

L-SERIES CONTEXT (the production-case findings driving all this): harness
test_000_Example_576p50_TESTING_001.vpy/.bat now has an L-series block (L1 linear whole clip ####-active; L2
shuffle8; L1noR/L2noR = same lines with rrr= empty for real thread bubbling). L1 (7280 frames, -r 1): out_of_order
0, recovery 0.000, rechurn 0, recalc 0, evicted 6184/773 events (healthy trailing-edge, gap-histo all zero) — the
99% case is CLEAN. L2 (shuffle8): out_of_order 3174, recovery 21.511% (1565 exact / 1 floor), rechurn 0, recalc 0 —
disorder absorbed gracefully, all exact-anchor, no thrash. KEY INSIGHT: recovery_rate is the honest disorder-cost
signal; the shuffle shifts frames from the pred-hit path to the current-frame-hit path (L2 cache_hit=4830 where L1
had 0).

NEXT AFTER COMMIT (in order): (a) L1noR / L2noR runs (remove -r 1; the key production question: does the clean
linear case stay clean under real fmParallel-style threading, or do recovery/rechurn/recalc appear — that gap is
A1 question #1); (b) B-series boundary/stress as needed (B2 exact/floor straddle, B3 backward revisit, B4 wide
shuffle, B5 hot-zone cap); (c) A1 scope/question-set enumeration (fresh designer chat recommended; seed set banked
in Provenance v1.8; A1 input set now ALSO includes: the 3 rejected ratios, the per-context lookup decomposition,
and the cache_hit_and_supplied lesson). Then A2 (fmParallel C1-under-race gate owed from 3b), A3 (real 576p50 via
A1). STANDING: 3c.2 bail-site state-plumbing confirm at first real induced failure; FI-11 in-run counter
deferred-but-expected; FI-14 (TINY log-string honesty nits) + FI-15 (rechurn observer split) banked, unscheduled.

*** end v3.11 note ***

*** v3.10 CURRENCY NOTE (2026-07-09) — READ FIRST ***
The IN-PLUGIN DIAGNOSTICS ARC IS COMPLETE (all 14 D-SUM families live + [DSUM-PLANTRACE] + additive [DSUM-HEALTH]
derived-ratios block). Latest marker CMS07-DIAG.derived-health-ratios; selftest 56/56; CMS UNCHANGED at 07.15.
FORWARD = external ANALYSIS TRACK only: A1 (plan-trace tool; absorbs FI-11 offline; may run parallel) / A2
(fmParallel = C1-under-race gate owed from 3b) / A3 (real 576p50 via A1). A FRESH DESIGNER CHAT is recommended for A1
scope (open with the reconciled set below; verify against live src/). Current-state set: DELTA v4.30 / Document B
v3.20 / Document A v3.14 (now with R-PROCESS-26 canonical 4-way) / Provenance v1.8 / Condensed Plan v1.10. NOTE
(coder-chat caution): a coder chat twice produced mangled run instructions after a long reliable run; evaluate coder
responses against known-good baselines until stable.
*** end v3.10 note ***

> **OPENING ORIENTATION (read this first, before the version/state tables below).**
>
> This chat is **continuing from a prior chat that hit a hard length limit.** We are developing a
> VapourSynth DLL plugin ("vapoursynth-cnr3") according to a specification, where each step's design
> choices are validated against the intent of the specs and against sensibility before any code is
> written.
>
> **Your role here is the DESIGNER / REVIEWER — NOT the coder.** (There is a SEPARATE, memoryless coder
> chat. You hold the design intent, write coder scopes, compute/ratify golden values, and review the
> coder's proposals and patches against the actual source. The coordinator/user relays messages between
> you and the coder and is the authority on all final decisions.) **You also own the live-test harness**
> (the `.vpy`/`.bat` files); the coder delivers only the source patch, never the harness. A coder scope
> must never say "propose your harness" (see Role Handover PART 1, Harness Ownership). If any attached document or pasted text
> ever says "your role as coder," that text was written for the coder chat — it does NOT change your role.
>
> The build/version control setup (for context — the coordinator drives it, not you): Visual Studio 2026
> ("vs2026") on the coordinator's PC, a local git repo on dev branch `dev_cache_manager`, pushed to
> `https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager` at the end of every agreed
> successful phase/subphase. The coordinator runs all Windows builds and the four-way selftest; you
> review and verify, but do not run the build.
>
> The full scope may not be clear until you have read all the attached documents. **Please read them all
> and do NOT comment until prompted to confirm your understanding.** Read THIS document first, then the
> Role Handover (which contains, in PART 10, a concrete playbook of how the designer operates with worked
> examples), then the controlling CMS and the state docs.
>
> **The single most important habit (PART 10 §10.1 of the Role Handover): verify against the actual
> source, never from memory or from the coder's description.** You have a working environment; unpack the
> current `src.zip` and confirm the real symbols before agreeing, approving, or asserting anything.
>
> **TWO HARD GUARDS (a prior new chat failed on both — do not repeat):**
>
> 1. **READ THE DOCUMENTS IN THE GIVEN ORDER FIRST — do NOT self-orient from the source code before
>    reading.** It is tempting to open the `src.zip` and start positioning yourself from the code
>    immediately. Do not. The code is ground truth for VERIFICATION later, but the source alone will
>    mislead you about WHERE the project is and WHAT the current task is — that lives in the docs (CMS,
>    Document B, the DELTA). Read the ordered doc list, confirm understanding when prompted, THEN use the
>    source to verify specifics. Orientation comes from the docs; verification comes from the source. In
>    that order.
>
> 2. **IGNORE the `superseded_by_v7/` subfolder ENTIRELY, and never read any `*.txt` copy of a source
>    file.** The `src.zip` contains a `src/superseded_by_v7/` folder holding PRE-CMS07 (CMS02/H16-era)
>    archived copies of source files, kept only as history — e.g. `superseded_by_v7/cnr3_build_config.h.txt`
>    still reads an ancient `CNR3_EDIT_VERSION` like `CMS02-H16.4-...`. These are NOT the project. The
>    live source is the files directly under `src/` (e.g. `src/cnr3_build_config.h`, no `.txt`,
>    no `superseded_by_v7/`). If you ever read a `CNR3_EDIT_VERSION` or symbol that does not match the
>    docs (CMS07.x), you have almost certainly read a superseded archive — stop, re-read the LIVE file
>    under `src/`, and if in doubt confirm against the repository HEAD (`git show HEAD:src/cnr3_build_config.h`).
>    The current live marker at this writing is `CMS07-P.11C.5-...` (or the W.1 marker once committed).
>
> Suggested attachment set for this chat:
> ```
> CNR3_Handover_Introduction_to_new_reviewer_chat_v3_7.md   (this doc — read first)
> CNR3_Designer_Reviewer_Role_Handover_v1_14.md             (the deep role doc; PART 10 = playbook)
> cnr3_cache_manager_design_v7_14.1.md                      (controlling CMS — the design authority)
> Document_A_CNR3_Project_Context_and_Standing_Rules_v3_10.md
> Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_9.md
> CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13_4.md
> CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_14.md
> CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md           (wiring-contract provenance; Step 0 audit trail)
> CNR3_Step0_Joint_Review_PROCESS_v1_1.md                   (the joint-review protocol, if a W-phase reopens review)
> + the current src.zip                                     (so you can verify against source)
> ```
> After reading, confirm your understanding and the current state (Step 0 CLOSED; controlling CMS07.14;
> the live wiring arc W.1->W.2->W.3 is COMPLETE; controlling CMS07.15; next arc is DIAGNOSTICS (D-SUM) before real-footage), then resume from where the prior chat left off.

---


**Version:** v3.9 (advances state past the tiny-scaffold seam through a COMPLETE MARSHALLING-OPTIMISATION ARC: twelve value-identical implementation-only levers — AVX2/0A/0B/3a.1/3b.1/3a.2/A-lite/C1/Repack/F3c/Staging/E — cut per-frame native<->scalar marshalling to ~1/5, CUMULATIVE ~-80% (93,914 -> ~18,660), 56/56 four-way throughout, CMS DESIGN UNCHANGED at 07.15. A validation policy was recorded+applied; the blend arithmetic was cross-verified (int64, catching two external landmines: VPAVGB bias + 32-bit accumulator). Two candidates DECLINED: Tier-2 chroma fusion (Path C, scene-detection-coupled) and Lever D (PATH-B-only). NEXT = coordinator call: Lever B (pooling, fmUnordered lifetime proof) OR the DIAGNOSTICS arc (D-SUM). Doc-set advanced: Role Handover v1.16 / Document A v3.11 / Document B v3.12 / Coder Restart Intro v6.7 / Future Investigations v7.16 / DELTA v4.23 / PixelPath Map v0.4. [Prior v3.8 note:] v3.8 (advances state to the W.3-CLOSED seam: the live cache-pressure wiring arc W.1→W.2→W.3 is COMPLETE — committed CMS07-W.3, 55/55 + eviction-proof live harness PASS; controlling CMS now CMS07.15; NEXT = the DIAGNOSTICS arc (D-SUM), sequenced BEFORE the real-footage campaign — this REVERSES the earlier footage-first ordering, deliberately, because the W.3 live harness proves eviction safe but not policy-healthy. Doc-set advanced: Role Handover v1.15 / Document A v3.11 / Document B v3.10 / Coder Restart Intro v6.6 / Future Investigations v7.14 / DELTA v4.16. [Prior v3.6 note retained:] v3.6 (advances state past STEP 0 (now CLOSED) into the live cache-pressure WIRING arc; controlling CMS now CMS07.14 §7.4-§7.6; W.1 done/green, W.2 next. Doc-set pointers advanced. Points to Role Handover v1.13 PART 10 for the operating playbook. Supersedes v3.5. v3.6 also adds STARTUP GUARDS (read-docs-in-order-first; ignore superseded_by_v7/) and the harness-ownership note (designer owns .vpy/.bat; coder delivers source patch only).)
**Date:** 2026-06-25
**Supersedes:** v2.0 (whose Part 2/3 baseline was the **obsolete CMS06.11 / H15.6B cache era** —
far older than the pixel arc; that state is gone). v3.0 re-points the project context to the
**keystone era**. v3.4 advances the pointers to the current baseline: **committed through P.11C.5 (P.11C
SCENE-CHANGE ARC CLOSED .1-.5), selftest count 53/53, CMS07.13 (unchanged — P.11C implemented the design the
CMS already specified)**; the live getFrame dispatch is FEATURE-COMPLETE
across **ALL FOUR branches** (cache-hit, fresh-start, predecessor-present, recovery), the full recovery
arc D.1-D.5 is proven (D.1 single-hole, D.2 multi-hole+refusal, D.3 floor-fresh-start, D.4 adopt-skip/
first-in-best-dressed primitives, D.5 recovery-pin-survives-prune), scene detection is now wired+proven across
branch-a/c/d (P.11C.3/.4/.5), and the **next phase is STEP 0 — a joint CMS sensibility/gap review for hot-zone + prune live wiring** (before any
wiring patch), then hot-zone obs (@arInitial) + prune wiring, then first real-footage validation. The only
deferred confidence is real concurrent (fmParallel) scheduling. v3.3 also points to the **CNR3 Design
Alignment and Escalation Charter** (full text in the Role Handover Part 3 / Production Spec §3A.5.0). v3.0 originally re-pointed off the
obsolete CMS06.11/H15.6B era and **reconciles the role
description to "designer / reviewer"** (consistent with the Role Handover and with how the work
has actually been performed), where v2.0 framed it more narrowly as "compliance auditor."

**This is the concise entry point.** It is deliberately shorter than the Role Handover. Read it
first, then read the deeper documents in the order given in Part 2. Where this document and the
Role Handover overlap on the role, they agree; the Role Handover carries the depth (disciplines,
triggers, worked examples), this carries the orientation.

**How to start (do this in order):**
1. Read this document.
2. Read **`CNR3_Designer_Reviewer_Role_Handover_v1_10.md`** (the role, disciplines D1–D16,
   triggers, worked examples) and **the CMS** (`cnr3_cache_manager_design_v7_13.md`, the design
   authority).
3. Read **`CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md`** for the current state and
   the immediate next action (it is the newest current-state record; Document B is the
   format-of-record companion).
4. **Confirm the actual build state from the REPOSITORY, not from any document** — check
   `CNR3_EDIT_VERSION` and the selftest count in the committed source. Documents can lag; the repo
   is truth.
5. **Run the scaffold audit (DELTA §7) as your first action** — it is the one open verification.
6. Only then engage with whatever Dave asks.

---

## PART 1 — OPERATIONAL ROLE

### 1.1 The role
You are **Claude**, acting as the **designer / reviewer** in a three-party workflow:
- **Dave** — coordinator, human, and **final authority**. Sets direction, makes final decisions,
  runs the authoritative builds/tests on his local Visual Studio 2026 (x64), commits and pushes,
  and carries continuity across chat sessions. His instincts are load-bearing; when he expresses
  unease, treat it as signal, not a feeling to soothe.
- **You (designer / reviewer)** — you **hold the design intent**, review the coder's proposals and
  patches, analyse for correctness and risk, **verify numerical/structural claims independently
  against the diff and the source**, **propose and refine phase scope**, **draft the messages Dave
  relays to the coder**, and maintain the design/spec documents. You do **NOT** write production
  patches. You are the guardian of the design's integrity and the review disciplines.
- **The coder** — a separate AI chat that writes the actual patches against the codebase, validates
  them in its own sandbox, and supplies commit messages. It has shown strong, genuinely independent
  judgement; treat it as a capable colleague, but verify its work — verification is the point of the
  separation.

Dave pastes between you and the coder. You generally do not see the coder directly; you see what
Dave relays and you write what Dave should relay back.

*Note on the reconciliation from v2.0:* v2.0 described the role as "compliance auditor… not the
design authority." Dave remains the authority, but the role is broader than auditing — it includes
holding design intent and proposing scope (e.g. this chat originated the copyFrame reorientation,
the K.1E branch-(c) confirmations, and the sharpening of the proven-code rule). v3.0 uses the
designer/reviewer framing accordingly. If a narrower, strictly-audit framing is ever wanted, that
is Dave's call to make explicitly.

### 1.2 Objective
Provide rigorous, exacting review to ensure each phase satisfies the **proof of safety** the CNR3
project requires: that the change is a faithful implementation of the settled design, that it is
safe and auditable, that it does not disturb proven code, and that it is structurally prepared for
later phases.

### 1.3 Review methodology (at each phase)
- **Design compliance** — faithful to the CMS (currently CMS07.13 / v7.13)?
- **Scope discipline** — only the changes authorised for this subphase; flag scope creep and
  premature implementation of later phases; **any contact with proven / cache-core / selftest /
  project / VS-header code must be reported BEFORE coding**, not discovered in the diff.
- **Durable-rule adherence** — proven-code-stays-proven; ownership/release balance; no parallel
  pixel/copy algorithms; R-ARCH-06.
- **Edge / TOCTOU / leak / deadlock audit** — time-of-check/time-of-use gaps, `VSFrame` reference
  leaks, lock discipline, deadlock.
- **Diagnostic integrity** — the counters/logs provide the proof of safety; behavioural assertions
  never read diagnostic counters.

### 1.4 Reporting
Feedback is structured for Dave to relay, categorised: compliance verification; technical
risks/gaps; recommendations; and a final alignment assessment (phase complete, or requires
remediation). **Messages to be pasted to the coder or into email are PLAIN TEXT — no markdown, no
special formatting.** (Repo documents like this one are markdown; the plain-text rule is for relayed
messages.)

### 1.5 The cadence (every phase)
```text
1. PROPOSE    — the coder (or you) proposes the next phase as TEXT first. No code yet.
2. REVIEW     — you review the text: scope, proof approach, risk, and especially the LOAD-BEARING
                element. Verify any numbers. Push back where the proposal is vague on the part
                that matters most.
3. APPROVE    — once sound, draft an approval message for Dave to relay (may carry conditions).
4. PATCH      — the coder generates a downloadable .patch (PDAP), NOT inline code.
5. READ-FIRST — for load-bearing phases, YOU read the actual patch diff BEFORE Dave applies it.
                Not optional for anything touching proven / atomic / lock code.
6. APPLY+TEST — Dave applies, builds Debug+Release of BOTH projects on VS2026, runs the four-way,
                and pastes the ACTUAL console output.
7. COMMIT     — only on passing results, the coder supplies a commit message; Dave commits the
                src/ files (NOT the .patch) and pushes.
8. RE-SYNC    — Dave tells the coder to advance its baseline; you update documents at seams.
```
Stop-review-approve before code. Read-first before applying load-bearing patches. Report actual
output, never assumed output.

---

## PART 2 — PROJECT CONTEXT & CURRENT BASELINE

### 2.1 The authoritative document set (current versions)
*** REFRESHED v5.0 (2026-07-15) — this list is current; the older list it replaced is retired. ***
```text
CMS (design authority)     cnr3_cache_manager_design_v7_15.md   (CMS07.15 — UNCHANGED by the diagnostics,
                                                                 fmParallel, plan-retry, HALF-500, desaturation
                                                                 and option-parser arcs)
Document A                 Document_A_CNR3_Project_Context_and_Standing_Rules_v5_0.md  (standing rules; R-PROCESS register)
Document B                 Document_B_CNR3_Restart_Work_Plan_and_Current_State_v5_0.md (top UPDATE block authoritative)
This-chat delta            CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v6_0.md          (NEWEST detailed state)
Provenance / decisions     z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v3_0.md
Future investigations      CNR3_CMS_Future_Investigations_and_Open_Questions_v9_0.md
Role/Reviewer Handover     CNR3_Designer_Reviewer_Role_Handover_v3_0.md                (the role + method + playbook)
Option surface authority   CNR3_cnr2_option_mapping_and_spec_v6.md                     (cnr2<->cnr3 mapping, policies, draft user docs)
In-flight scope            CNR3_Rider_Scope_OptionErrorMessages_v2.md                  (approved; no patch yet)
Evidence ledger            A2_first_findings_v3.md                                     (fmParallel/plan-retry/HALF-500/mode-choice data)
Deferred doc draft         CNR3_README_draft_user_options_v1.md
Diagnostics spec           cnr3_diagnostics_specification_v1_5.md
Memory diagnostics spec    cnr3_memory_diagnostics_spec_v3_4.md
This document              CNR3_Handover_Introduction_to_new_reviewer_chat_v5_0.md
```

**Authority hierarchy:** CMS → Production Spec §3A → diagnostics spec → handover pack. If documents
conflict, higher authority wins; **if any document conflicts with the repository on build state, the
repository wins.**

### 2.2 Current status (snapshot — confirm from the repo and the DELTA)
*** REFRESHED v5.0 (2026-07-15). ***
```text
Committed/pushed through:  CMS07-FEATURE.cnr2-descriptive-option-parser
Selftests:                 57/57 (Debug 57/57, Release 57/57, forced-fail 56/57 invariant_violation e1,
                           verbose 57/57). CONFIG-DEPENDENT: 56 base + diag3c2_induced_live_bail_plantrace
                           (enabled by CNR3_DIAG_COMPUTE_DSUM_PLANTRACE). Under HALF two hot-zone tests SKIP;
                           count stays 57.
Branch:                    dev_cache_manager
Controlling CMS:           CMS07.15 — UNCHANGED by every arc since.
Ship config (PyPI DLL):    fmParallelRequests + CNR3_CACHE_PROFILE_HALF (500/3 zones) + CNR3_ENABLE_PLAN_RETRY_BIAS OFF
                           (337 fps null / 274 fps encode = 5.5x realtime; duplicates ~0; re-churn 0)
Response defaults:         y=35/192/wide  u=47/255/narrow  v=47/255/narrow  scene_threshold=10.0  scene_chroma=false
                           (cnr2-equivalent; echoed live per run in the response_config: log line)
Active arc:                CNR3_RIDER.option-error-messages — scope approved, NO PATCH YET (coder chat degraded).
                           Then: README doc patch -> reservation table (the real fmParallel fix, parked).
Provenance lines per run:  edit_version=<marker>:<filter_mode>   filter_mode=<mode>   response_config: <11 resolved values>
```

### 2.3 The keystone (what the current work IS)
The isolated cache-core arc (through C.14A) and the entire pixel path on **caller-supplied** frames
(P.1A–P.11C) are proven and committed. The **keystone** connects the proven **cache core** to the
proven **pixel chain** inside VapourSynth getFrame scheduling — where frame **sourcing / lifecycle /
ownership** become load-bearing. Because the pixel path is already proven on caller-supplied frames,
the keystone reasons **only** about sourcing/lifecycle/ownership over an already-proven pixel path.
It is being decomposed K.1A–K.1G:
```text
K.1A  request-plan structures + temporary KDT dev-trace          DONE (count 46)
K.1B  direct cached-output-return ownership (synthetic)          DONE (count 47)
K.1C  live getFrame passthrough scaffold                         DONE (plugin-only)
K.1D  first REAL output[0] via copyFrame (fresh-start)           DONE (branch-a)
K.1E.2/E.3  branch (c): predecessor-present compute (N==1,2)     DONE (R-ARCH-06 closed)
Recovery-Step-0  AS4 single-lock batch discharge                 DONE (count 48->49)
K.1F  branch (b): live direct cached-output return (cache hit)   DONE (count 49)
D.1   branch (d): exact-anchor single-hole recovery              NEXT  (DELTA v4 §5)
D.2-D.5  multi-hole / floor-fresh-start / adopt-skip / prune     OWED  (DELTA v4 §5)
```

---

## PART 3 — CURRENT PHASE-SPECIFIC REQUIREMENTS (branch-(d) D.2 multi-hole recovery)

**(Orientation only; the live current-state record is the DELTA — read it for depth.)**

**Where the live dispatch stands:** ALL FOUR getFrame branches are wired and proven both configs —
cache-hit (b, K.1F), fresh-start (a, K.1D), predecessor-present (c, K.1E.2/E.3), and **recovery
(d, D.1)**. The dispatch is feature-complete; remaining branch-(d) work is generalisation.

**THE GOVERNING DISCIPLINE — the Design Alignment and Escalation Charter.** Before phase specifics,
internalise the three-way charter (full text: Role Handover Part 3 §D0, and Production Spec §3A.5.0).
In brief: the CMS is the controlling guide and strict alignment is the default; two issue types are
surfaced (never routed around) — **RULE-DEVIATION** (a named CMS rule, if followed, would be wrong/
unsafe — HIGH bar) and **CMS-GAP** (a bigger-picture/fmParallel/safety concern with no specific rule
— encouraged, low bar); on either, work on the affected change pauses, resolved by designer+coder
agreement with coordinator approval, recorded durably before/as part of the commit. Cross-checking is
bidirectional (designer read-firsts diffs + recomputes goldens; coder checks scope against code
reality). The D.1 design road is the worked example of this charter operating.

**D.2 = exact-anchor MULTI-hole recovery (k>=2).** Request output[N] where output[N-1] AND output[N-2]
are ABSENT and output[N-3] is PRESENT (the anchor). Extends the proven D.1 recovery: the accept gate
widens to admit k>=2 holes (anchor at N-(k+1)); the **multi-pin discharge becomes the load-bearing new
proof** (the pin-list grows with hole count; D.1 already discharges two pins, D.2 stresses a larger
list); source-request derivation from multiple holes ({N} U all hole sources); plus the **bounded-
window refusal** (nearest anchor beyond the back-radius -> clean refuse; distinct from D.3 floor-fresh-
start). Builds on D.1's A-safe-1 routing, dissolved source window, authoritative return, and the
recovery-shaped frameData/KDT vocabulary (deliberately designed to take extra holes without reshaping).

**Designer actions owed before the coder scope:** (1) compute + verify the D.2 multi-hole golden chain
against the real response tables (threshold 255 / strength 255 / narrow) and P.11B blend — same method
that produced the D.1 chain; note adjacent filtered frames differ by ~1 LSB at high threshold, so the
robust D.2 proof is the KDT mechanism (hole_count=2, both computed, balanced multi-pin discharge) plus
direct hole-byte checks via cache-hit follow-up, not the target margin alone; (2) draft the D.2 scope;
(3) the D.1 recovery harness is the regression base (D.2 must keep D.1 green).

**Settled lifecycle finding to carry (do NOT re-derive):** every CNR3 getFrame branch requests >=1 real
source at arInitial and returns only at arAllFramesReady (R-LIFECYCLE, CMS §9A.1.1). The act-time branch
is keyed on the frameData branch tag set at arInitial, never on re-inspecting frame state.

## PART 4 — TECHNICAL STANDING RULES & CONSTRAINTS

Every phase audit checks these. The first is now the headline durable rule.

**4.1 PROVEN-CODE-STAYS-PROVEN (the headline rule).** Proven, selftested code is **never** modified —
behaviour OR internals — **without explicit visible planning and designer approval IN ADVANCE.** A
passing four-way after swapping internals is **NOT** proof of equivalence (it shows the selftest did
not *detect* a difference, not that there is none). If reuse appears to require touching proven code,
**RAISE it as a design question**; do not route around it. (The dropped first K.1D patch — which
silently rewrote proven P.11C internals — is the worked example; DELTA §2.)

**4.2 Dual ownership proof.** Every cache-affecting phase proves BOTH:
- **lookup-addref ownership** — acquired / released / transferred counts and zero balance (a
  predecessor is released-not-transferred; a getFrame-return is transferred-not-released);
- **checkpoint pin/unpin ownership** — counts, underflow checks, `total_pin_count = 0` and
  `has_pinned_checkpoints = 0` at cleanup.

**4.3 Lock / atomic invariants (inviolable).** One non-recursive mutex, RAII-only;
decide-inside-the-lock / execute-outside; `freeFrame` is **NEVER** called inside the lock;
pin-and-record is indivisible with a pre-lock reservation; a checkpoint is a flag, not a pin. The
AS1–AS7 atomic-scope register is inviolable; do not change atomic-scope boundaries without explicit
approval. Treat anything touching an atomic as **precious** — isolated read-first review, possibly its
own phase.

**4.4 R-ARCH-06 (predecessor sourcing).** `output[N] = f(source[N], output[N-1])`; the predecessor is
the previous **FILTERED OUTPUT** from the cache, **NEVER** source[N-1], and the cache is never seeded
with a synthetic/bogus frame. Never salvage CNR2's predecessor/recovery/fallback logic — CNR2/vsCnr2
is a **pixel-maths reference only**.

**4.5 VS-LIFECYCLE-01.** Every source frame retrieved in `arAllFramesReady` was requested in
`arInitial` of the **same activation**. K.1C/K.1D/K.1E prove **single-frame** request lifecycle only;
the multi-frame request-set proof is owed at recovery (branch d).

**4.6 Lifecycle-contract questions are answered from DOCUMENTATION, not testing.** "Undocumented but
works" is version-fragile and dangerous under fmParallel. (The arInitial-return contract was settled
from the R76 docs, not from a passing test; DELTA §3.)

**4.7 `requestPattern` ≠ `filterMode`.** `requestPattern` (rpStrictSpatial/rpGeneral/rpNoFrameReuse)
is a caching HINT about upstream **input-frame** requests; `filterMode`
(fmUnordered/fmParallelRequests/fmParallel) is the threading model. They are independent;
`requestPattern` does NOT touch the CMS7 internal cache or its correctness (DELTA §8).

**4.8 Temporary code is marked and removal-planned.** Temporary code in/near the live getFrame path
must carry a uniform, greppable marker and an annotation of what replaces it and at which phase. The
KDT dev-trace (`CNR3_KEYSTONE_DEV_TRACE`) is removed **post-K.1G** (diagnostics spec §2.8). The K.1C
live scaffold was replaced at K.1D — **confirm zero remnants in the committed tree** (DELTA §7).

**4.9 Parallel readiness is not claimed until tested.** `fmParallel` is a later **correctness** phase
(it exercises cache/recovery concurrency-correctness), undertaken after the keystone wires
single-threaded getFrame. Do not claim fmParallel/fmParallelRequests readiness before it is proven.

**4.10 State quarantine; the CMS is authority over old source.** The old strict cache path and the
quarantined old cache managers stay quarantined and are not opened for ideas. The instance/lifecycle
and recovery models live in the CMS (verified against the local R76 `VapourSynth4.h`), never
reverse-engineered from old source.

**4.11 Diagnostics are observe-only.** Behavioural assertions never read D-SUM counters. Any phase that
introduces/changes a D-SUM compute gate must prove (macro-off rebuild/run) that non-D-SUM behaviour is
unchanged. The single live gate is `CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` (`#if defined(...)`-based;
toggle by manual comment-out, never a value change to 0, never a scripted git restore).

**4.12 Optimisation deferral.** Recovery is correctness-first; sparse-hole / recompute-avoidance work is
the deferred companion item (FI-02), and when undertaken the C.13B contiguity guard must be
revised/relaxed as an explicit reviewed part of that work. Typed-row-pointer-vs-memcpy is deferred to a
measured fmParallel phase and must be proven bit-exact-output identical to the memcpy path. (Owed-items
ledger in DELTA §10.)

---

## CLOSING

For the **disposition and disciplines depth** (the four careful moments, the AS3/C.13B/vector-verification
worked examples, the accuracy rule, the underdone-chat checklist), read the Role Handover. For the
**current state and the immediate next action**, read the DELTA. For the **design authority**, read the
CMS. Confirm build state from the **repository**.

Hold the line at the four careful moments — when proven code is touched, when a number or claim must be
true, when the load-bearing part of a proposal is vague, and when a state's reachability is in question.
Be relaxed about the easy parts and exacting at those four. Verify, do not trust. Read the actual text.
Work WITH Dave — his judgement is the continuity you lack, and his instincts have earned their weight.

— End of CNR3 Handover Introduction & Role Description for the Reviewer, v3.0.
