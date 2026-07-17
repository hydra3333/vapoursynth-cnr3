# CNR3 DIAG.3c.1 — Coder Confirm-Before-Patch Report

**Phase:** CMS07-DIAG.3c.1  
**Topic:** Plan-trace buffered capture + clean-end dump, observe-only  
**Status:** Confirm-before-patch report for designer/coordinator review  
**Patch status:** No patch produced in this pass.

## Source/reference basis

Reading order followed:

1. `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v2_2.md`
2. `CNR3_Patch_Scope_DIAG3c1_plantrace_capture_v2.md`
3. Current post-DIAG.3b uploaded source archive

Key controlling-document points:

- Scope v2 says the controlling input is `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v2_2.md`, and where scope and spec disagree, the spec wins and the discrepancy must be flagged.
- Scope v2 says this is a proposal for coder investigate/confirm and that no patch should be produced until the confirm report is reviewed and decisions are issued.
- Spec v2.2 says its locked design decisions are not open for silent revision; any change requires a proposal under R-PROCESS-25.
- 3c.1 is explicitly fenced to observe-only buffered capture + clean-end dump. 3c.2 owns dump-on-bail, Set 4 X/E, Set 5 failure-reason, and bail-site writes.

## Executive finding

No source-level blocker was found for **DIAG.3c.1 observe-only buffered plan/result capture + clean-end dump**.

I recommend proceeding with **3c.1 standalone**, then 3c.2 separately. This matches the scope’s recommended split and keeps the R-PROCESS-19 proof cleanly separated from the later bail-site work.

Important: I recommend a **hybrid capture mechanism**, not a single mechanism for both phases:

```text
O record at arInitial exit:
  Use a top-level arInitial scope-exit guard.

R record at arAllFramesReady exit:
  Use explicit branch-local successful-exit capture calls.
```

Reason: `arInitial` can safely capture from the top-level after the branch-publish helper returns, because `frame_data` remains published and fully populated. `arAllFramesReady` cannot cleanly use the same top-level pattern because the successful branch helpers discard/delete `frame_data` before returning the `VSFrame`. Capturing R therefore needs branch-local copies of the relevant facts before deletion, followed by an explicit capture after the successful discharge point.

This is additive and observe-only, but the exact mechanism should be designer-ratified before patch.

---

## 1. Exit-capture mechanism recommendation

### 1.1 arInitial / O record

Recommended mechanism:

```text
Top-level scope-exit guard in cnr3_arInitial().
```

Current source:

```text
src/cnr3_arInitial.cpp:606-746
```

Successful arInitial routes:

```text
cache_hit_return:
  helper return at src/cnr3_arInitial.cpp:75
  top-level route at src/cnr3_arInitial.cpp:670-678

frame0_fresh_start:
  helper return at src/cnr3_arInitial.cpp:152
  top-level route at src/cnr3_arInitial.cpp:691-699

predecessor_present_compute:
  helper return at src/cnr3_arInitial.cpp:116
  top-level route at src/cnr3_arInitial.cpp:715-725

recovery:
  helper return at src/cnr3_arInitial.cpp:601
  top-level route at src/cnr3_arInitial.cpp:738-745
```

Why top-level RAII is safest here:

```text
- It captures the true arInitial function entry/exit timing.
- It avoids touching the four branch-publish helpers.
- It fires after the helper has populated request_data and published frame_data.
- It can be armed only when frame_data is non-null, branch != none,
  requested_frame == n, and source_requested == true.
- It does not own, free, transfer, or mutate any VSFrame or cache resource.
```

Capture sees the O plan fully populated:

```text
cache_hit:
  branch/requested/source/cache_hit_pin_taken set at src/cnr3_arInitial.cpp:53-56
  source request made at src/cnr3_arInitial.cpp:67-73

predecessor_present:
  branch/requested/source set at src/cnr3_arInitial.cpp:100-102
  predecessor_frame set at src/cnr3_arInitial.cpp:702
  predecessor_pin_taken set at src/cnr3_arInitial.cpp:715-716
  source request made at src/cnr3_arInitial.cpp:108-114

frame0:
  branch/requested/source set at src/cnr3_arInitial.cpp:136-138
  source request made at src/cnr3_arInitial.cpp:144-150

recovery:
  branch/recovery_branch/requested/predecessor/floor/plan set at
  src/cnr3_arInitial.cpp:515-520
  source request vector filled at src/cnr3_arInitial.cpp:537-548
  per-hole outcomes allocated at src/cnr3_arInitial.cpp:550-564
  source_requested set at src/cnr3_arInitial.cpp:566
  frame_data published at src/cnr3_arInitial.cpp:589
  source requests issued at src/cnr3_arInitial.cpp:591-599
```

### 1.2 arAllFramesReady / R record

Recommended mechanism:

```text
Explicit branch-local capture helper called on each successful branch return path.
```

Successful arAllFramesReady branch exits:

```text
cache_hit_return:
  src/cnr3_arAllFramesReady.cpp:1047-1057

predecessor_present_compute:
  src/cnr3_arAllFramesReady.cpp:1268-1292

recovery:
  src/cnr3_arAllFramesReady.cpp:2027-2086

frame0_fresh_start:
  src/cnr3_arAllFramesReady.cpp:2286-2309
```

Why not top-level RAII for R:

```text
- cnr3_arAllFramesReady() dispatches to branch helpers at
  src/cnr3_arAllFramesReady.cpp:2337-2372.
- Successful branch helpers call cnr3_discard_frame_data_with_cache() before
  returning the frame.
- That discard deletes the request_data the R record needs.
- Therefore a top-level arAllFramesReady destructor cannot safely read the plan
  facts after the branch helper returns.
```

Recommended R capture pattern:

```text
1. At top-level arAllFramesReady entry, sample R enter_tick outside any
   diagnostics mutex.
2. Store that tick in a diagnostic-only, gate-compiled frameData field, or pass
   it to the branch helper.
3. In each branch helper, copy the R facts before request_data is discarded.
4. After successful cnr3_discard_frame_data_with_cache(), emit the R record
   from copied facts.
5. Then return the already-owned VSFrame exactly as before.
```

My preference is **diagnostic-only frameData field** over changing all branch-helper signatures:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    Cnr3DiagPlanTraceTick ar_all_enter_tick{};
#endif
```

It keeps the true top-level arAllFramesReady entry tick without expanding the four branch helper signatures. This is a diagnostics-only field and compiles out with the master gate.

**Designer decision requested:** ratify diagnostic-only frameData enter_tick storage vs helper-signature extension.

---

## 2. Field availability confirmation

### 2.1 Set 1 strategy

Confirmed available from `Cnr3LiveGetFrameFrameData::branch` and recovery sub-branch.

Source:

```text
src/cnr3_plugin_internal.h:71-83
src/cnr3_plugin_internal.h:92-104
```

Population sites:

```text
cache_hit_return:
  src/cnr3_arInitial.cpp:53

frame0_fresh_start:
  src/cnr3_arInitial.cpp:136

predecessor_present_compute:
  src/cnr3_arInitial.cpp:100

recovery:
  src/cnr3_arInitial.cpp:515-520
```

Recovery exact/floor selection:

```text
src/cnr3_arInitial.cpp:477-487
```

No gap found.

### 2.2 Set 2 roles

Confirmed available or safely derivable.

```text
target:
  requested_frame in src/cnr3_plugin_internal.h:94
  populated at src/cnr3_arInitial.cpp:54, 101, 137, 517

predecessor:
  predecessor_frame in src/cnr3_plugin_internal.h:95
  predecessor-present path sets it at src/cnr3_arInitial.cpp:702
  recovery path sets it at src/cnr3_arInitial.cpp:518

anchor:
  recovery_plan in src/cnr3_plugin_internal.h:100
  planned by plan_bounded_recovery_search_and_record_anchor_pin at
  src/cnr3_arInitial.cpp:446-453
  moved into request_data at src/cnr3_arInitial.cpp:520

floor:
  recovery_floor_frame in src/cnr3_plugin_internal.h:101
  set at src/cnr3_arInitial.cpp:475-487 and 519

holes:
  recovery_plan.hole_frame_numbers
  floor hole numbers derived at src/cnr3_arInitial.cpp:522-535
  recovery source vector derived from holes at src/cnr3_arInitial.cpp:359-398
```

### 2.3 Sources derivation

Confirmed branch-specific derivation is required.

The non-recovery branches set `source_requested = true` but do **not** populate `source_request_frame_numbers`:

```text
cache_hit:
  src/cnr3_arInitial.cpp:53-56

predecessor_present:
  src/cnr3_arInitial.cpp:100-102

frame0:
  src/cnr3_arInitial.cpp:136-138
```

The vector is recovery-only and is filled by:

```text
src/cnr3_arInitial.cpp:359-398
```

The existing retrieval helper already uses the same fallback rule: if `source_request_frame_numbers` is empty, the requested source is the target frame itself.

```text
src/cnr3_arAllFramesReady.cpp:540-555
```

So the scope/spec rule is confirmed:

```text
CACHE_HIT / FRAME0 / PRED_PRESENT:
  sources = [n]

RECOVERY_EXACT / RECOVERY_FLOOR:
  sources = request_data.source_request_frame_numbers
```

This matches the spec’s branch-specific sources rule.

### 2.4 Pinned derivation

Confirmed: do **not** enumerate the private pin-list.

The public pin-list surface exposes only `empty()`, `pin_count()`, reservation, record, and discharge. It does not expose tokens/frame numbers:

```text
src/cnr3_cache_core.h:1783-1838
```

The private token vector is private:

```text
src/cnr3_cache_core.h:1840-1856
```

Therefore `pinned` must remain branch-derived, exactly as the spec says.

Derivations confirmed:

```text
CACHE_HIT:
  pinned = [n]
  lookup_frame_and_record_pin at src/cnr3_arInitial.cpp:667-668
  cache_hit_pin_taken set at src/cnr3_arInitial.cpp:55

PRED_PRESENT:
  pinned = [n - 1]
  predecessor_frame set at src/cnr3_arInitial.cpp:702
  predecessor pin lookup at src/cnr3_arInitial.cpp:709-713
  predecessor_pin_taken set at src/cnr3_arInitial.cpp:715-716

RECOVERY_EXACT:
  initial pinned = [anchor]
  recovery plan/pin produced at src/cnr3_arInitial.cpp:446-453
  exact-anchor validation checks anchor_found, anchor_pin_recorded, and pin_count
  at src/cnr3_arAllFramesReady.cpp:1327-1341

RECOVERY_FLOOR:
  pinned facts are result-time facts from floor/hole adoption or store-and-pin.
```

No public pin-list accessor is needed. Adding one would violate the scope fence.

### 2.5 Set 3 outcome

Confirmed from successful branch outcome regions:

```text
RETURNED_CACHE_HIT:
  cache-hit successful return at src/cnr3_arAllFramesReady.cpp:1047-1057

RETURNED_COMPUTED:
  predecessor-present successful return at src/cnr3_arAllFramesReady.cpp:1268-1292
  frame0 successful return at src/cnr3_arAllFramesReady.cpp:2286-2309

RETURNED_RECOVERED:
  recovery successful return at src/cnr3_arAllFramesReady.cpp:2027-2086

FAILED:
  not in 3c.1; belongs to 3c.2.
```

This matches the 3c.1 Set 3 subset in the spec.

### 2.6 Set 4 C/K/L/U/N

Confirmed for 3c.1 subset only: **C/K/L/U/N**. X/E stay fenced to 3c.2.

Available source facts:

```text
C/K/L/N enum:
  src/cnr3_plugin_internal.h:85-90

floor adopted-skipped:
  src/cnr3_arAllFramesReady.cpp:1400-1402

floor computed/post-compute-loser:
  src/cnr3_arAllFramesReady.cpp:1547-1550

hole adopted-skipped:
  src/cnr3_arAllFramesReady.cpp:1610-1613

hole computed/post-compute-loser:
  src/cnr3_arAllFramesReady.cpp:1820-1823
```

U derivation:

```text
U is not read from private pin-list contents.
It is derived after successful pin-list discharge from branch facts and
result-time outcomes.

cache_hit:
  successful discard at src/cnr3_arAllFramesReady.cpp:1020-1045
  U = [n] after discard success

predecessor_present:
  successful discard at src/cnr3_arAllFramesReady.cpp:1268-1281
  U = [predecessor_frame] after discard success

recovery:
  successful discard at src/cnr3_arAllFramesReady.cpp:2052-2065
  U = initial anchor/floor/hole pins derived from recovery branch facts and
      C/K/L outcomes after discard success

frame0:
  no plan pin, so no U item.
```

No gap found, but the patch must be careful to capture/copy R facts before deleting `request_data`.

---

## 3. Buffer home and concurrency model

Confirmed location: add a gated per-instance buffer to `Cnr3FilterData`, alongside the existing D-SUM stats.

Current location:

```text
src/cnr3_plugin_internal.h:29-64
```

Recommended member:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    Cnr3DiagPlanTraceBuffer dsum_plantrace{};
#endif
```

Concurrency model to patch:

```text
- Buffer type lives in cnr3_diagnostics.h/.cpp.
- Buffer has diagnostics-only std::mutex.
- enter_tick sampled outside the diagnostics mutex.
- action_seq incremented inside the diagnostics mutex, in the same critical
  section as vector append.
- Formatting and stderr emission happen outside the mutex.
- No cache/CMS mutex is used.
```

This directly preserves the spec’s load-bearing invariant.

Preallocated window:

```text
capacity = 2 * (to_frame - from_frame + 1)
```

The spec requires this windowed preallocated buffer and explicitly rejects ring/saturation machinery.

---

## 4. Time, sort keys, and widths

Confirmed design to implement:

```text
- Hot path stores steady_clock ticks.
- UTC display column derived at dump time from one per-instance steady/system
  anchor.
- View (a) sort key is exactly:
    enter_tick ASC, action_seq ASC
- phase is displayed in view (a), but is not a sort key.
```

This is directly controlled by spec v2.2.

Recommended width derivation:

```text
frame_width:
  max(8, decimal_digits(max(abs(from_frame), abs(to_frame))))

action_seq_width:
  max(8, decimal_digits(2 * window_frame_count))

tick_width:
  fixed 20 for uint64_t-ish externally sortable integer ticks, or
  max(20, decimal_digits(max_observed_tick)) at dump time.
```

For action sequence, because the buffer is window-bounded and records at most two records per in-window frame in 3c.1, `2 * window_frame_count` is the natural derivation. The formatting should never truncate; if the numeric value exceeds the nominal width, standard stream/printf width expansion is acceptable.

**Designer decision requested:** ratify these width rules, especially whether `tick_width` should be fixed or dump-derived.

---

## 5. Clean-end dump site

Confirmed clean-end dump site:

```text
src/vapoursynth-Cnr3.cpp:289-387
```

Current D-SUM summaries are already emitted in `cnr3_free_filter()` before `freeNode()` and `delete data`:

```text
D-SUM-01: src/vapoursynth-Cnr3.cpp:302-307
D-SUM-03: src/vapoursynth-Cnr3.cpp:308-313
D-SUM-06: src/vapoursynth-Cnr3.cpp:314-319
D-SUM-07: src/vapoursynth-Cnr3.cpp:320-325
D-SUM-09: src/vapoursynth-CNr3.cpp:344-349
D-SUM-14: src/vapoursynth-Cnr3.cpp:374-379
delete/free: src/vapoursynth-Cnr3.cpp:381-386
```

Recommended plan-trace dump placement:

```text
After the existing summaries or near the end of the diagnostic block, before:
  vsapi->freeNode(data->source)
  delete data
```

I recommend placing it **after D-SUM-14** so the aggregate summaries remain grouped first and the larger copy-pasteable plan-trace block appears last.

The 3c.1 clean-end dump must be once-guarded, but only the clean-end arm is implemented in this cycle. The bail arm is 3c.2 and must not be touched now. The fence is explicit.

---

## 6. Gate/family naming proposal

Scope asks coder to propose and coordinator/designer to ratify final naming.

Recommended build_config names:

```cpp
#define CNR3_DIAG_COMPUTE_DSUM_PLANTRACE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
#   define CNR3_DIAG_PRINT_DSUM_PLANTRACE 1
#endif
#if defined(CNR3_DIAG_PRINT_DSUM_PLANTRACE) && !defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
#   error "Cannot print DSUM_PLANTRACE without computing DSUM_PLANTRACE"
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
#   define CNR3_DIAG_DSUM_PLANTRACE_VIEW_DATETIME 1
#   define CNR3_DIAG_DSUM_PLANTRACE_VIEW_FRAME 1
#   define CNR3_DIAG_DSUM_PLANTRACE_VIEW_PHASE 1
#   define CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME 0
#   define CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME 120
#endif
```

Recommended output tag:

```text
[DSUM-PLANTRACE]
```

I do **not** recommend `[DSUM-SUMMARY]` for per-frame trace records. The scope also says this is per-frame TRACE, not a summary.

**Designer/coordinator decision requested:**

```text
Should this family remain nonnumeric DSUM-PLANTRACE, or should it be assigned
a numeric D-SUM ID such as D-SUM-15?
```

My recommendation: **use nonnumeric `DSUM-PLANTRACE`** to keep it visibly separate from summary families D-SUM-01 through D-SUM-14.

---

## 7. Expected file list

Confirmed expected file list is sufficient.

Files likely touched:

```text
src/cnr3_diagnostics.h
src/cnr3_diagnostics.cpp
src/cnr3_plugin_internal.h
src/cnr3_arInitial.cpp
src/cnr3_arAllFramesReady.cpp
src/vapoursynth-Cnr3.cpp
src/cnr3_build_config.h
src/cnr3_cache_core_selftest_main.cpp
```

This matches the scope’s expected list.

No expected need to touch:

```text
src/cnr3_cache_core.h
src/cnr3_cache_core.cpp
project files
VapourSynth registration logic beyond free-filter dump call
```

---

## 8. Scope discrepancies / cautions found

### 8.1 arAllFramesReady true entry timestamp needs an implementation choice

The scope/spec require R enter timestamp to represent function entry. A branch-local helper called inside `cnr3_complete_live_*` would otherwise capture helper-entry time, not top-level `cnr3_arAllFramesReady()` entry time.

Recommended solution:

```text
Sample R enter_tick at top-level cnr3_arAllFramesReady entry and store it in a
diagnostic-only frameData field before switch dispatch.
```

Designer decision requested.

### 8.2 R capture must avoid reading deleted request_data

Successful R exits commonly call `cnr3_discard_frame_data_with_cache()` before returning. That deletes `request_data`.

Therefore, the patch must:

```text
- copy plan/result facts before discard;
- perform successful discard;
- then append the R record from the copied facts;
- then return the VSFrame.
```

This applies especially to:

```text
cache_hit:
  src/cnr3_arAllFramesReady.cpp:1020-1057

predecessor_present:
  src/cnr3_arAllFramesReady.cpp:1268-1292

recovery:
  src/cnr3_arAllFramesReady.cpp:2027-2086

frame0:
  src/cnr3_arAllFramesReady.cpp:2286-2309
```

### 8.3 No public pin-list accessor should be added

Confirmed. The source supports the spec’s decision. `pinned` and `U` are branch/result-derived facts only.

### 8.4 3c.1 must not touch bail sites

Confirmed. This cycle must not touch, wrap, or write near `cnr3_set_filter_error` sites. Set 4 X/E, Set 5, and dump-on-bail are 3c.2.

---

## 9. Recommended designer decisions before patch

```text
D3C1-A
Approve 3c.1 standalone patching, with 3c.2 as the next cycle.

D3C1-B
Approve hybrid exit capture:
  O = top-level arInitial scope-exit guard.
  R = explicit branch-local successful-exit captures.

D3C1-C
Approve R enter_tick storage:
  sample at top-level arAllFramesReady entry, store in a diagnostic-only
  frameData field, then branch helpers read/copy it.

D3C1-D
Approve output tag and family naming:
  CNR3_DIAG_COMPUTE_DSUM_PLANTRACE
  CNR3_DIAG_PRINT_DSUM_PLANTRACE
  [DSUM-PLANTRACE]
  no numeric D-SUM ID unless coordinator wants one.

D3C1-E
Approve width derivation:
  frame_width = max(8, digits(max(abs(from), abs(to))))
  action_seq_width = max(8, digits(2 * window_frame_count))
  tick_width = fixed 20 or dump-derived; designer to choose.

D3C1-F
Approve dump placement:
  cnr3_free_filter(), after D-SUM-14 and before freeNode/delete.
```

## Bottom line

I recommend **proceeding to patch after designer ratification of D3C1-A through D3C1-F**. The scope is implementable as observe-only 3c.1 without touching bail sites, cache core, or project files.
