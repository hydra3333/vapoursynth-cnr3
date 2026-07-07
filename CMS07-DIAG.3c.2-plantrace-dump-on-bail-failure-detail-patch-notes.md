# CMS07-DIAG.3c.2 plantrace dump-on-bail failure detail — patch notes

## Scope

This patch is for designer diff review only. It applies on top of the committed DIAG.3c.1 source and implements DIAG.3c.2 according to the accepted confirm decisions:

- per-site additive FAILED-record calls, not a widened `cnr3_set_filter_error()` signature;
- explicit once-guarded bail-arm dump;
- master plantrace gate only;
- `E` means the actual frame the failing operation was on;
- AI-06 uses a code-derived category distinction between recovery refusal and discharge failure.

## Files changed

```text
src/cnr3_arInitial.cpp
src/cnr3_arAllFramesReady.cpp
src/vapoursynth-Cnr3.cpp
src/cnr3_diagnostics.h
src/cnr3_diagnostics.cpp
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_build_config.h
```

No cache-core files, project files, pin-list accessors, or production `Cnr3LiveRecoveryHoleOutcome` enum changes are included.

## What the patch adds

### Diagnostics vocabulary / format

- Adds `Cnr3DiagPlanTraceFailReason` with the 16 Set 5 categories.
- Adds `Cnr3DiagPlanTraceOutcome::failed`.
- Adds FAILED-record fields:
  - `fail_reason`
  - `not_reached_frames` (`X`)
  - `error_here_frames` (`E`)
- Extends result codes to emit `X` and `E` as plantrace-local codes.
- Extends the legend with `FAILED`, `X = not_reached`, and `E = error_here`.
- R records include `fail_reason=<Set5>` only when `outcome=FAILED`.

### Dump-on-bail

- Adds `cnr3_diag_plantrace_write_bail_dump_to_stderr()` using the same `buffer.dumped` once-guard as clean-end.
- Adds helper APIs to write a FAILED record and immediately trigger the bail dump.
- If clean-end later runs, it sees `dumped=true` and emits nothing.

### Site wiring

- Wires all 65 bail call sites with gated additive plantrace FAILED writes:
  - 14 arInitial sites via minimal FAILED records.
  - 50 arAllFramesReady sites via request/progress-aware FAILED records.
  - 1 top-level getFrame invalid-state site, guarded for `data != nullptr`.
- Existing `cnr3_set_filter_error(...)` calls and `return nullptr` paths remain in place.
- Success-path O/R capture remains untouched except for using the shared buffer once-guard.

### Recovery X/E derivation

- For recovery failures, progress is reconstructed from recovery floor and hole outcomes already present in `request_data`.
- `E` is the supplied actual failing frame.
- `X` is the unreached recovery-plan remainder: unreached holes plus the target if the target was not the failing item.
- Non-recovery branches emit `X=[]`.

### Discharge-failure handling

- The three arAllFramesReady discharge-failure sites use pre-discard copied progress fields and enter_tick, then emit a FAILED record after `discard_status` fails.
- AI-06 uses a code-derived category distinction:
  - `RECOVERY_PLAN_FAILED_OR_REFUSED` when the refusal path itself is clean;
  - `DISCHARGE_FAILED` when the refusal pin discharge fails.

### Selftest fixture

- Extends the plantrace reference dump fixture with a FAILED recovery record:
  - `outcome=FAILED`
  - `fail_reason=SOURCE_RETRIEVAL_FAILED`
  - `E` on the failing recovery item
  - non-empty `X` for unreached hole/target remainder
- Calls the bail-dump helper, then calls clean-end dump to exercise the once-guard no-duplicate path.

## Validation performed in sandbox

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
```

Changed files after apply:

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_arInitial.cpp
src/cnr3_build_config.h
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_diagnostics.cpp
src/cnr3_diagnostics.h
src/vapoursynth-Cnr3.cpp
```

Syntax-only validation with local minimal VapourSynth stubs:

```text
cnr3_diagnostics.cpp: PASS
cnr3_arInitial.cpp: PASS
cnr3_arAllFramesReady.cpp: PASS
cnr3_cache_core_selftest_main.cpp: PASS
```

Not performed in sandbox:

```text
VS2026 Debug|x64 build
VS2026 Release|x64 build
cache-core selftest execution
R-PROCESS-19 macro-off build/proof
S1/S7/S8 .vpy byte-identical proof
induced arInitial/arAllFramesReady bail proof
```

## Expected proof gate after designer diff review

1. Build Debug|x64 and Release|x64 with plantrace enabled.
2. Four-way selftest all-on.
3. Induced arInitial bail proof: minimal FAILED record, `fail_reason`, `E`, no O record, bail dump emitted once.
4. Induced recovery arAllFramesReady bail proof: prior O record, `outcome=FAILED`, correct `fail_reason`, `E`, non-empty `X`, progress-so-far, bail dump emitted before return.
5. Flush proof: no lost BEGIN/END tail on induced bail.
6. Macro-off proof: master plantrace gate OFF compiles all 3c.2 additions out; four-way identical; S1/S7/S8 byte-identical on/off.
7. Restored-all-on proof.
8. Clean-run S1/S7/S8 unchanged from 3c.1: no FAILED records and no `fail_reason`/X/E.
