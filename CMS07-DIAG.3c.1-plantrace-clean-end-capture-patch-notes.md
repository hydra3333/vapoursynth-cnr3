# CMS07-DIAG.3c.1 plantrace clean-end capture — patch notes

## Scope

Adds DIAG.3c.1 observe-only DSUM-PLANTRACE buffered capture and clean-end dump.

This patch implements 3c.1 only:

- O records at successful `arInitial` exits.
- R records at successful `arAllFramesReady` branch exits.
- Per-instance preallocated, window-bounded diagnostics buffer.
- Diagnostics-only mutex, with `enter_tick` sampled outside the mutex and `action_seq` incremented inside the same critical section as append.
- Clean-end dump in `cnr3_free_filter`, after D-SUM-14 and before `freeNode` / `delete data`.
- Three gated views: datetime, frame, phase.
- Nonnumeric `[DSUM-PLANTRACE]` record tag.
- Marker bump to `CMS07-DIAG.3c.1-plantrace-clean-end-capture`.

## Explicitly not included

- No `cnr3_set_filter_error` bail-site touches.
- No dump-on-bail arm.
- No Set 4 X/E.
- No Set 5 failure reason.
- No public pin-list accessor.
- No cache-core changes.
- No project-file changes.

## Designer decisions implemented

- D3C1-A: 3c.1 standalone patch.
- D3C1-B: hybrid capture.
  - O = top-level arInitial RAII guard, armed only after published-success conditions.
  - R = explicit branch-local captures on the four successful result paths.
- D3C1-C: top-level arAllFramesReady `enter_tick` stored in a diagnostic-only frameData field and copied before `cnr3_discard_frame_data_with_cache` deletes frameData.
- D3C1-D: gate names approved, nonnumeric `[DSUM-PLANTRACE]` used per coordinator ratification.
- D3C1-E: fixed 20-width tick formatting; approved frame/action widths.
- D3C1-F: clean-end dump after D-SUM-14, before node/data teardown.

## Successful R capture call sites

- cache-hit return: copies R facts before discard, emits after successful discard and before transfer return.
- predecessor-present compute: copies R facts before discard, emits after successful discard and trace emission.
- recovery: copies R facts before discard, emits after successful discard and trace emission.
- frame0 fresh start: copies R facts before discard, emits after discard and transfer observations.

## Changed files

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_arInitial.cpp
src/cnr3_build_config.h
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_diagnostics.cpp
src/cnr3_diagnostics.h
src/cnr3_plugin_internal.h
src/vapoursynth-Cnr3.cpp
```

## Sandbox validation performed

From a synthetic git repository containing the uploaded source under `src/`:

```text
git apply --check CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch: PASS
git apply --check --whitespace=error CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch: PASS
git apply CMS07-DIAG.3c.1-plantrace-clean-end-capture.patch: PASS
git diff --check: PASS
```

Syntax-only validation with local minimal VapourSynth API stubs:

```text
Plantrace ON:
  cnr3_diagnostics.cpp: PASS
  cnr3_cache_core_selftest_main.cpp: PASS
  cnr3_arInitial.cpp: PASS
  cnr3_arAllFramesReady.cpp: PASS
  vapoursynth-Cnr3.cpp: PASS

Plantrace master gate OFF:
  cnr3_diagnostics.cpp: PASS
  cnr3_cache_core_selftest_main.cpp: PASS
  cnr3_arInitial.cpp: PASS
  cnr3_arAllFramesReady.cpp: PASS
  vapoursynth-Cnr3.cpp: PASS
```

Not performed in sandbox:

- VS2026 Debug/Release builds.
- cache-core selftest executable runs.
- `.vpy` byte-identical macro-off proof.
- S-series real-run content proof.

## Expected proof gate after applying locally

Use the DIAG.3c.1 scope proof gate:

1. Four-way all-on.
2. Master gate off macro-off proof.
3. Presence/absence proof for `[DSUM-PLANTRACE]`.
4. S-series content sanity with window chosen to include the relevant frames.
5. Prior DSUM01-14 unchanged.

