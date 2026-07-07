# CMS07-DIAG.3c.1 emission-format refinement — patch notes

## Intent

This is a delta patch to apply on top of the already-applied
`CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch` source. It revises the
DSUM-PLANTRACE writer/dump layer to match spec v2.3 and the emission-refinement
scope.

It is not a replacement for the 3c.1 capture patch and is not intended to be
committed separately. After this delta is applied and proven, commit 3c.1 once
in the revised form.

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

## What this patch deliberately does not change

- No arInitial RAII guard changes.
- No arAllFramesReady branch-local observe-result invocation changes, except
  removal of now-dead `unpinned_frames` diagnostic list population.
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
3. apply this delta patch.

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
