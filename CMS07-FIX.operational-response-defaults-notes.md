# CMS07-FIX.operational-response-defaults - patch notes

## Adds / changes

This patch changes one file:

- `src/vapoursynth-Cnr3.cpp`

It replaces the K.1E.2 proof-placeholder response-table defaults with operational vscnr2-style defaults:

- Y: threshold 35, strength 192, curve wide
- U: threshold 47, strength 255, curve narrow
- V: threshold 47, strength 255, curve narrow

It also emits the effective response configuration on the existing startup-provenance gate:

```text
CNR3[i] INFO CONFIG: response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow
```

The line is printed from the live `Cnr3ResponseTableConfig`, not directly from preprocessor literals.

## Deliberately out of scope

- No user option parsing.
- No `ln/lm/un/um/vn/vm/mode/scdthr` surface.
- No scene-change policy change.
- No blend/math/table/downsample/cache/PlanRetry change.
- No selftest changes.

## Important proof note

R-PROCESS-19 byte-identical output proof does not apply. This patch deliberately changes frame output.

The main behavioural proof is the existing 10-frame Y4M stackhorizontal/input-output comparison and `cnr3_y4m_chroma_report.py` delta analyser.

Expected post-patch shape:

- frames 8/9 should no longer be classified as `LIKELY_DESATURATION_BUG`, or U/V MAD and red/brown chroma magnitude loss should drop sharply;
- frames 0/2/4/6 should remain byte-identical passthrough;
- frames 1/3/5/7 should remain minor/smoothing-active rather than all becoming unchanged;
- startup log should include the `response_config` line above;
- `K1E2_PROOF_DEFAULT` should be grep-clean in active source.

## Apply sequence

From repo root on `dev_cache_manager`:

```bat
git status --short
git branch --show-current

git apply --check CMS07-FIX.operational-response-defaults.patch
git apply --check --whitespace=error CMS07-FIX.operational-response-defaults.patch
git apply CMS07-FIX.operational-response-defaults.patch

git diff --check
git status --short
```

## Build + baseline selftest

From `vs\cnr3`:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"

"%msbuild%" cnr3.slnx /m /p:Configuration=Debug /p:Platform=x64
"%msbuild%" cnr3.slnx /m /p:Configuration=Release /p:Platform=x64

x64\Debug\cnr3_cache_core_selftest.exe 1>NUL
echo Debug normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe 1>NUL
echo Release normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL
echo Release forced-fail exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL
echo Release verbose exit_code=%ERRORLEVEL%
```

Expected selftest count should remain unchanged from the current baseline.

## Grep checks

From repo root:

```bat
git grep -n "K1E2_PROOF_DEFAULT" -- src

git grep -n "CNR3_DEFAULT_" -- src/vapoursynth-Cnr3.cpp

git grep -n "response_config" -- src/vapoursynth-Cnr3.cpp
```

Expected:

- no active-source matches for `K1E2_PROOF_DEFAULT`;
- matches for `CNR3_DEFAULT_Y_THRESHOLD_8BIT`, `CNR3_DEFAULT_Y_STRENGTH_8BIT`, `CNR3_DEFAULT_UV_THRESHOLD_8BIT`, and `CNR3_DEFAULT_UV_STRENGTH_8BIT`;
- the startup `response_config` format string and call are present.

## Sandbox validation performed

Validated against the uploaded source snapshot available in this conversation.

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git diff --check after apply: PASS
K1E2_PROOF_DEFAULT grep in patched vapoursynth-Cnr3.cpp: 0
CNR3_DEFAULT_ grep in patched vapoursynth-Cnr3.cpp: present
response_config grep in patched vapoursynth-Cnr3.cpp: present
Cnr3ResponseCurveKind::wide use confirmed in table build path
```

No Visual Studio build or runtime selftest was performed in the sandbox.
