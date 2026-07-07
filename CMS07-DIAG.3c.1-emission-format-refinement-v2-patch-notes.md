# CMS07-DIAG.3c.1 emission-format refinement v2 — patch notes

## Intent

This is a replacement v2 delta patch to apply on top of the already-applied
`CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch` source.

It supersedes the first emission-format refinement patch:

```text
CMS07-DIAG.3c.1-emission-format-refinement.patch
```

Do not apply both. Apply this v2 patch instead.

The patch revises the DSUM-PLANTRACE writer/dump layer to match spec v2.3 and
the emission-refinement scope. It is still part of the single DIAG.3c.1 commit;
it is not intended to be committed separately as 3c.1b.

## v2 replacement reason

Designer finding D3C1E-1 identified build-safety risk from the first refinement
patch: after removing `unpinned_frames` population, two result helper functions
kept parameters that were no longer used.

Under MSVC /W4 this can produce C4100 unused-parameter warnings, and if the
project is built with warnings-as-errors the proof build can fail before tests
run.

v2 applies the preferred clean fix:

```text
- Drop the now-dead parameter from cnr3_diag_plantrace_make_cache_hit_result().
- Drop predecessor_frame and predecessor_was_pinned from
  cnr3_diag_plantrace_make_computed_result().
- Update all corresponding call sites.
```

No `(void)` suppression is used.

`predecessor_frame_for_trace` remains live because it is still used by the
existing `cnr3_trace_live_predecessor_present_compute()` call. It is not a dead
plantrace-only local in the current source.

## What this patch changes

- Renders each O/R record as one physical ASCII line with fixed-schema fields.
- Zero-pads only the external sort keys: `enter_tick`, `exit_tick`, `seq`, and
  `frame`.
- Leaves role-list and `codes=[...]` frame numbers unpadded.
- Removes the three in-plugin sorted views and their build-config sub-gates:
  `CNR3_DIAG_DSUM_PLANTRACE_VIEW_DATETIME`,
  `CNR3_DIAG_DSUM_PLANTRACE_VIEW_FRAME`, and
  `CNR3_DIAG_DSUM_PLANTRACE_VIEW_PHASE`.
- Emits one natural-order block in buffer/action-sequence order.
- Emits one column-aligned ASCII legend at the head of the block.
- Brackets each instance block with:
  - `BEGIN schema=3c1v1 instance=<n> window=[from,to] records=<count>`
  - `END   schema=3c1v1 instance=<n> records=<count> truncated=<0|1>`
- Renames the rendered L label to `post_compute_discarded`.
- Removes `unpinned_frames`, `unpinned=`, and U from the plantrace result record
  and plantrace codes.
- Removes now-dead helper parameters left behind by the U/unpinned deletion.

## What this patch deliberately does not change

- No arInitial RAII guard changes.
- No arAllFramesReady branch-local observe-result invocation changes, except
  removal of now-dead `unpinned_frames` diagnostic list population and the now-
  dead helper parameters/call arguments.
- No `plantrace_ar_all_enter_tick` sampling or frameData field changes.
- No enter_tick/action_seq lock-invariant changes.
- No window-bound or reserve/reserve_failed logic changes.
- No bail-site touches.
- No dump-on-bail.
- No Set 4 X/E.
- No Set 5 failure-reason.
- No cache-core changes.
- No project-file changes.

## Confirm findings against the current 3c.1 patched source

- The emission/dump code is centralized in `cnr3_diagnostics.cpp` and can be
  rewritten as a single natural-order dump without touching capture mechanics.
- The three `VIEW_*` gates are only the build-config sub-flags and writer-side
  view guards; no stale `VIEW_*` references remain after this patch.
- `unpinned_frames` feeds only DSUM-PLANTRACE result-field emission/codes and
  the selftest reference fixture. Removing its pushes changes no cache, pin,
  discharge, or ownership behaviour.
- The removed `frame_number`, `predecessor_frame`, and `predecessor_was_pinned`
  helper parameters were used only for the removed `unpinned_frames` diagnostic
  population.
- The source enum `Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser`
  remains unchanged; only the rendered label changes to
  `post_compute_discarded`.
- BEGIN/END are emitted per instance and `truncated=` is wired to
  `reserve_failed`.
- The 3c.1 fence remains intact.

## Changed files

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_build_config.h
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_diagnostics.cpp
src/cnr3_diagnostics.h
```

## Sandbox validation performed

Validation was performed on a sandbox tree reconstructed as:

1. unzip the current post-DIAG.3b source snapshot;
2. apply `CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch`;
3. apply this v2 delta patch.

Results:

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
```

Syntax-only validation with local minimal VapourSynth API stubs:

```text
plantrace ON:
  src/cnr3_diagnostics.cpp: PASS
  src/cnr3_cache_core_selftest_main.cpp: PASS
  src/cnr3_arAllFramesReady.cpp: PASS

plantrace OFF:
  src/cnr3_diagnostics.cpp: PASS
  src/cnr3_cache_core_selftest_main.cpp: PASS
  src/cnr3_arAllFramesReady.cpp: PASS
```

Additional build-safety check:

```text
No remaining references to:
  unpinned_frames
  CNR3_DIAG_DSUM_PLANTRACE_VIEW_DATETIME
  CNR3_DIAG_DSUM_PLANTRACE_VIEW_FRAME
  CNR3_DIAG_DSUM_PLANTRACE_VIEW_PHASE

cnr3_diag_plantrace_make_cache_hit_result() has no unused parameter.
cnr3_diag_plantrace_make_computed_result(int frame_number, Cnr3Status store_status)
has no unused predecessor parameters.
predecessor_frame_for_trace remains live for the existing KDT trace call.
```

Not performed in sandbox:

```text
VS2026 Debug|x64 build
VS2026 Release|x64 build
cnr3_cache_core_selftest.exe runs
.vpy byte-identical macro-off proof
S1/S8 real-run format/content proof
```

## Expected local proof gate

After applying, run the revised DIAG.3c.1 proof gate:

```text
1. Four-way all-on:
   Debug normal 56/56 PASS exit 0
   Release normal 56/56 PASS exit 0
   Release forced-fail 55/56 FAIL exit 1
   Release verbose 56/56 PASS exit 0

2. Macro-off:
   CNR3_DIAG_COMPUTE_DSUM_PLANTRACE disabled => plantrace compiles out,
   four-way identical, [DSUM-PLANTRACE] absent.

3. Restored all-on:
   gate restored, four-way identical, [DSUM-PLANTRACE] present.

4. S1/S8 format/content proof:
   one BEGIN/END-bracketed natural-order block per instance;
   legend once;
   records are one physical line;
   fixed schema with empty=[];
   no unpinned= and no U;
   L renders as post_compute_discarded;
   padding only on enter_tick/exit_tick/seq/frame;
   records sort cleanly by external keys.
```
