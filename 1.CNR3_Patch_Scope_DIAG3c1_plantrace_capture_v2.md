# CNR3 — PATCH SCOPE: DIAG.3c.1 — PLAN-TRACE BUFFERED CAPTURE + CLEAN-END DUMP (observe-only) — v2

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Scope v2 (supersedes v1):** carries the spec v2.2 clarifications on plan-trace view (a) — the
(enter_tick ASC, action_seq ASC) sort key, phase dropped as a sort term, and the enter_tick-outside-lock /
action_seq-inside-lock capture invariant (§4.3/§4.4/§1). No change to the 3c.1 scope boundary.
**Controlling input:** `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v2_2.md` (the plan-trace spec).
Read it first; this scope proposes the 3c.1 slice of it. Where this scope and the spec disagree, the spec
wins and you flag the discrepancy.
**Status:** PROPOSAL for coder investigate/confirm. Do NOT patch until your confirm report is reviewed and
decisions are issued. Confirm-before-patch, always — the confirm report is where this scope gets reconciled
with real source, and every recent cycle that pass caught something the scope missed. That is the process
working; find what I got wrong.
**Scrutiny:** NORMAL-HARD, rising to exceptional on two points — this is a NEW family (new data structure,
new gates) and it inserts capture at proven getFrame EXIT paths. Treat the exit-capture mechanism (§4.1) as
the highest-risk item.

---

## 0. THE SCOPING DECISION (3c.1 vs 3c.2 — and why this scope is 3c.1 only)

Spec v2.2 §12 deliberately left the 3c.1/3c.2 packaging to scope time. Decision for this cycle:

- **This scope covers 3c.1 ONLY** — the observe-only buffered capture + clean-end dump. It touches NO bail
  site. It is provable with the standard R-PROCESS-19 pattern (macro-off + matrix + S-series).
- **3c.2** (dump-on-bail + E/X + Set 5 failure-reason writes across the 65 bail sites) is a SEPARATE later
  cycle with its own scope, its own R-PROCESS-21/25 review, and the site-to-category table as its foundation.

**Why 3c.1 first is a no-regret move (holds under either final packaging):** 3c.2 builds ON 3c.1's buffer,
record format, and dump machinery — none of the bail work can start until that layer exists. So the coder's
first investigate/confirm/patch cycle is the 3c.1 capture layer regardless of whether we ultimately commit
3c.1 standalone or roll straight on into 3c.2.

**DESIGNER RECOMMENDATION (coordinator's call at the 3c.1 boundary):** commit 3c.1 standalone, then do 3c.2
as the next cycle. Reasoning: it keeps the clean R-PROCESS-19 observe-only proof unentangled from the 65
proven-path touches; it lets the risky invasive work get focused scrutiny on a committed foundation; and it
matches how every prior diagnostics family was scoped and proven one at a time. The coordinator's recorded
direction ("capture fails from day one — see what is happening in each fail case", rationale doc / spec §12)
is honored either way: the split delivers 3c.2 as the very next cycle, so failure forensics still lands
early — just on a proven base. **W3X: confirm split-vs-combined at the 3c.1 commit boundary; nothing in this
scope forecloses combining if you prefer.**

---

## 1. WHAT 3c.1 DELIVERS

A per-output-frame PLAN-vs-RESULT trace, buffered per-instance, dumped as clean copy-pasteable sorted
block(s) at CLEAN END-OF-RUN only. Per spec v2.2:

- **O record** at arInitial EXIT (the plan): Set 1 strategy + Set 2 intent roles.
- **R record** at arAllFramesReady EXIT (the result): Set 3 outcome + Set 4 subset **C/K/L/U/N** only.
- Record fields per spec §3 (phase, zero-padded frame, zero-padded action_seq, enter/exit datetime, run_ms,
  frame_code, labelled-list body + appended machine codes).
- Windowed preallocated buffer (from/to COMPILE-TIME, spec §2); NO ring, NO saturation machinery.
- Three legend-headed sorted views, each sub-#ifdef gated (spec §8): (a) temporal — sort by
  (enter_tick ASC, action_seq ASC), UTC is the display column only (NOT a datetime string sort); (b) frame,
  phase — pairing; (c) phase, frame — interleaving.
- steady_clock ticks on the hot path; UTC readable column derived at dump time from a once-captured
  (steady, system) anchor (spec §6).
- FLUSH-ALWAYS (R-PROCESS-24): per-line flush, explicit flush at end of the dump.

**Explicitly NOT in 3c.1 (fenced off to 3c.2):** Set 4 **X / E**; Set 5 failure-reason; the dump-on-bail
trigger; ANY write at or near a `cnr3_set_filter_error` site. If a clean 3c.1 requires even reading state at
a bail site, STOP and flag it — it moves to 3c.2.

---

## 2. THE OBSERVE-ONLY BOUNDARY (R-PROCESS-19 is the exit gate)

3c.1 adds only: reads of plan/result state that already exists, writes into the diagnostics-only buffer, and
the clean-end dump. It must not change what arInitial/arAllFramesReady decide, compute, pin, or return.
With the master gate OFF, the buffer, records, all writes, and the dump compile OUT and behaviour is
byte-identical — that is the acceptance exit gate (§7).

If capturing a record cleanly appears to require reordering, restructuring, or otherwise touching a proven
line (not merely adding an observe call adjacent to it), that is R-PROCESS-25: PROPOSE the exact edit for
review, do not fold it in. "Behaviourally identical" is the designer's call, not an assumption you make.

---

## 3. THE DEFERRED-TO-3c.2 FENCE (do not cross in this cycle)

```text
DO NOT, in 3c.1:
  - touch, wrap, or write near ANY of the 65 cnr3_set_filter_error call sites
  - add the dump-on-bail path or the once-guard's bail arm
  - emit Set 4 X (not_reached) or E (error_here)
  - emit Set 5 failure-reason
  - expand the production Cnr3LiveRecoveryHoleOutcome enum (X/E stay local to 3c.2's record, later)
  - add any public pin-list enumerator/accessor (pinned is DERIVED — spec §4 Set 2; if you think you need
    an accessor, that is a PROPOSAL to the coordinator, not a patch)
```
A frame that bails in 3c.1 simply receives no R record; under a clean run there are no bails, so the
clean-end block is complete. Confirm the buffer/dump handles an O-without-R frame gracefully (no crash, no
mis-pairing) even though a clean run should not produce one.

---

## 4. WHAT YOU MUST INVESTIGATE AND CONFIRM (before any patch)

Report file:line citations against the CURRENT committed post-DIAG.3b source. Do not accept this scope's
pointers as fact — re-derive. The items below are ranked by risk.

### 4.1 (HIGHEST RISK) The EXIT-capture mechanism — O at arInitial exit, R at arAllFramesReady exit
arInitial dispatches through four branch-publish tail-returns after populating `request_data`
(cache_hit / frame0 / predecessor_present / start_recovery). arAllFramesReady returns from four
branch-outcome regions. There are many successful return points in each function.
- Map EVERY successful-exit point in both functions (the clean returns, not the bail `return nullptr`s).
- Propose the least-invasive capture that fires once per in-window frame at exit, for ALL branches. Options
  to weigh and recommend between: (a) a scope-exit/RAII guard that captures on the way out; (b) an explicit
  capture helper called at each successful return. Note the 2b RAII-ownership lesson — if RAII, prove it
  cannot alter ownership/lifetime of anything it observes. State which you recommend and why.
- Confirm the capture point sees the plan FULLY populated (O) / the result FULLY known (R). If any field is
  not yet set at the natural exit point, say so — that shapes where capture goes.

### 4.2 Field availability at exit, per branch (drives Sets 1-4)
Confirm each is readable at the O/R capture point, and cite where:
- Set 1 strategy from `request_data->branch` (+ recovery sub-branch for EXACT/FLOOR).
- Set 2 roles: target = requested_frame; predecessor = predecessor_frame; anchor =
  recovery_plan.anchor_frame_number; floor = recovery_floor_frame; holes = recovery_plan.hole_frame_numbers.
- **sources — BRANCH-SPECIFIC (spec §4 Set 2, finding a):** `[n]` for cache_hit/frame0/pred_present (they do
  NOT populate source_request_frame_numbers); `source_request_frame_numbers` for recovery. Confirm the
  vector is empty/untouched on the three non-recovery branches at the capture point.
- **pinned — BRANCH-DERIVED (spec §4 Set 2, finding 6):** cache_hit=[n]; pred_present=[n-1]; recovery_exact
  initial=[anchor]; floor/holes = result-time facts. Confirm derivable from branch facts WITHOUT enumerating
  the private pin-list vector.
- Set 3 outcome from the arAllFramesReady end states (cache_hit / computed / recovered / — FAILED is 3c.2).
- Set 4 C/K/L/N from `Cnr3LiveRecoveryHoleOutcome`; U (unpinned) from the pin-list discharge fact. Confirm
  these are accessible at the successful R exit.

### 4.3 The buffer home and concurrency
- Confirm the per-instance buffer lives on `Cnr3FilterData` (reachable from both arInitial and
  arAllFramesReady), gated behind the master compute macro so it vanishes when off.
- Diagnostics-only `std::mutex` (NOT a cache/CMS lock). Snapshot timestamp OUTSIDE the lock; briefly lock to
  write record + bump action_seq; format/emit OUTSIDE the lock (DIAG.1 discipline).
- **INVARIANT (spec v2.2 §8) — enforce and do not "optimize" away:** `enter_tick` is sampled OUTSIDE the
  mutex (cheap monotonic read, carried in with the record); `action_seq` is bumped INSIDE the mutex, in the
  SAME critical section as the buffer append. Moving the action_seq increment outside the lock reintroduces
  the fmUnordered read-increment-write collision and destroys its uniqueness — which is its ONE JOB as the
  view (a) tie-break. If you see a reason to move either, that is a PROPOSAL, not a patch.
- Confirm behaviour under fmUnordered (O and R time-separated, interleaved with other frames — pairing is by
  frame number at dump, not emission order).

### 4.4 Time, keys, widths
- Once-per-instance (steady, system) anchor capture; steady ticks stored per record; UTC derived at dump.
- action_seq: monotonic per-instance counter stamped at record write, INSIDE the buffer lock (see §4.3
  invariant) — globally unique per instance; it is the DETERMINISTIC TIE-BREAK for view (a).
- **View (a) sort key = (enter_tick ASC, action_seq ASC)** (spec v2.2 §8). `phase` is NOT a sort term for
  view (a) — it is a displayed field only; O-before-R falls out of action_seq automatically. Do not re-add
  phase as a tie-break. Views (b)/(c) sort by frame,phase / phase,frame respectively.
- Zero-pad widths for frame and action_seq — propose a derivation (from to_frame / a fixed width) that keeps
  keys externally sortable.

### 4.5 The clean-end dump site
- Identify the filter free/teardown point (likely in vapoursynth-Cnr3.cpp) where the clean-end dump fires.
- Once-only `dumped` flag (in 3c.1 only its clean-end arm exists; the bail arm is 3c.2). Confirm the flag and
  the dump are gated and compile out when off.
- The three views + legends emit here. Confirm each view is independently sub-#ifdef gated with numbers/flags
  INSIDE the feature #if (no stale compile of a commented-out view).

### 4.6 Family naming / gates (propose; coordinator ratifies the final tag/number)
Working names, house convention (confirm and propose final): master `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` +
print `CNR3_DIAG_PRINT_DSUM_PLANTRACE` (two-gate #error pattern like DSUM01-14); sub-flags
`..._VIEW_DATETIME / ..._VIEW_FRAME / ..._VIEW_PHASE`, window `..._FROM / ..._TO`. Block/record tag for the
[O]/[R] lines (this is a per-frame TRACE, not a [DSUM-SUMMARY]) — propose a distinct tag, e.g.
`[DSUM-PLANTRACE]`. Do NOT alter existing DSUM01-14 gates; ADD the new family only. **Final family number and
tag are a coordinator naming decision — flag it, don't assume it.**

---

## 5. GATE STRUCTURE (build_config.h)

Master compute gate wraps everything (two-gate #error paired with the print gate). Nested sub-flags with
numbers INSIDE their feature #if: the three view toggles, the compile-time window from/to, any dump options.
Master OFF => whole family compiles out (R-PROCESS-19). Home: build_config.h. Marker bump handled at commit
(as with DIAG.3b) — the new family's gates ARE a legitimate build_config change here (unlike 3b), so
build_config is in the file list this time.

---

## 6. EXPECTED FILE LIST (confirm the exact set in your report)

```text
cnr3_diagnostics.h / .cpp          new family: record struct, buffer type, capture/derive helpers, dump,
                                   sorted-view emitters, legends, flush
cnr3_plugin_internal.h             the per-instance buffer member on Cnr3FilterData (gated)
cnr3_arInitial.cpp                 O-record capture at successful exits (§4.1/§4.2)
cnr3_arAllFramesReady.cpp          R-record capture at successful exits (§4.1/§4.2) — NO bail touch
vapoursynth-Cnr3.cpp               clean-end dump at filter free/teardown (§4.5)
cnr3_build_config.h                new family gates + sub-flags (+ marker bump at commit)
cnr3_cache_core_selftest_main.cpp  selftest reference emitter for the new family (house pattern:
                                   deterministic fixture proving the writers + views + legends)
```
Flag any file you must touch that is not listed, and any listed file you find you do NOT need.

---

## 7. PROOF GATE (3c.1)

```text
1. Four-way all-on: 56/56 (Debug) / 56/56 (Release) / 55/56 exit 1 (forced-fail) / 56/56 (verbose);
   the plan-trace block emits at clean end-of-run with all three views (as gated) + legends; window
   respected; O/R pair correctly by frame; Set 4 shows only C/K/L/U/N.
2. R-PROCESS-19 MACRO-OFF (THE exit gate): master gate OFF => compiles/links, buffer/records/writes/dump
   compile out, four-way IDENTICAL, .vpy byte-identical on/off.
3. R-PROCESS-19 six-config-style presence: the new family block ABSENT when its gate is off, present when on,
   with the existing DSUM01-14 matrix unchanged (do not regress the 3b matrix).
4. S-series real-run (-r 1, a chosen compile-time window): the plan-trace block emits and pairs correctly,
   window respected, and the CONTENT matches the known scenario as a correctness check —
     S1 in-order: all CACHE_HIT / RETURNED_COMPUTED, no recovery strategies.
     S7/S8: RECOVERY_EXACT plans with holes/sources visible, consistent with the D-SUM-12 recovery the same
     run reports (the plan-trace is the per-frame view of that aggregate).
   Prior families 01/03/04/05/06/07/08/09/10/11/12/13/14 unchanged; non-diagnostic behaviour unchanged.
   (Note: the plan-trace carries NO balance — it is descriptive; there is no zero-balance check here. Its
   backstop is the macro-off byte-identical proof plus the scenario-content sanity check.)
```

---

## 8. RULES IN FORCE

R-PROCESS-19 (macro-off exit gate). R-PROCESS-24 (flush-always; no_flush FORBIDDEN in this family).
R-PROCESS-25 (propose before touching any proven line — see §2/§4.1). Snapshot-outside-lock (DIAG.1).
Confirm-before-patch. Whole-patch deletion scan on the eventual diff. Verify INVOCATION not just definition
(the D-1 lesson: a capture helper with zero live call sites compiles and silently captures nothing — trace
the call graph at review). Two-gate #error discipline for the new gates.

---

## 9. YOUR CONFIRM REPORT SHOULD DELIVER

1. The exit-capture mechanism recommendation (§4.1) with the mapped successful-exit points, file:line.
2. Per-branch field-availability confirmation (§4.2), including the sources/pinned derivations proven against
   source and any gap found.
3. Buffer home + concurrency model confirmation (§4.3); time/keys/widths proposal (§4.4); dump site (§4.5).
4. Proposed gate/family names + tag (§4.6) for coordinator ratification.
5. The exact file list (§6), with any additions/removals justified.
6. Anything this scope got wrong or missed — explicitly. That is the highest-value part of your report.
