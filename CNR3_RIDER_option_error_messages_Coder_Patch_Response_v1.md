# CNR3 - Coder Patch Response: RIDER option-error-messages

**Response version:** v1  
**Phase marker on success:** `CMS07-RIDER.option-error-messages`  
**Baseline:** committed `CMS07-FEATURE.cnr2-descriptive-option-parser`  
**Patch:** `CMS07-RIDER.option-error-messages.patch`  
**Patch status:** ready for designer review and coordinator application/testing

## 1. Outcome

The designer D1/D2/D3 rulings are implemented in a standalone patch cut against the exact uploaded `src.zip` baseline.

The patch changes message construction only. It does not change which option values are accepted or rejected, parser short-circuiting, defaults, resolved configuration, pixel processing, cache/recovery behaviour, filter mode, cache profile, or PlanRetry state.

The STOP condition was not triggered during implementation or the source-fragment parser probe.

## 2. Designer rulings implemented

### 2.1 D1-a - shortest round-trip float echo

Implemented the preferred `std::to_chars` path with no format or precision argument.

- Finite values use shortest round-trip formatting.
- `100.0001` renders as `100.0001`.
- `-0.1` renders as `-0.1`.
- `101.0` renders as `101`.
- NaN and infinities are handled explicitly as `nan`, `inf`, or `-inf`.
- A failed `to_chars` operation falls back to the fixed text `<format-error>`.
- The local value buffer is 64 bytes and the output range reserves one byte for the terminating NUL.

This is D1-a, not the `%.17g` fallback.

### 2.2 D2 - API-size-bounded curve echo

The code never passes raw user data directly to `%s`.

`cnr3_format_curve_option_echo()`:

1. Clamps a non-positive `value_size` to an empty echo.
2. Copies at most 32 bytes, based on the API-returned `value_size`.
3. NUL-terminates the local echo buffer itself.
4. Retains a defensive nullptr path.

A non-NUL-terminated data element is therefore read only within its reported size.

### 2.3 D3 - one-line sanitisation and truncation

Each copied curve byte outside printable ASCII `0x20..0x7E` is replaced with `?`.

The selected truncation form is:

```text
got "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa...", expected exactly "wide" or "narrow".
```

That is 32 sanitised input bytes followed by `...`, with the marker inside the quotes. The marker is appended only when the API-reported `value_size` is greater than 32.

### 2.4 TRAP 1 choice (b) - bool expectation override

The approved optional expectation override was added to `cnr3_parse_optional_int_option()`.

`cnr3_parse_optional_bool_option()` still delegates to the integer parser with the same `0..1` validation. It now supplies:

```text
expected 0 or 1.
```

Resulting messages are:

```text
CNR3: invalid scene_chroma option: got 2, expected 0 or 1.
CNR3: invalid scene_chroma option: incorrect value type, expected 0 or 1.
```

The bool parser was not rewritten and its validation structure is unchanged.

## 3. Exact changed files

The patch contains exactly these two files:

```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

Patch stat:

```text
src/cnr3_build_config.h  |   2 +-
src/vapoursynth-Cnr3.cpp | 188 +++++++++++++++++++++++++++++++++++++++++++----
2 files changed, 173 insertions(+), 17 deletions(-)
```

No README, harness, project, cache, pixel, diagnostics, or other source file is present in the patch.

## 4. Patch contents

### 4.1 `src/cnr3_build_config.h`

Marker-only change:

```text
CMS07-FEATURE.cnr2-descriptive-option-parser
```

to:

```text
CMS07-RIDER.option-error-messages
```

The shipping configuration remains:

```text
CNR3_FILTER_MODE_PARALLEL_REQUESTS  ON
CNR3_CACHE_PROFILE_HALF            ON
CNR3_ENABLE_PLAN_RETRY_BIAS        OFF
```

### 4.2 `src/vapoursynth-Cnr3.cpp`

Adds:

- `<charconv>` for D1-a shortest round-trip formatting;
- shared integer, float, and curve expectation builders;
- shortest round-trip float echo formatting;
- `value_size`-bounded curve copying, printable-ASCII sanitisation, and truncation marking;
- the optional integer expectation override used only by the bool delegate.

Changes the existing type-error paths to:

```text
incorrect value type, <shared expectation>
```

Changes the existing range/value-error paths to:

```text
got <received value>, <shared expectation>
```

The same expectation buffer constructed once in each parser invocation is reused by both its type-error and range/value-error branches.

## 5. Validation and control-flow non-changes

Cold diff review confirms the patch does not change:

- `cnr3_option_present_once()` behaviour or its `expected exactly one value.` message;
- any `mapNumElements`, `mapGetInt`, `mapGetFloat`, `mapGetData`, or `mapGetDataSize` call;
- any accepted bounds;
- the finite/range predicates;
- `cnr3_data_equals_literal()`;
- the exact accepted curve literals `wide` and `narrow`;
- threshold-zero acceptance;
- resolved-value assignments;
- the eleven-option short-circuit `&&` chain;
- option defaults or application;
- response configuration emission;
- pixel, cache, recovery, PlanRetry, or filter-mode behaviour.

## 6. Fixed-buffer analysis

Existing fixed buffers remain unchanged:

```text
detail[128]
message[256]
```

New local buffers are:

```text
expectation[96]
float value_text[64]
curve value_text[36]   // 32 bytes + "..." + NUL
```

The final detail formatting also has explicit `%s` precision limits, rather than relying only on source-buffer sizes:

```text
incorrect value type, %.96s
got %lld, %.96s
got %.32s, %.80s
got "%.35s", %.80s
```

These bounds keep every constructed detail within `detail[128]`. The longest current curve detail is 79 characters; the complete `y_curve` error message is 109 characters, within `message[256]`.

All writes are bounded. Raw curve data is copied only within the API-reported size and is never scanned for a NUL terminator.

## 7. Sandbox validation performed

### 7.1 Baseline and scope checks

- Uploaded baseline marker found: `CMS07-FEATURE.cnr2-descriptive-option-parser`.
- Shipping mode/profile/bias state matched the controlling documents.
- Patch was generated from the actual uploaded source, not from a reconstructed baseline.

### 7.2 Patch integrity

All passed:

```text
git apply --check --ignore-whitespace
git apply --check --whitespace=error
patch -p1 --dry-run --binary
git diff --check
```

A fresh application produced exactly:

```text
M src/cnr3_build_config.h
M src/vapoursynth-Cnr3.cpp
```

The two applied files were byte-compared with the intended modified files and matched.

Patch metadata:

```text
file:   CMS07-RIDER.option-error-messages.patch
lines:  288
SHA256: 030ce419c0198486b74e65ad5a994d1f5fcebcd0f0cfea064763fbfe928fb973
```

### 7.3 Exact source-fragment parser probe

The option-error helpers and parsers were extracted directly from the modified source into a mock-VSAPI probe and compiled with:

```text
C++20
-Wall -Wextra -Wpedantic -Werror
AddressSanitizer
UndefinedBehaviorSanitizer
```

Result:

```text
37/37 probe checks PASS
sanitizer stderr: empty
```

The probe exercised:

- integer type, high-range, and low-range rejection;
- integer `threshold == 0` acceptance;
- bool type/range rejection and valid acceptance;
- float `100.0001`, `-0.1`, infinity, and type rejection;
- a non-NUL-terminated curve value;
- a control-character curve value and one-line output;
- a 200-byte curve value and the selected truncation marker;
- rejection of old cnr2 curve spelling `o`;
- valid `wide` without trailing NUL;
- valid `narrow` with trailing NUL;
- curve type rejection;
- unchanged multiple-value rejection;
- unchanged absent-option handling.

Representative observed messages:

```text
CNR3: invalid scene_threshold option: got 100.0001, expected a finite float in the range 0.0..100.0 inclusive.
CNR3: invalid scene_threshold option: got -0.1, expected a finite float in the range 0.0..100.0 inclusive.
CNR3: invalid y_curve option: got "bad?value", expected exactly "wide" or "narrow".
CNR3: invalid y_curve option: got "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa...", expected exactly "wide" or "narrow".
```

## 8. Validation not performed in the sandbox

The sandbox does not contain the project VapourSynth headers, VS2026 environment, plugin build projects, or the designer-owned runtime harnesses. In accordance with the standing process, I did not reconstruct or chase those dependencies.

Therefore the following remain for W3X/W3D:

- VS2026 Debug x64 rebuild;
- VS2026 Release x64 rebuild;
- canonical four-way selftest;
- live invalid-option runs and no-frame/non-zero-exit checks;
- designer harness checks for the non-NUL data element where available;
- no-args BEFORE/AFTER byte-identical output anchor;
- live marker and `response_config:` emission checks.

No VS2026 or VapourSynth runtime PASS is claimed by this response.

## 9. Required coordinator/designer proof gate

Use the enriched-v3 PART 9 gate plus the D1/D2/D3 additions. Expected selftest state remains configuration-dependent 57/57, with forced-fail 56/57 and exit 1 under the stated configuration.

Additional ruling-specific checks:

```text
scene_threshold=100.0001 -> echo contains 100.0001
scene_threshold=-0.1     -> echo contains -0.1 (D1-a)
non-NUL curve data       -> bounded correct echo, no overrun
y_curve="bad\nvalue"     -> one line, bad?value
200-byte curve value     -> 32 sanitised bytes + ... inside quotes
```

If any invalid value is accepted, stop: that is a parser bugfix outside this rider.

## 10. Apply block

From the repository root on `dev_cache_manager`:

```bat
git status --short
git apply --check --ignore-whitespace CMS07-RIDER.option-error-messages.patch
git apply --ignore-whitespace CMS07-RIDER.option-error-messages.patch
git diff --check
git status --short
```

Fallback:

```bat
patch -p1 --binary < CMS07-RIDER.option-error-messages.patch
```

Do not use `git stash`. `git apply` is all-or-nothing; verify `git status --short` after application.

## 11. Designer review request

Please review the standalone patch against:

- enriched scope v3 as amended by D1/D2/D3;
- message-only/no-validation-change boundary;
- shared expectation anti-drift requirement;
- D1-a shortest round-trip implementation;
- D2 `value_size`-bounded raw-data handling;
- D3 sanitisation and selected `...` truncation marker;
- exact two-file patch boundary.
