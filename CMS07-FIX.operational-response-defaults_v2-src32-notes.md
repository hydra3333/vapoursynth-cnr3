# CMS07-FIX.operational-response-defaults v2-src32 notes

Baseline used for patch validation: latest uploaded `src(32).zip`.

Marker on success: `CMS07-FIX.operational-response-defaults`.

Scope:
- One changed file: `src/vapoursynth-Cnr3.cpp`.
- Replace K.1E.2 proof-placeholder response-table defaults with operational vscnr2-style defaults.
- Emit the effective response config from the live table config on the existing startup provenance gate.
- Deliberately changes output; R-PROCESS-19 byte-identical proof does not apply.

Operational defaults:
- Y: threshold 35, strength 192, curve wide.
- U: threshold 47, strength 255, curve narrow.
- V: threshold 47, strength 255, curve narrow.

Out of scope:
- User option parsing.
- scene_chroma policy.
- blend math, table math, downsample, cache, PlanRetry, selftests.

Sandbox validation against `src(32).zip`:
- `git apply --check`: PASS.
- `git apply --check --whitespace=error`: PASS.
- `git diff --check` after apply: PASS.
- `K1E2_PROOF_DEFAULT` grep in patched `src/vapoursynth-Cnr3.cpp`: 0.
- `CNR3_DEFAULT_` grep in patched `src/vapoursynth-Cnr3.cpp`: present.
- `response_config` grep in patched `src/vapoursynth-Cnr3.cpp`: present.
- `Cnr3ResponseCurveKind::wide` / `wide_response` use present in source.

Required local proof:
1. Apply from repo root on `dev_cache_manager`.
2. Build Debug|x64 and Release|x64.
3. Run the canonical 4-way selftest; selftest count should be unchanged.
4. Re-run the exact 10-frame Y4M input/output comparison and the Python delta analyser.
5. Confirm the startup provenance log contains:
   `response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow`.
6. Eyeball the stackhorizontal clip; brown/red should hold saturation.
