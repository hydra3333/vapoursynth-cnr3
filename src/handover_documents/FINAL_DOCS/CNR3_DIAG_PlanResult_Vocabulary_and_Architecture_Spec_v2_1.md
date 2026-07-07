# CNR3 — PLAN/RESULT DIAGNOSTIC (D-SUM PLAN-TRACE) — VOCABULARY + ARCHITECTURE SPEC v2.1

**From:** designer/reviewer (W3D), via coordinator (W3X).
**Status:** CONTROLLING DOCUMENT for the DIAG.3c scope. Supersedes SPEC v1 (a DRAFT for coder cross-check)
and SPEC v2 (superseded by this v2.1). The cross-check has happened
(`CNR3_DIAG3a_PlanResult_Cross_Check_Report_2026-07-04.md`); its findings are folded in here as RESOLVED.
Do NOT re-commission the cross-check.
**Revision v2.1 (supersedes v2):** applies fidelity-review finding **D-V2-1** — the bail-site total is **65
call sites**, not 66. The AR raw `cnr3_set_filter_error` grep of 51 includes the function DEFINITION at
`cnr3_arAllFramesReady.cpp:526` (not a bail site); AR call sites = 50, so 14 + 50 + 1 = 65. Corrected in the
Set 5 count block (§4), §7, §12, and §13(c). No other change: all six findings and all eight locked decisions
stand as in v2. Provenance: the cross-check's "51 sites plus the definition" phrasing conflated raw-count with
site-count; v2 propagated it in good faith (its raw grep counts were correct); the definition-exclusion is the
corrected step. The spec's own "counts are a snapshot; the authoritative table is built at 3c.2 scope time from
live source" discipline re-derives 65 at scope time regardless.
**Purpose:** a per-output-frame plan-vs-result diagnostic. For each requested output frame it records
(a) the PLAN arInitial produced (intent), and (b) the RESULT arAllFramesReady produced (outcome), as a
matched pair, emitted at end-of-run (or on failure bail) as clean, copy-pasteable, externally-sortable
blocks. Observe-only (R-PROCESS-19) except the bail-path writes isolated in §7/§12 (the 3c.2 invasive part).
**Grounding:** enums below derived from real source (`cnr3_arInitial.cpp`, `cnr3_arAllFramesReady.cpp`,
`cnr3_plugin_internal.h`, `cnr3_cache_core.h`). Set 1/3/4 outcome codes are source enums; the O-item roles,
R-item E/X, and the failure-reason categories are DERIVED (source-grounded, cross-checked — see §4/§13).
**Builds on:** DIAG.1 framework (`cnr3_diag_write_line`, `[DSUM-SUMMARY]` pattern, snapshot-outside-lock).

**Binding-intent note (read before revising this doc):** the design decisions in §5–§8 are not open for
silent revision. Each was an explicit coordinator ruling recorded in
`CNR3_Ring_and_PlanTrace_Design_Rationale_and_Intent_v1.md` (Part B), and each carries its reasoning and its
*rejected alternative* inline here so the "why" cannot be separated from the "what". To change any of them,
raise a proposal with reasoning (R-PROCESS-25) — do not merge a change in as if it were editorial.

---

## 0. What changed from v1 (and the 5-vs-6 finding-count correction)

v2 is a full revision, not a merge. Two classes of change, kept deliberately distinct because they carry
different authority:

- **RESOLVED FINDINGS (applied):** the coder cross-check produced **SIX** findings. The handoff note that
  commissioned v2 named five and folded the sixth under the spirit of its "carry-forward" directive rather
  than naming it. v2 records **all six as resolved** and states the count plainly, so no future reader (the
  coordinator, a next designer, or the 3c scoper) is tripped by a five-vs-six discrepancy between the note
  and this spec. The six are enumerated in §13; they are applied throughout §2/§4/§5/§7.
- **CARRIED-FORWARD DECISIONS (locked, reasoning bound in):** the eight rationale-doc decisions (§5–§8),
  preserved verbatim in intent with their reasoning and rejected alternatives attached in place.

The finding-count line, stated once for the record: **the cross-check has six findings, not five.** See §13.

---

## 1. Concept

Two records per in-window requested output frame:
- **O record (phase "O", plan_open):** written at arInitial EXIT — the plan (strategy + intent per item).
- **R record (phase "R", plan_result):** written at arAllFramesReady EXIT — the result (outcome + per item).

Records are buffered per-instance; dumped as clean sorted block(s) at end-of-run OR on failure bail (§8).
External tools/humans sort/pair via zero-padded keys.

**Pairing is by frame number, not by emission order.** Under fmUnordered/fmParallel the O and R records for
one frame are written at different times and interleaved with other frames' records; the dump pairs them by
frame. This is a property the format must serve, not an incidental detail (§5, §8 view (b)).

---

## 2. Window parameters

- **from_frame / to_frame** (inclusive) — OUTPUT frame-number bounds. A frame is recorded only if
  `from_frame <= n <= to_frame`, tested at each function EXIT. Frames outside the window are never recorded.
- Buffer preallocated for the window: up to `2 * (to_frame - from_frame + 1)` records (2 per frame).

**DECISION — from/to are COMPILE-TIME `#define`s for the first cut. [RESOLVED — cross-check finding (d)]**
- WHY / rejected alternative: runtime `.vpy` filter params were considered and deferred. Compile-time
  mirrors the existing diagnostic gate style, keeps the family strictly observe-only internals, and is far
  easier to prove under the R-PROCESS-19 macro-off gate. Runtime params would change the plugin API surface
  and broaden the patch beyond observe-only. Re-window = rebuild; acceptable for a diagnostic.
- Deferred, not rejected: runtime `.vpy` from/to MAY be added later once the diagnostic semantics are
  proven and if a no-rebuild workflow is wanted. If so, it is a separate, later, API-surface proposal.

**DECISION — windowed preallocated buffer; NO ring, NO saturation machinery. [LOCKED — rationale B.3.5]**
- WHY / rejected alternative: the buffer is window-bounded (`n` outside `[from,to]` is never recorded), so it
  cannot overflow — there is nothing to saturate. This is the deliberate CONTRAST with the D-SUM-10 ring and
  the D-SUM-13 recalculation table, which take *unbounded* inputs and therefore need the derived-capacity +
  saturation-honest discipline. Do not import ring/saturation machinery here; it would be dead complexity.

---

## 3. Record fields (both O and R)

```text
- phase              : "O" or "R"  (sortable; O sorts before R so a frame's open precedes its result)
- frame              : requested output frame N, ZERO-PADDED (e.g. 00000123) — sortable
- action_seq         : monotonic per-instance counter stamped at record write — ZERO-PADDED — sortable
                       (gives action/arrival order independent of frame number)
- enter_datetime     : function entry timestamp (see §6)
- exit_datetime      : function exit timestamp
- run_ms             : (exit - enter) in milliseconds, human-readable derived field
- frame_code         : O = strategy (Set 1); R = outcome (Set 3)
- plan/result body   : labelled lists (§5, human-primary) + appended machine codes (§5, parser)
- (R only) fail_reason: if frame_code == FAILED, the failure-reason category (Set 5)
```

---

## 4. THE FIVE ENUM SETS (the vocabulary — source-grounded, cross-checked)

### Set 1 — O-FRAME-LEVEL strategy (source: `Cnr3LiveGetFrameBranch` + `Cnr3LiveRecoveryBranch`)
Cross-check §1: matches source exactly.
```text
1 CACHE_HIT       cache_hit_return              frame already cached; pin + return, no compute
2 FRAME0          frame0_fresh_start            frame 0, no predecessor
3 PRED_PRESENT    predecessor_present_compute   predecessor N-1 present/pinned; compute directly
4 RECOVERY_EXACT  recovery + exact_anchor       predecessor absent; exact anchor in window; walk holes
5 RECOVERY_FLOOR  recovery + floor_fresh_start  predecessor absent, no anchor; fresh-start from floor
(0 NONE           none                          uninitialised/error guard — must not appear in a valid record)
```
Source symbols: `Cnr3LiveGetFrameBranch` { none, cache_hit_return, frame0_fresh_start,
predecessor_present_compute, recovery }; `Cnr3LiveRecoveryBranch` { none, exact_anchor, floor_fresh_start }
(both in `cnr3_plugin_internal.h`). Line numbers drift across snapshots — cite by symbol, confirm at scope
time.

### Set 2 — O-ITEM-LEVEL intent roles (DERIVED, source-grounded)
Labelled lists (human) + single-letter machine codes (parser). A frame may hold MULTIPLE roles; overlap is
real and shown honestly (e.g. a hole is also a source-request → `00000120=HS`).
```text
target       T   the requested output frame N
predecessor  P   frame N-1 as compute base (PRED_PRESENT)
anchor       A   recovery start frame (RECOVERY_EXACT)          (* if checkpoint-protected)
floor        F   fresh-start base frame (RECOVERY_FLOOR)
holes        H   absent outputs the plan will compute
sources      S   source frames queued for fetch                 [BRANCH-SPECIFIC — see below]
pinned       N   frames this plan pinned                        [BRANCH-DERIVED FACT — see below]
```

**`sources` is BRANCH-SPECIFIC. [RESOLVED — cross-check finding (a); a REAL GAP in v1]**
v1 derived `sources` from `source_request_frame_numbers` unconditionally. That vector is **recovery-only**:
cache_hit/frame0/pred_present request source frame `n` (as the arAllFramesReady trigger) WITHOUT populating
it; only the recovery branches fill it (in `cnr3_fill_recovery_source_request_numbers`). Verified against
committed source (the fill clears then pushes only on the exact-anchor and floor/hole paths; the three
non-recovery branches never touch it). Derivation:
```text
if branch in { CACHE_HIT, FRAME0, PRED_PRESENT }:  sources = [ n ]
if branch in { RECOVERY_EXACT, RECOVERY_FLOOR }:   sources = source_request_frame_numbers
```
This preserves VS-LIFECYCLE-01 source-request visibility for EVERY branch, not just recovery.

**`pinned` is BRANCH-DERIVED, not enumerated. [RESOLVED — cross-check finding #6 (§2.3/§8.2)]**
This is the sixth finding (see §0/§13). The pin-list vector (`Cnr3CachePinList` of `Cnr3CacheSlotPinToken`,
each carrying `frame_number`) is **private**; only `pin_count()` is public. `pinned` must therefore be
derived from branch facts, NOT read from a generic pin-list enumerator:
```text
CACHE_HIT        pinned = [ n ]                     (target pinned for direct return)
PRED_PRESENT     pinned = [ n - 1 ]                 (predecessor pin taken as compute base)
RECOVERY_EXACT   pinned (initial) = [ anchor ]      (anchor pin); hole/floor pins are result-time facts
RECOVERY_FLOOR   pinned = result-time facts from floor/hole store/adopt outcomes
```
**DECISION — do NOT add a public pin-list enumerator/accessor. [coordinator-confirmed]** The plan-trace
lives on derived branch facts. If 3c ever appears to need a live pin-list surface, that is a separate
PROPOSAL to the coordinator (R-PROCESS-25), never a silent new accessor.

**`pinned`/`unpinned` are per-item FACTS ONLY — no pin counts, either flavour. [LOCKED — rationale B.3.1]**
- WHY / rejected TWICE: (i) a GLOBAL pin-count compared across a frame's O and R phases is *semantically
  invalid* — between one frame's arInitial and its arAllFramesReady, OTHER frames pin/unpin on the same
  shared cache, so any delta mixes everyone's activity, not this plan's; (ii) even plan-LOCAL taken/released
  COUNTS were dropped on the coordinator ruling: "this exercise is about SEEING PLANS, not pin counts." Only
  the per-item facts remain — `pinned` (O-item), `unpinned` (R-item, Set 4 `U`).
- Where valid balances DO live (so the reader isn't tempted to add them here): D-SUM-04 (cache-side ref
  balance) and D-SUM-12 (recovery-plan lifecycle). The plan-trace is a *microscope on plans*, not a balance.

### Set 3 — R-FRAME-LEVEL outcome (source: arAllFramesReady end states)
Cross-check §3: sound.
```text
1 RETURNED_CACHE_HIT   returned directly from cache, no compute
2 RETURNED_COMPUTED    computed (frame0 / pred-present) and returned
3 RETURNED_RECOVERED   recovery completed, produced and returned
4 FAILED               getFrame errored (cnr3_set_filter_error + return nullptr) — carries fail_reason (Set 5)
```
`RETURNED_COMPUTED` covers frame0 + predecessor-present; `RETURNED_RECOVERED` covers recovery exact/floor
target return. FAILED sites are the existing `cnr3_set_filter_error(...)` + `return nullptr` pairs.

### Set 4 — R-ITEM-LEVEL outcome (source: `Cnr3LiveRecoveryHoleOutcome` + discharge + bail writes)
```text
C  computed            calculated and stored                       (Cnr3LiveRecoveryHoleOutcome::computed)
K  adopted_skipped     already present; skipped compute, pinned    (adopted_skipped)
L  post_compute_loser  computed but lost the race; adopted         (adopted_post_compute_loser)
U  unpinned            this plan released this frame               (pin_list discharge)   [per-item FACT]
N  none                nothing to do / default                     (none)
X  not_reached         planned item never reached (bailed before)  [DERIVED — needs bail-path write]
E  error_here          processing failed ON this item (bail cause) [DERIVED — needs bail-path write]
```
- **C/K/L/N** map to the existing `Cnr3LiveRecoveryHoleOutcome` source enum (declared in
  `cnr3_plugin_internal.h`, stringified in `cnr3_arAllFramesReady.cpp`). **U** is the pin-list-discharge fact.
  These five are OBSERVE-ONLY → they belong to **3c.1**.
- **X and E are NOT source enums.** They require the bail-site writes (§7) and are the invasive part → **3c.2**.
- **DECISION — keep X/E LOCAL to the plan-trace result record; do NOT expand the production
  `Cnr3LiveRecoveryHoleOutcome` enum.** [cross-check §4 recommendation, adopted] Adding X/E as live outcome
  values would touch a production enum used on the hot path; the plan-trace carries them in its own record
  instead. If a future need arises to promote them to production, that is a separate proposal.

### Set 5 — FAILURE-REASON (on FAILED) — a SOURCE-LINE SITE-TO-CATEGORY TABLE, not a message parser
**[RESOLVED — cross-check findings (b) + (c)]**

**DECISION — the mapping is a mechanical per-site table built from the actual source lines, NEVER a runtime
message parser.** [finding (b)] Each `cnr3_set_filter_error` site is assigned a category by its source
location, produced as a table (site → category) at 3c.2 scope time against then-current source. Rationale:
string-parsing bail messages at runtime is fragile and couples the diagnostic to message wording; a compile-
time site table is exact and survives message edits. The branch/phase context (K.1D/K.1E/K.1F/D.3/P.11C/W.3)
is already carried by `frame_code` (Set 1/3), so the category needs only the WHAT-went-wrong; `frame_code` +
category reconstruct the exact message.

**Categories — the v1 thirteen PLUS three added = SIXTEEN. [finding (c)]**
```text
 1 COPYFRAME_FAILED             copyFrame returned null/error
 2 COPYFRAME_SOURCE_ALIAS       copyFrame returned the source-frame alias (correctness guard)
 3 SOURCE_RETRIEVAL_FAILED      source frame retrieval (getFrameFilter) returned null
 4 SOURCE_NOT_REQUESTED         needed source not requested at arInitial (plan/request mismatch)
 5 ACQUIRE_REF_FAILED           failed to acquire predecessor/target/compute reference
 6 ADOPT_FAILED                 failed to adopt computed output / owned cache reference
 7 STORE_PRUNE_FAILED           failed to store/pin/prune (hole/floor/target/output)
 8 DISCHARGE_FAILED             failed to discharge pin-list
 9 INVALID_LIFECYCLE            invalid frameData/recovery lifecycle (state-machine violation)
10 INVALID_BRANCH_FOUNDATION    invalid recovery branch foundation
11 SCENE_PROCESSING_FAILED      pixel/scene processing failed (P.11C)
12 BYTE_ESTIMATE_FAILED         failed to compute output byte estimate (W.3 cache-pressure)
13 FRAMEDATA_MISSING_OR_UNKNOWN missing frameData / unknown branch / pinned output not retrievable
-- added (finding (c); cross-check §5) --
14 ALLOCATION_FAILED            frameData / per-hole outcome allocation failure (arInitial alloc sites)
15 RECOVERY_PLAN_FAILED_OR_REFUSED  bounded recovery plan failed, or recovery refused (arInitial)
16 HOT_ZONE_OBSERVATION_FAILED  hot-zone observation failure (arInitial policy/diagnostic site)
```
Why the three were added rather than folded: cross-check §5 found ALLOCATION_FAILED, the recovery-plan
failure/refusal, and hot-zone-observation each land at real arInitial sites that fit the existing 13 only by
force (INVALID_LIFECYCLE / FRAMEDATA_MISSING / STORE_PRUNE / INVALID_BRANCH_FOUNDATION — none exact).
Explicit categories are clearer at the exact bail site; the site-table makes the assignment unambiguous.

**Bail-site count: >50, specifically 65 CALL sites as of the cross-check snapshot. [finding (c); corrected D-V2-1]**
```text
cnr3_arInitial.cpp        14 call sites   (v1's "~50" OMITTED these; 0 definitions here)
cnr3_arAllFramesReady.cpp 50 call sites   (raw grep 51 INCLUDES the DEFINITION at AR:526,
                                           `void cnr3_set_filter_error(` — not a bail site;
                                           plugin_internal.h:111 is the declaration, also not a site)
vapoursynth-Cnr3.cpp       1 call site    (top-level getFrame state; also omitted by v1)
------------------------------------------
total                     65 call sites
```
Verified against committed source: the function DEFINITION lives in `cnr3_arAllFramesReady.cpp:526`, so the AR
raw `cnr3_set_filter_error` grep of 51 is 50 CALL sites + 1 definition. Total CALL sites = 14 + 50 + 1 = 65.
(D-V2-1: v2 propagated the cross-check's "51 sites plus the definition" phrasing as 66; the definition-
exclusion was the missed step. The raw counts v2 cited were correct; the site count was off by one.) These
counts are a snapshot; the authoritative site-to-category TABLE is built at 3c.2 scope time against then-
current source (see §12), where 65 is re-derived authoritatively. "~50" in v1 was AR-side only.

---

## 5. Emission format (human-primary labelled lists + appended machine codes)

**DECISION — human-primary labelled lists, one per line, top-to-bottom, so O and R align by eye; machine
codes appended at line end. [LOCKED — rationale B.3.8]** Both audiences (human reader, parser) served by one
record. Frame numbers ZERO-PADDED; a trailing `*` marks a checkpoint-protected frame (legend-defined,
extensible with further glyphs later). Overlap shown honestly (a frame appears in every list it belongs to).

Example (note `sources` is now branch-correct — a RECOVERY_EXACT frame, so from the recovery vector):
```text
[O] seq=00000042 frame=00000123 strategy=RECOVERY_EXACT enter=... exit=... ms=1.83
    anchor=00000119*
    holes=[00000120,00000121,00000122]
    sources=[00000120,00000121,00000122,00000123]
    pinned=[00000119]
    target=00000123
    codes=[00000119=AP,00000120=HS,00000121=HS,00000122=HS,00000123=TS]
[R] seq=00000191 frame=00000123 outcome=RETURNED_RECOVERED enter=... exit=... ms=7.44
    computed=[00000121,00000122,00000123]
    adopted_skipped=[00000120]
    unpinned=[00000119]
    codes=[00000119=U,00000120=K,00000121=C,00000122=C,00000123=C]
```
Branch-specific contrast (a CACHE_HIT frame — `sources=[n]`, NOT from the recovery vector; `pinned=[n]`):
```text
[O] seq=00000007 frame=00000045 strategy=CACHE_HIT enter=... exit=... ms=0.02
    sources=[00000045]
    pinned=[00000045]
    target=00000045
    codes=[00000045=TSN]
[R] seq=00000009 frame=00000045 outcome=RETURNED_CACHE_HIT enter=... exit=... ms=0.05
    unpinned=[00000045]
    codes=[00000045=U]
```
On FAILED, R additionally carries `fail_reason=<Set 5 category>`, and the codes show `E` on the bail item and
`X` on planned items never reached (3c.2). Legends (Sets 1–5 + the `*` glyph) print at the top of every dump.

---

## 6. Datetimes

**DECISION — steady-clock ticks as the hot-path sort key; UTC readable column derived AT DUMP TIME.
[LOCKED — rationale B.3.6]**
- Capture at function ENTER and EXIT for BOTH phases (4 timestamps per frame across the pair).
- SORT KEY: a monotonic high-resolution tick (`std::chrono::steady_clock`, integer ticks, zero-padded) —
  never goes backward, separates near-simultaneous concurrent events.
- READABLE column: a formatted UTC datetime (`system_clock`), DERIVED at DUMP time from a once-captured
  `(steady, system)` anchor pair — so the hot path stores only cheap ticks; formatting happens single-
  threaded at the dump.
- WHY / rejected alternative: storing formatted datetime strings live was rejected — it is expensive on the
  hot path and gives a non-monotonic key that can reorder near-simultaneous concurrent events. `run_ms =
  (exit_tick - enter_tick)` in ms.

---

## 7. Observe-only boundary + the bail-path writes (R-PROCESS-19 / R-PROCESS-21 / R-PROCESS-25)

```text
- Everything is gated behind the D-SUM plan-trace compute macro. With it OFF, the buffer, records, all
  writes, and the dump compile OUT — behaviour byte-identical. The R-PROCESS-19 macro-off proof is the exit
  gate (§10).
- PURE-OBSERVE (3c.1): O/R record capture reads plan/result state that already exists; it does not change
  what arInitial/arAllFramesReady decide, compute, pin, or return. Sets 1-3 + the C/K/L/U/N subset of Set 4.
- THE INVASIVE PART (3c.2): capturing R-item X (not_reached) and E (error_here) and the FAILED reason
  requires the 65 bail sites (§4) to WRITE their outcome/reason before the existing `return nullptr`. This is
  ADDITIVE (set a value, then the existing return) but it is control-flow-ADJACENT to proven getFrame paths →
  R-PROCESS-21 + R-PROCESS-25 apply: additive only, no restructure, propose the exact per-site edits for
  review, with the site-to-category table (§4/§12) as the foundation artifact.
```
The 3c.1/3c.2 split is delineated in §12; whether they ship as one patch or two is a SCOPING decision, not
decided here.

---

## 8. ARCHITECTURE — buffered clean blocks, dumped on end-OR-bail, FLUSHED ALWAYS

**DECISION — buffered clean copy-pasteable BLOCKS, not streaming. [LOCKED — rationale B.3.2]**
- WHY / rejected alternative: streaming is crash-proof by construction, and it was explored — but it produces
  output interleaved across frames and mixed with all other stderr, which is grep-and-reconstruct. The
  coordinator requirement (easy to forget, easy to violate) is that the output be CLEAN, SELF-CONTAINED,
  COPY-PASTEABLE blocks that paste straight into designer/coder chats and tools. The three-party relay is
  block-oriented. Streaming was rejected on this requirement — do not reintroduce it as an "improvement".

```text
- BUFFER: per-instance, preallocated for the window (§2), mutex-guarded WRITES (a diagnostics-only
  std::mutex, NOT a cache/CMS lock). Capture timestamp OUTSIDE the lock; briefly lock to write the record +
  bump action_seq; format/emit OUTSIDE the lock (DIAG.1 snapshot-outside-lock discipline).
```

**DECISION — dump on clean-end OR bail, once-guarded. [LOCKED — rationale B.3.3]**
```text
- DUMP TRIGGER: emit ONE clean sorted block per run at whichever comes FIRST:
    (a) clean end-of-run (filter free / teardown), OR
    (b) FAILURE BAIL PATH — before/at cnr3_set_filter_error's return, dump the buffer-so-far as a clean
        block (same format, "as far as we got"; the failed frame's E/X + reason are the last entries).
  Guard with a once-only `dumped` flag so end + bail cannot double-emit. Clean-end OR bail: exactly one block
  per run either way.
```
- WHY: traced the ACTUAL failure behaviour — every bail is uniform (`cnr3_set_filter_error(...)` then
  `return nullptr`; no abort/throw/assert). VapourSynth tears down after a bail, so an end-of-run-only dump
  would LOSE exactly the case we most want (the failure). The failing getFrame call itself is the reliable
  flush point. (This uniform-bail trace is also WHY the bail-path E/X writes in §7 are feasible as pure
  additive writes.)

**DECISION — FLUSH ALWAYS. HARD REQUIREMENT. [LOCKED — now ratified as R-PROCESS-24, Document A v3.13]**
```text
- Every emitted line uses Cnr3StderrFlushPolicy::flush (the DIAG.1 writer default — do NOT pass no_flush),
  and the block dump ends with an explicit cnr3_diag_flush_stderr().
```
- WHY, both load-bearing: (1) CRASH-SURVIVAL — unflushed stdio buffers are lost when VS tears down on
  failure; per-line flush guarantees bytes are on the wire before the bail's `return nullptr`. (2) MULTITHREAD
  ORDERING — per-line flush keeps interleaved output in true temporal order, which the datetime-sorted view
  (§8 view (a)) depends on; buffered lines could reorder vs events. `no_flush` is FORBIDDEN anywhere in this
  family (R-PROCESS-24).

**DECISION — three legend-headed sorted views, each sub-#ifdef gated. [LOCKED — rationale B.3.7]**
```text
    (a) sort by enter_datetime, phase  — temporal: what arrived/began-completing when (most natural)
    (b) sort by frame, phase           — pairing: each frame's plan directly above its result
    (c) sort by phase, frame           — interleaving: all opens as a block, all results as a block
```
- Each view is independently print-or-not gated and PRECEDED BY ITS CODE LEGEND (Sets 1-5 + the `*` glyph).
  Numbers/flags live INSIDE the feature `#if` so a commented-out view will not compile stale (mirrors the
  D-SUM-10 dump gating). External re-sorting also works because of the zero-padded keys — that is what they
  are FOR. Home: `build_config.h`.

---

## 9. Gate structure (build_config.h; nested, consistent with CMS07-B.1 + the D-SUM-10 dump pattern)

```text
Master compute gate (new D-SUM plan-trace family, two-gate #error pattern like DSUM01-14) wraps everything.
Nested sub-flags (numbers INSIDE their feature #if):
  - the three sort-view toggles (VIEW_DATETIME / VIEW_FRAME / VIEW_PHASE),
  - the window from/to compile-time bounds (§2),
  - any dump options.
Master OFF => the whole family compiles out (R-PROCESS-19). Do NOT alter existing DSUM01-14 gates; ADD the
new family's gates only. Print gates paired with compute gates per the standing two-gate #error pattern.
```

---

## 10. Proof gate (SPLIT by 3c.1 / 3c.2)

**3c.1 — buffered capture + clean-end dump (pure observe-only):**
```text
1. Build default config, master gate ON: four-way 56/56 / 56/56 / 55/56 exit 1 / 56/56; the plan-trace block
   emits at end-of-run with all three views (as gated) + legends; window respected; Sets 1-3 + C/K/L/U/N.
2. R-PROCESS-19 macro-off proof: master gate OFF => compiles/links, buffer/records/writes/dump compile out,
   four-way IDENTICAL, .vpy byte-identical on/off. THE exit gate.
3. R-PROCESS-19 matrix + S-series real-run acceptance per the standing pattern (block-presence when off;
   real-run behaviour unchanged; prior families untouched).
```

**3c.2 — dump-on-bail + E/X + failure-reason bail-site writes (the invasive part):**
```text
4. FAILURE-DUMP proof: force a bail (a known failing scenario) and confirm the block STILL prints (from the
   bail path) with the failed frame's E/X items + fail_reason category, FLUSHED, before teardown.
5. FLUSH proof: confirm per-line flush leaves no lost tail on an induced failure (R-PROCESS-24).
6. R-PROCESS-21/25: the 65 bail-site writes are ADDITIVE only; the proven getFrame paths are otherwise
   unchanged; each per-site edit was proposed and reviewed; cache-core + recovery selftests pass unchanged;
   whole-patch deletion scan clean.
```

---

## 11. Project rule status

R-PROCESS-24 (flush-per-line) is **RATIFIED** (Document A v3.13) — no longer a proposal (v1 §11 carried it as
"R-PROCESS-2x proposed"). R-PROCESS-25 (propose-before-transforming-proven-code) is likewise ratified and
governs the §7/§12 bail-site writes. §8's flush-always and §7's additive-only bail writes are the concrete
application of both.

---

## 12. THE 3c.1 / 3c.2 BOUNDARY (delineation only — the split is a SCOPING decision, deferred)

This section draws the line so the DIAG.3c scope can decide whether to ship one patch or two. It does NOT
decide the split.

```text
3c.1  BUFFERED PLAN/RESULT CAPTURE + CLEAN-END DUMP   — OBSERVE-ONLY, R-PROCESS-19
  In scope:
    - the per-instance windowed buffer, diagnostics-only mutex, snapshot-outside-lock capture (§8)
    - O record at arInitial EXIT; R record at arAllFramesReady EXIT (§1, §3)
    - Set 1 (strategy), Set 2 (roles, incl. branch-specific sources + branch-derived pinned), Set 3
      (outcome), Set 4 subset C/K/L/U/N (§4)
    - the three legend-headed sorted views + emission format (§5, §8)
    - clean END-OF-RUN dump only (NO bail path)
    - the master gate + sub-flags (§9)
  Touches NO bail site. Provable with the standard R-PROCESS-19 pattern (§10 items 1-3).

3c.2  DUMP-ON-BAIL + E/X + FAILURE-REASON BAIL-SITE WRITES  — INVASIVE, R-PROCESS-21/25
  In scope:
    - the dump-on-bail trigger + once-guard (§8)
    - Set 4 X (not_reached) + E (error_here) capture (§4)
    - Set 5 failure-reason on FAILED (§4)
    - the SITE-TO-CATEGORY TABLE (65 call sites: arInitial 14 / AR 50 / top-level 1; raw AR grep 51 includes the AR:526 definition), built from live source at
      scope time — the FOUNDATION ARTIFACT for this half
    - the per-site additive writes before each existing `return nullptr`
  Touches the 65 bail sites (additive only). Its own R-PROCESS-21/25 review + the §10 items 4-6 proof gate.
```
Dependency: 3c.2 builds ON 3c.1's buffer/format/dump machinery (it adds the bail trigger and the E/X/reason
entries). Note the coordinator's stated direction in the rationale doc ("see what is happening in each fail
case" — capture fails from day one); that direction favours doing both, but the SEQUENCING/packaging is left
to the scope. Delineated, not decided.

---

## 13. Findings resolution record (SIX findings — the 5-vs-6 correction, for provenance)

The cross-check (`CNR3_DIAG3a_PlanResult_Cross_Check_Report_2026-07-04.md`) produced **six** findings. The
handoff note that commissioned v2 named five (a-e) and folded the sixth under the spirit of its carry-forward
directive rather than naming it. All six are recorded here as RESOLVED so the note-vs-spec count matches.

```text
(a) sources BRANCH-SPECIFIC          RESOLVED §4 Set 2. sources = [n] for cache_hit/frame0/pred_present;
                                     = source_request_frame_numbers for recovery. (Was a real gap in v1.)
(b) SITE-TO-CATEGORY TABLE           RESOLVED §4 Set 5 + §12. Mechanical per-site table from source lines;
    (not a message parser)           never a runtime message parser. Table built at 3c.2 scope time.
(c) +3 CATEGORIES, >50 SITES         RESOLVED §4 Set 5. 13 -> 16 (ALLOCATION_FAILED,
                                     RECOVERY_PLAN_FAILED_OR_REFUSED, HOT_ZONE_OBSERVATION_FAILED).
                                     Bail-site count corrected: 65 call sites (14 + 50 + 1; raw AR grep 51 includes the AR:526 definition), not "~50" (AR-only).
(d) from/to = COMPILE-TIME           RESOLVED §2. Decided compile-time for the first cut; runtime .vpy
                                     params deferred as a later API-surface option.
(e) dump-on-bail = INVASIVE          RESOLVED §7, §12. Isolated as 3c.2 under R-PROCESS-21/25 with the
                                     site-to-category table as its foundation.
(6) pinned BRANCH-DERIVED            RESOLVED §4 Set 2. THE SIXTH. pinned derived from branch facts, not
    (the un-named one)               enumerated; pin-list stays private (no public accessor added). Folded
                                     under the note's directive-2 spirit; named here for provenance.
```

Carried-forward LOCKED decisions (rationale Part B; §5-§8 here): blocks-not-streaming; pinned/unpinned as
per-item facts only (pin counts rejected twice); dump-on-clean-end-OR-bail with once-guard; flush-always
(R-PROCESS-24); windowed buffer, no ring/no saturation; steady-clock ticks + UTC-at-dump; three legend-headed
views; human-lists + appended machine-codes. Any change to these is a PROPOSAL (R-PROCESS-25), not an edit.

---

## 14. Next step

This spec is the controlling input to the DIAG.3c SCOPE (designer), which decides the 3c.1/3c.2 packaging,
then: coder investigate/confirm -> designer decisions -> patch(es) -> diff review -> proof gate -> commit.
The site-to-category TABLE (§4/§12) is the first artifact the 3c.2 scope must produce from live source.
```
