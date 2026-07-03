# CNR3 — PLAN/RESULT DIAGNOSTIC (D-SUM PLAN-TRACE) — VOCABULARY + ARCHITECTURE SPEC v1 (DRAFT)

**From:** designer/reviewer (W3D), via coordinator (W3X). **Status:** DRAFT for coder sensibility/gap cross-check.
**Purpose:** a per-output-frame plan-vs-result diagnostic. For each requested output frame it records (a) the
PLAN arInitial produced (intent), and (b) the RESULT arAllFramesReady produced (outcome), as a matched pair,
emitted at end-of-run (or on failure) as clean, copy-pasteable, externally-sortable blocks. Observe-only
(R-PROCESS-19) except the one bail-path touch in §7. This is a getFrame/recovery family → DIAG.3 batch.
**Grounding:** all enums below derived from real source (cnr3_arInitial.cpp, cnr3_arAllFramesReady.cpp,
cnr3_plugin_internal.h, cnr3_cache_core.h). The O-frame/R-frame/R-item outcome codes are source enums; the
O-item roles, R-item E/X, and the failure-reason categories are DERIVED and flagged for coder cross-check.
**Builds on:** DIAG.1 framework (cnr3_diag_write_line, [DSUM-SUMMARY] pattern, snapshot-outside-lock).

---

## 1. Concept

Two records per in-window requested output frame:
- **O record (phase "O", plan_open):** written at arInitial EXIT — the plan (strategy + intent per item).
- **R record (phase "R", plan_result):** written at arAllFramesReady EXIT — the result (outcome + per item).
Records buffered per-instance; dumped as clean sorted block(s) at end-of-run OR on failure bail. External
tools/humans sort/pair via zero-padded keys. Under fmParallel/fmUnordered the two records for a frame are
written at different times and interleaved with other frames — pairing is by frame number in the dump, not
by emission order.

## 2. Window parameters

- **from_frame / to_frame** (inclusive) — OUTPUT frame-number bounds. A frame is recorded only if
  from_frame <= n <= to_frame, tested at each function EXIT. Frames outside the window are never recorded
  (buffer cannot overflow — window-bounded, so NO ring, NO saturation flag needed).
- Buffer preallocated for the window: up to 2*(to_frame - from_frame + 1) records (2 per frame).
- OPEN QUESTION for coordinator (deferred): are from/to COMPILE-TIME #defines (rebuild to re-window) or
  RUNTIME .vpy filter params (no rebuild, but a plugin-API surface change)? [decide before implement]

## 3. Record fields (both O and R)

```text
- phase              : "O" or "R"  (sortable; O sorts before R so frame's open precedes its result)
- frame              : requested output frame N, ZERO-PADDED (e.g. 00000123) — sortable
- action_seq         : monotonic per-instance counter stamped at record write — ZERO-PADDED — sortable
                       (gives action/arrival order independent of frame number)
- enter_datetime     : function entry timestamp (see §6)
- exit_datetime      : function exit timestamp
- run_ms             : (exit - enter) in milliseconds, human-readable derived field
- frame_code         : O = strategy (Set 1); R = outcome (Set 3)
- plan/result body   : labelled lists (§5, human-primary) + appended machine codes (§5, parser)
- (R only) fail_reason: if frame_code == FAILED, the failure-reason (Set 5)
```

## 4. THE FIVE ENUM SETS (the vocabulary — coder cross-check these against source for sensibility/gaps)

### Set 1 — O-FRAME-LEVEL (strategy; source: Cnr3LiveGetFrameBranch + Cnr3LiveRecoveryBranch)
```text
1 CACHE_HIT       cache_hit_return           frame already cached; pin + return, no compute
2 FRAME0          frame0_fresh_start         frame 0, no predecessor
3 PRED_PRESENT    predecessor_present_compute predecessor N-1 present/pinned; compute directly
4 RECOVERY_EXACT  recovery + exact_anchor     predecessor absent; exact anchor in window; walk holes
5 RECOVERY_FLOOR  recovery + floor_fresh_start predecessor absent, no anchor; fresh-start from floor
(0 NONE           none                        uninitialized/error guard — should not appear in a valid record)
```

### Set 2 — O-ITEM-LEVEL (intent roles; DERIVED from arInitial-populated fields — CROSS-CHECK)
Labelled lists (human) with single-letter machine codes (parser). A frame may hold MULTIPLE roles
(overlap is real and honest — e.g. a hole is also a source-request).
```text
target       T   the requested output frame N                    (n)
predecessor  P   frame N-1 pinned as compute base (PRED_PRESENT)  (predecessor_frame/predecessor_pin_taken)
anchor       A   recovery start frame (RECOVERY_EXACT)            (recovery start; * if checkpoint)
floor        F   fresh-start base frame (RECOVERY_FLOOR)          (recovery_floor_frame)
holes        H   absent outputs the plan will compute            (hole_frame_numbers)
sources      S   source frames queued for fetch                  (source_request_frame_numbers)
pinned       N   frames this plan pinned                         (pin_list/*_pin_taken)  [per-item FACT only]
```

### Set 3 — R-FRAME-LEVEL (outcome; source: arAllFramesReady end states)
```text
1 RETURNED_CACHE_HIT   returned directly from cache, no compute
2 RETURNED_COMPUTED    computed (frame0 / pred-present) and returned
3 RETURNED_RECOVERED   recovery completed, produced and returned
4 FAILED               getFrame errored (setFilterError + return nullptr) — carries fail_reason (Set 5)
```

### Set 4 — R-ITEM-LEVEL (per-item outcome; source: Cnr3LiveRecoveryHoleOutcome + discharge + §7 fails)
```text
C  computed            calculated and stored                       (computed)
K  adopted_skipped     already present; skipped compute, pinned    (adopted_skipped)
L  post_compute_loser  computed but lost the race; adopted         (adopted_post_compute_loser)
U  unpinned            this plan released this frame               (pin_list.discharge_all)  [per-item FACT]
N  none                nothing to do / default                     (none)
X  not_reached         planned item never reached (bailed before)  [DERIVED — needs §7 bail-path write]
E  error_here          processing failed ON this item (bail cause) [DERIVED — needs §7 bail-path write]
```

### Set 5 — FAILURE-REASON (on FAILED; DERIVED from the ~50 bail message strings, ~13 categories — CROSS-CHECK)
The branch/phase context (K.1D/K.1E/K.1F/D.3/P.11C/W.3) is already carried by the frame_code (Set 1/3), so
the reason enum needs only the WHAT-went-wrong category; frame_code + reason reconstruct the exact message.
```text
 1 COPYFRAME_FAILED            copyFrame returned null/error
 2 COPYFRAME_SOURCE_ALIAS      copyFrame returned the source frame alias (correctness guard)
 3 SOURCE_RETRIEVAL_FAILED     source frame retrieval failed
 4 SOURCE_NOT_REQUESTED        needed source not requested at arInitial (plan/request mismatch signal)
 5 ACQUIRE_REF_FAILED          failed to acquire predecessor/compute reference
 6 ADOPT_FAILED                failed to adopt computed output / cache reference
 7 STORE_PRUNE_FAILED          failed to store/pin/prune (hole/floor/target/output)
 8 DISCHARGE_FAILED            failed to discharge pin-list
 9 INVALID_LIFECYCLE           invalid frameData/recovery lifecycle (state-machine violation)
10 INVALID_BRANCH_FOUNDATION   invalid recovery branch foundation
11 SCENE_PROCESSING_FAILED     pixel/scene processing failed (P.11C)
12 BYTE_ESTIMATE_FAILED        failed to compute output byte estimate (W.3 cache-pressure)
13 FRAMEDATA_MISSING_OR_UNKNOWN missing frameData / unknown branch / pinned output not retrievable
```

## 5. Emission format (human-primary labelled lists + appended machine codes)

Human-primary: labelled lists, each on its own line, top-to-bottom, so O and R align by eye. Frame numbers
ZERO-PADDED; a trailing "*" marks a checkpoint-protected frame (legend-defined). Overlap shown honestly (a
frame appears in every list it belongs to). Machine codes appended at line end as codes=[frame=ROLES,...]
for parsing/consolidation.
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
(on FAILED, R also carries: fail_reason=SOURCE_NOT_REQUESTED, and codes show E on the bail item, X after.)
```

## 6. Datetimes (say "datetime"; representation to confirm)

- Capture at function ENTER and EXIT for BOTH phases (4 timestamps/frame across the pair).
- SORTABLE key: a monotonic high-resolution tick (recommend std::chrono::steady_clock, integer ticks,
  zero-padded) — never goes backward, separates near-simultaneous concurrent events.
- READABLE column: a formatted UTC datetime (from system_clock), DERIVED at DUMP time from a once-captured
  (steady, system) anchor pair — so the hot path stores only ticks (cheap), formatting happens single-
  threaded at the dump. run_ms = (exit_tick - enter_tick) in ms.
- [confirm: steady ticks as sort key + UTC formatted as readable, formatted at dump time — vs storing
  formatted strings live.]

## 7. Observe-only boundary + the ONE control-flow touch (R-PROCESS-19 / R-PROCESS-21)

```text
- Everything is gated behind the D-SUM plan-trace compute macro; with it OFF, the buffer, records, all
  writes, and the dump compile OUT — behaviour byte-identical (R-PROCESS-19 macro-off proof is the exit gate).
- PURE-OBSERVE parts: O/R record capture reads plan/result state that already exists; it does not change
  what arInitial/arAllFramesReady decide, compute, pin, or return.
- THE ONE TOUCH: capturing R-item X (not_reached) and E (error_here) and the FAILED reason requires the
  ~50 bail sites in arAllFramesReady to WRITE their outcome/reason before the existing `return nullptr`.
  This is additive (set a value, then the existing return) but it IS a control-flow-adjacent change to
  proven getFrame paths → R-PROCESS-21 applies: additive only, no restructure; propose exact edits for
  review. [Coordinator decision pending: capture E/X/reason NOW (this touch) vs first cut pure-observe with
  existing outcomes only and add fails later. Current direction: capture fails from day one — "see what is
  happening in each fail case" is the stated goal.]
```

## 8. ARCHITECTURE — buffered clean blocks, dumped on end-OR-bail, FLUSHED ALWAYS

```text
- BUFFER: per-instance, preallocated for the window, mutex-guarded WRITES (a diagnostics-only std::mutex,
  NOT a cache/CMS lock). Capture timestamp OUTSIDE the lock; briefly lock to write the record + bump
  action_seq; format/emit OUTSIDE the lock (DIAG.1 discipline).
- DUMP TRIGGER: emit ONE clean sorted block per run at whichever comes FIRST:
    (a) clean end-of-run (filter free / teardown), OR
    (b) FAILURE BAIL PATH — before/at cnr3_set_filter_error's return, dump the buffer-so-far as a clean
        block (same format, "as far as we got"; the failed frame's E/X/reason are the last entries).
  Guard with a once-only `dumped` flag so end + bail don't double-emit.
  RATIONALE: VapourSynth tears down on failure; an end-of-run-only dump would LOSE the failure case (the
  case we most want). Dumping at the bail path guarantees the block survives.

- *** FLUSH ALWAYS — HARD REQUIREMENT (non-negotiable for this family) ***
  Every emitted line uses Cnr3StderrFlushPolicy::flush (the DIAG.1 writer default — do NOT pass no_flush),
  and the block dump ends with an explicit cnr3_diag_flush_stderr(). Reasons, both load-bearing:
    (1) CRASH-SURVIVAL: unflushed stdio buffers are lost when VS tears down on failure; per-line flush
        guarantees bytes are on the wire before the bail's return nullptr.
    (2) MULTITHREAD ORDERING: per-line flush keeps interleaved diagnostic output in true temporal order,
        which the datetime-ordered sort view depends on. Buffered (unflushed) lines could reorder vs events.
  no_flush is FORBIDDEN anywhere in this family. (Proposed project rule: see §11.)

- THREE SORTED VIEWS in the block, each independently sub-#ifdef gated (print-or-not), each PRECEDED BY ITS
  CODE LEGEND (Sets 1-5 + the "*"=checkpoint glyph):
    (a) sort by enter_datetime, phase  — temporal: what arrived when, what began completing when (most natural)
    (b) sort by frame, phase           — pairing: each frame's plan directly above its result
    (c) sort by phase, frame           — interleaving: all opens as a block, all results as a block
  Sub-#ifdef structure mirrors the D-SUM-10 dump gating (feature flag + the sort-view selection), numbers/
  flags INSIDE the feature #if so a commented-out view will not compile stale. Home: build_config.h.
```

## 9. Gate structure (build_config.h; nested, consistent with CMS07-B.1 + the D-SUM-10 dump pattern)

```text
Master compute gate (new D-SUM plan-trace family, two-gate #error pattern like DSUM01-14) wraps everything.
Nested sub-flags (numbers inside their feature #if): the three sort-view toggles (VIEW_DATETIME / VIEW_FRAME
/ VIEW_PHASE), the window from/to (if compile-time), and any dump options. Master off => whole family
compiles out (R-PROCESS-19). Do NOT alter existing DSUM01-14 gates; ADD the new family's gates only.
```

## 10. Proof gate (when implemented)

```text
1. Build default config, master gate ON: four-way 56/56 / 56/56 / 55/56 exit 1 / 56/56; the plan-trace
   block emits at end-of-run with all three views (as gated) + legends; window respected.
2. R-PROCESS-19 macro-off proof: master gate OFF => compiles/links, buffer/records/writes/dump compile out,
   four-way IDENTICAL, .vpy byte-identical on/off. THE exit gate.
3. FAILURE-DUMP proof: force a bail (a known failing scenario) and confirm the block STILL prints (from the
   bail path) with the failed frame's E/X items + fail_reason, flushed, before teardown.
4. FLUSH proof: confirm per-line flush (no lost tail on an induced failure).
5. R-PROCESS-21: the ~50 bail-site writes are additive only; the proven getFrame paths otherwise unchanged;
   cache-core + recovery selftests pass unchanged.
```

## 11. Proposed project rule (coordinator ratification, Document A)

R-PROCESS-2x (proposed): "Diagnostic emissions flush per line by default. no_flush is permitted ONLY for a
high-volume family that flushes once at a bounded end-point, and NEVER on a path that can precede a failure
bail or that carries ordering-sensitive output." Codifies the current default+convention (cnr3_diag_write_line
defaults to flush) as a standing rule so it cannot erode. [Ratify when Document A is next touched; meanwhile
§8 states it hard for THIS family.]

## 12. Coder cross-check requested (per coordinator)

Confirm sensibility + gaps in: Set 1-5 vocabularies vs source; the O-item role derivation (are the 7 roles
complete/correct? any plan role missing?); the R-item E/X capture feasibility at the ~50 bail sites; the
failure-reason categorisation (~13 vs the ~50 messages — any message that does not fit a category?); the
buffer/mutex/dump-on-bail architecture; and whether from/to should be compile-time or runtime.
