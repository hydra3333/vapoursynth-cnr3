# CNR3 RIDER.option-error-messages — Coder Confirm-Before-Patch Report

**Phase:** `CMS07-RIDER.option-error-messages`  
**Topic:** Self-explanatory option error messages  
**Scope authority:** `CNR3_Patch_Scope_OptionErrorMessages_ENRICHED_v3.md`  
**Baseline marker:** `CMS07-FEATURE.cnr2-descriptive-option-parser`  
**Status:** Confirm-before-patch report for designer/coordinator review  
**Patch status:** **No patch produced in this pass.** The enriched scope requires this report to be reviewed before patching, and the confirm pass found three decisions that must be resolved first.

---

## 1. Source and authority basis

I used:

1. `CNR3_Patch_Scope_OptionErrorMessages_ENRICHED_v3.md` — the attached copy and the copy inside the updated `FINAL_DOCS.zip` are byte-identical.
2. `Document_A_CNR3_Project_Context_and_Standing_Rules_v5_0.md`.
3. `cnr3_cache_manager_design_v7_15.md` (CMS07.15), including the Design Alignment and Escalation Charter.
4. The actual uploaded `src.zip`, extracted without reconstructing or modifying the baseline.

Source archive identity:

```text
src.zip SHA-256:
8fe2e3680f6a89ebd3a1754b10a05ce1e14485222836991998956c0f5598b27b
```

Enriched-scope identity:

```text
attached enriched v3 SHA-256:
5b54b0b1dc6ebba5756ea36322c9198216370c66ea04b0ab2a4042eebc844835

FINAL_DOCS enriched v3 SHA-256:
5b54b0b1dc6ebba5756ea36322c9198216370c66ea04b0ab2a4042eebc844835
```

The uploaded baseline marker is correct:

```text
src/cnr3_build_config.h:98
#define CNR3_EDIT_VERSION_LITERAL "CMS07-FEATURE.cnr2-descriptive-option-parser"

src/cnr3_build_config.h:100-101
inline constexpr const char* CNR3_EDIT_VERSION =
CNR3_EDIT_VERSION_LITERAL CNR3_SELECTED_FILTER_MODE_TEXT_SUFFIX;
```

The shipping configuration is also the required one and must remain untouched:

```text
src/cnr3_build_config.h:62
#define CNR3_FILTER_MODE_PARALLEL_REQUESTS 1

src/cnr3_build_config.h:120
#define CNR3_CACHE_PROFILE_HALF 1

src/cnr3_build_config.h:138
//#define CNR3_ENABLE_PLAN_RETRY_BIAS 1
```

---

## 2. Executive finding

The enriched scope correctly identifies the committed parser and its current message sites. Static source inspection found **no invalid-option acceptance bug**:

- integer options reject typed-getter failures and values outside their passed inclusive bounds;
- `scene_threshold` rejects typed-getter failures, non-finite values, and values outside `0.0..100.0`;
- curve options accept only exact `wide` or `narrow` data;
- `scene_chroma` still delegates to the integer parser with bounds `0..1`;
- the short-circuit `&&` chain still reports only the first bad option;
- threshold zero remains valid and is safely handled by the response-table builder.

Therefore STOP condition 1 is **not triggered by the uploaded source**.

The intended message-only patch is structurally feasible, including shared expectation text and TRAP 1 option **(b)**. However, the confirm pass found three issues in the prescribed formatting details:

1. Plain `%g` does **not** preserve the named near-miss `100.0001`; it renders it as `100` with default precision.
2. Bare `%.32s` is not bounded by the `value_size` returned by VapourSynth and is not provably safe for a non-NUL-terminated data element.
3. Raw `%s` echoing does not guarantee the required one-line message when the supplied data contains CR/LF or other control bytes.

These are not reasons to change validation. They are message-construction issues within the rider, but they require an explicit designer ruling before a patch is cut.

---

## 3. Reconciliation of the current helpers and detail strings

### 3.1 Common option-error wrapper

Confirmed exactly as described by the scope:

```text
src/vapoursynth-Cnr3.cpp:478-493
```

It owns `char message[256]`, formats:

```text
CNR3: invalid <option_name> option: <detail>
```

and sends the result through `mapSetError` via `cnr3_set_create_error`.

### 3.2 `cnr3_option_present_once`

```text
Function: src/vapoursynth-Cnr3.cpp:495-523
Current multiple-value detail: src/vapoursynth-Cnr3.cpp:513-518
    "expected exactly one value."
```

This path is already self-explanatory and is outside the four type/value message families. It should remain unchanged.

### 3.3 `cnr3_parse_optional_int_option`

```text
Function: src/vapoursynth-Cnr3.cpp:525-567
Typed getter: src/vapoursynth-Cnr3.cpp:544-545
Current type-error detail: src/vapoursynth-Cnr3.cpp:547-549
    "expected an integer value."
Current range check: src/vapoursynth-Cnr3.cpp:552
Current range detail: src/vapoursynth-Cnr3.cpp:553-561
    "expected an integer in the range %d..%d inclusive."
```

The getter returns `std::int64_t`; `%lld` is appropriate for the received value after the project’s normal explicit cast discipline, if required by MSVC’s format checking.

### 3.4 `cnr3_parse_optional_bool_option`

```text
Function: src/vapoursynth-Cnr3.cpp:569-592
Delegation to the int parser: src/vapoursynth-Cnr3.cpp:578-586
Bounds passed: 0 and 1 at src/vapoursynth-Cnr3.cpp:583-584
Conversion only after parser success: src/vapoursynth-Cnr3.cpp:590
```

The scope’s TRAP 1 is correct: bool does not parse independently and must not be rewritten to do so.

### 3.5 `cnr3_parse_optional_float_option`

```text
Function: src/vapoursynth-Cnr3.cpp:594-636
Typed getter: src/vapoursynth-Cnr3.cpp:613-614
Current type-error detail: src/vapoursynth-Cnr3.cpp:616-618
    "expected a float value."
Current finite/range check: src/vapoursynth-Cnr3.cpp:621
Current range detail: src/vapoursynth-Cnr3.cpp:622-630
    "expected a finite float in the range %.1f..%.1f inclusive."
```

Typed-getter failure and finite/range failure are already cleanly distinguishable, so STOP condition 3 is not triggered.

### 3.6 `cnr3_parse_optional_curve_option`

```text
Function: src/vapoursynth-Cnr3.cpp:663-703
Data getter: src/vapoursynth-Cnr3.cpp:680-681
Data-size getter: src/vapoursynth-Cnr3.cpp:683-684
Current type-error condition/detail: src/vapoursynth-Cnr3.cpp:686-688
    "expected string value \"wide\" or \"narrow\"."
Exact wide acceptance: src/vapoursynth-Cnr3.cpp:691-694
Exact narrow acceptance: src/vapoursynth-Cnr3.cpp:696-699
Current value-error detail: src/vapoursynth-Cnr3.cpp:701
    "expected exactly \"wide\" or \"narrow\"."
```

The type path and value path are cleanly distinguishable. The received pointer and received size are both available at the value-error site.

---

## 4. R-PROCESS-31 parameter-surface gap analysis

Every existing parse/default/validate/apply element is present. The only gap for all eleven options is the **error-detail construction** specified by this rider.

| Option | Parse | Default | Validate | Apply | Rider gap |
|---|---|---|---|---|---|
| `y_threshold` | EXISTS `vapoursynth-Cnr3.cpp:714` | EXISTS `:438` = 35 | EXISTS int helper `:525-567`, bounds `0..255` at `:714` | EXISTS table config `:812` | Message text only |
| `y_strength` | EXISTS `:715` | EXISTS `:439` = 192 | EXISTS int helper `:525-567`, bounds `0..255` at `:715` | EXISTS `:813` | Message text only |
| `u_threshold` | EXISTS `:716` | EXISTS `:440` = 47 | EXISTS int helper `:525-567`, bounds `0..255` at `:716` | EXISTS `:816` | Message text only |
| `u_strength` | EXISTS `:717` | EXISTS `:441` = 255 | EXISTS int helper `:525-567`, bounds `0..255` at `:717` | EXISTS `:817` | Message text only |
| `v_threshold` | EXISTS `:718` | EXISTS `:442` = 47 | EXISTS int helper `:525-567`, bounds `0..255` at `:718` | EXISTS `:820` | Message text only |
| `v_strength` | EXISTS `:719` | EXISTS `:443` = 255 | EXISTS int helper `:525-567`, bounds `0..255` at `:719` | EXISTS `:821` | Message text only |
| `y_curve` | EXISTS `:720` | EXISTS `:444` = wide | EXISTS exact data validation `:663-703` | EXISTS `:814`; curve consumed in `cnr3_response_tables.cpp:184-194` | Message text only |
| `u_curve` | EXISTS `:721` | EXISTS `:445` = narrow | EXISTS exact data validation `:663-703` | EXISTS `:818`; consumed at `cnr3_response_tables.cpp:184-194` | Message text only |
| `v_curve` | EXISTS `:722` | EXISTS `:446` = narrow | EXISTS exact data validation `:663-703` | EXISTS `:822`; consumed at `cnr3_response_tables.cpp:184-194` | Message text only |
| `scene_threshold` | EXISTS `:723` | EXISTS `:447`; source default resolves to 10.0 via `cnr3_build_config.h:180` | EXISTS float helper `:594-636`, bounds `0.0..100.0` at `:723` | EXISTS scene config `:853-863` | Message text only |
| `scene_chroma` | EXISTS `:724` | EXISTS `:448` = false | EXISTS bool delegation `:569-592` to int bounds `0..1` | EXISTS scene config argument `:862`, stored in `cnr3_frame_processing.cpp:1100` | Message text only |

Additional static confirmations:

```text
Short-circuit first-error chain:
  src/vapoursynth-Cnr3.cpp:713-724

Threshold zero remains valid at parse:
  bounds are 0..255 at src/vapoursynth-Cnr3.cpp:714-719

Threshold-zero table handling remains safe:
  src/cnr3_response_tables.cpp:50-53
```

No parse, default, validation, application, resolved configuration, response-table, scene-change, pixel, cache, recovery, filter-mode, or logging-provenance gap belongs in this patch.

---

## 5. TRAP 1 choice: choose **(b)** — optional expectation override

I choose **TRAP 1 option (b)**.

It is clean against the uploaded source because:

1. `cnr3_parse_optional_int_option` is file-local in `vapoursynth-Cnr3.cpp`; there is no exported declaration or cross-translation-unit ABI.
2. Its only callers are the six direct integer option calls at `:714-719` and the bool delegation at `:578-586`.
3. An optional trailing expectation override can leave all six ordinary integer calls textually unchanged.
4. `cnr3_parse_optional_bool_option` can continue to call the same integer helper with the same `0..1` bounds and pass only the message expectation override.
5. Getter, range predicate, resolution assignment, control flow, and accepted/rejected set remain unchanged.

Recommended shape:

```text
- ordinary int call: no override -> shared expectation is
  "expected an integer in the range <min>..<max> inclusive."

- bool delegation: full expectation override ->
  "expected 0 or 1."
```

Both the int helper’s type path and range path consume the same already-built expectation text. Bool therefore gets:

```text
incorrect value type, expected 0 or 1.
got 2, expected 0 or 1.
```

without independently parsing the option.

---

## 6. Shared expectation-text feasibility

All four kinds can share one expectation string between their type and value paths without changing validation.

### Integer

Build the expectation once after presence is established and before `mapGetInt`. Reuse it for:

```text
incorrect value type, <expectation>
got <int64>, <expectation>
```

The optional override supplies the bool form while retaining the same int validation path.

### Float

Build the bounds expectation once after presence is established and before `mapGetFloat`. Reuse it for:

```text
incorrect value type, <expectation>
got <received-float>, <expectation>
```

### Curve

Build/copy the constant expectation once before the data getters or immediately after presence is established. Reuse it for:

```text
incorrect value type, <expectation>
got "<safe bounded echo>", <expectation>
```

### Bool

Bool uses the integer helper’s already-shared expectation path with the optional full-tail override. No separate typed getter or range predicate is introduced.

This satisfies the enriched scope’s anti-drift requirement.

---

## 7. Fixed-buffer capacity analysis

The requested messages fit comfortably in the existing buffers when formatting is bounded correctly.

Conservative exploratory lengths, including wider values than the actual option call sites use:

| Case | Maximum tested `detail` length | With longest relevant option prefix | Capacity |
|---|---:|---:|---:|
| int range, int64 received and full 32-bit min/max bounds | 93 | 131 | `detail[128]`, `message[256]` |
| int type, full 32-bit min/max bounds | 89 | 127 | fits |
| bool range | 42 | 80 | fits |
| bool type | 38 | 76 | fits |
| float range, 17-significant-digit received double | under 90 | under 130 | fits |
| float type | 80 | 118 | fits |
| curve range, 32 echoed bytes | 76 | 114 | fits |
| curve type | 58 | 96 | fits |

The actual integer bounds are only `0..255` or `0..1`, so their real messages are shorter than the conservative figures.

STOP condition 4 is not triggered by capacity. It does, however, require the curve echo to be bounded by both the display limit and the actual data length, as discussed below.

---

## 8. Blocking finding 1 — plain `%g` defeats the stated near-miss requirement

The enriched scope mandates received-float `%g` and gives the reason that `100.0001` must not be rendered misleadingly as `100.0`.

A standalone C++20 `std::snprintf` probe against the exact value produced:

```text
value=100.0001
%g      -> 100
%.15g   -> 100.0001
%.17g   -> 100.0001
```

Plain `%g` uses the default precision of six significant digits. Therefore the scope-prescribed output would be:

```text
got 100, expected a finite float in the range 0.0..100.0 inclusive.
```

That is even more misleading than the prohibited `got 100.0` form: the shown value appears exactly valid.

A second probe used the smallest representable `double` above `100.0`:

```text
raw value = 100.00000000000001
%g        -> 100
%.15g     -> 100
%.17g     -> 100.00000000000001
```

This shows that `%.15g`, although already used for startup provenance at `src/vapoursynth-Cnr3.cpp:738`, is not sufficient to guarantee that every rejected value just above the bound remains visibly above it.

### Coder recommendation

Use the round-trip precision for `double`:

```text
%.*g with std::numeric_limits<double>::max_digits10
```

or the equivalent fixed precision for the supported toolchain:

```text
%.17g
```

This keeps the example `101.0 -> 101`, preserves `100.0001`, and also distinguishes the nearest representable value above `100.0`.

It still fits `detail[128]` comfortably.

### Designer decision requested — D1

Please amend/except TRAP 2 from plain `%g` to `max_digits10` general formatting (`%.17g` for `double`).

No patch should implement literal `%g` while the scope simultaneously requires the rejected near-miss to remain visible.

---

## 9. Blocking finding 2 — bare `%.32s` is not bounded by `value_size`

The curve parser retrieves both:

```text
value      at src/vapoursynth-Cnr3.cpp:681
value_size at src/vapoursynth-Cnr3.cpp:684
```

The parser’s existing literal matcher explicitly handles both logical representations:

```text
src/vapoursynth-Cnr3.cpp:650-652
actual_size == literal_length

src/vapoursynth-Cnr3.cpp:654-657
actual_size == literal_length + 1 and trailing NUL
```

Therefore the current parser does not define the data element solely as a conventional NUL-terminated C string. The error formatter should respect the returned logical size.

A precision on `%s` is only a maximum character count; it does not make a shorter non-NUL-terminated object safe. An exploratory AddressSanitizer probe with a four-byte non-NUL buffer and `"%.32s"` reported a stack-buffer-overflow read.

### Coder recommendation

Bound the echo by both limits:

```text
echo_length = clamp(value_size, 0, 32)
format with %.*s using echo_length
```

or first copy exactly that many bytes into a local NUL-terminated/sanitised echo buffer and format the buffer normally.

The second form is preferable if D3 below adopts one-line sanitisation.

### Designer decision requested — D2

Please amend the literal `%.32s` prescription to require an actual-size-aware bound, e.g. `%.*s` with `min(value_size, 32)`, or a local bounded echo builder based on `value_size`.

This remains message-only and does not affect curve validation.

---

## 10. Blocking finding 3 — raw user text cannot guarantee one-line output

The enriched scope requires:

```text
- messages remain one line;
- a long/odd curve string produces one clean line;
- user-supplied curve text is echoed.
```

Raw `%s` formatting does not satisfy all three for data containing a newline. A standalone probe with `bad\nvalue` produced:

```text
1: got "bad
2: value", expected exactly "wide" or "narrow".
```

This is not a buffer overflow, but it violates the required one-line diagnostic and permits user data to split or forge log lines.

### Coder recommendation

Build a local curve-echo buffer from at most 32 input bytes, respecting `value_size`, and replace ASCII control bytes with `?` one-for-one:

```text
bytes 0x00..0x1F and 0x7F -> '?'
all other bytes           -> copied unchanged
```

Advantages:

- fixed maximum output length of 32 bytes plus NUL;
- no CR/LF injection;
- no locale-dependent `isprint` behaviour;
- ordinary values retain the exact required output (`wobbly`, `o`, etc.);
- validation remains byte-exact and unchanged because sanitisation is diagnostic-only and occurs only after validation has rejected the value.

A more elaborate escaping scheme is possible, but it expands data and adds unnecessary policy for this rider. One-for-one replacement is the smallest deterministic solution.

### Designer decision requested — D3

Please approve control-byte replacement for the echoed diagnostic text, or explicitly narrow the “long/odd string” and “one clean line” proof requirement so CR/LF/control data is excluded. The coder recommendation is to retain the stronger one-line requirement and approve replacement.

---

## 11. Stop-condition disposition

| Enriched-scope stop condition | Disposition from confirm pass |
|---|---|
| 1. Invalid option currently accepted | **Not found in static source inspection.** Runtime gate remains required. |
| 2. Message fix requires validation change | **No.** All proposed work can remain message-only. |
| 3. Typed-getter failure not distinguishable from value failure | **No.** All relevant helpers already have separate branches. |
| 4. Safe curve echo cannot fit buffers | **No capacity problem.** Safe implementation requires size-aware bounding and a decision on control-byte handling. |
| 5. Source disagrees with VERIFIED source claims | **No line-map or behaviour claim mismatch found.** The findings are contradictions/gaps in the prescribed formatting method, not stale source facts. |

Because D1-D3 affect the exact implementation contract and proof expectations, patch production pauses pending designer/coordinator resolution.

---

## 12. Final patch shape — conditional on D1-D3 approval

### Exact changed files

```text
src/vapoursynth-Cnr3.cpp
src/cnr3_build_config.h
```

### `src/vapoursynth-Cnr3.cpp`

Message-construction changes only:

1. Add small bounded expectation-text builders for integer, float, and curve expectations.
2. Add an optional full-expectation override to the file-local integer parser so bool can retain delegation and use `expected 0 or 1.`.
3. Build each expectation once per present option and reuse it for both typed-getter and value/range errors.
4. Type errors become:

   ```text
   incorrect value type, <shared expectation>
   ```

5. Value errors become:

   ```text
   got <received value>, <shared expectation>
   ```

6. Integer echo uses `%lld` with the existing `std::int64_t` value.
7. Float echo uses the designer-approved corrected general precision from D1.
8. Curve echo uses the designer-approved size-aware and one-line-safe builder from D2/D3.
9. `cnr3_option_present_once` and its multiple-value message remain unchanged.
10. No parser predicate, accepted/rejected set, assignment, call-site bound, default, apply path, short-circuit order, response-config output, or pixel/cache behaviour changes.

### `src/cnr3_build_config.h`

Marker only:

```text
CMS07-FEATURE.cnr2-descriptive-option-parser
->
CMS07-RIDER.option-error-messages
```

The shipping mode/profile/retry-bias gates remain byte-identical.

### No other files

No README, tests, project files, VapourSynth headers, response-table files, scene-change files, cache files, or harness files belong in the diff.

---

## 13. Proof obligations after patch

The enriched scope’s proof gate remains appropriate, with these refinements:

1. Add `scene_threshold=100.0001` and preferably the nearest representable value above `100.0` to prove D1 rather than checking only `101.0`.
2. The curve long/odd test should include:
   - more than 200 bytes;
   - data whose logical size is not assumed to include a trailing NUL, where the harness can construct it;
   - CR/LF/control content if D3 approves sanitisation.
3. Type and range counterparts must compare their expectation tails exactly.
4. The canonical four-way count remains expected at config-dependent 57/57, forced-fail 56/57 exit 1; any difference must be located in code.
5. Debug and Release must use rebuild after the header marker change.
6. R-PROCESS-19 no-args BEFORE/AFTER frame output must remain byte-identical while the edit-version markers differ.
7. The runtime log must show the new marker and unchanged `response_config:` emission.

The `.vpy`/`.bat` option and byte-comparison harnesses remain the designer’s deliverable; VS2026 execution remains the coordinator’s responsibility.

---

## 14. Sandbox validation performed in this confirm pass

Performed:

```text
- extracted the actual uploaded src.zip;
- verified the baseline marker and ship configuration;
- verified the enriched scope copy against the FINAL_DOCS copy by SHA-256 and byte comparison;
- inspected every helper, call site, default, validation branch, and apply site cited above;
- enumerated all calls to the int parser and confirmed TRAP 1(b) is local and clean;
- calculated conservative detail/message lengths against the fixed buffers;
- compiled and ran standalone C++20 snprintf float probes;
- compiled and ran an AddressSanitizer non-NUL %.32s probe;
- compiled and ran a newline-injection probe.
```

Not performed, as expected at confirm-before-patch stage:

```text
- no source patch;
- no VapourSynth DLL compile;
- no VS2026 Debug/Release build;
- no canonical four-way runtime selftest;
- no invalid-option runtime harness;
- no BEFORE/AFTER pixel byte comparison.
```

No claim of those deferred validations is made.

---

## 15. Requested designer response

Please rule on:

```text
D1. Received float:
    Approve max_digits10 general formatting (`%.*g`, max_digits10 / equivalently `%.17g`)
    instead of literal `%g`.

D2. Curve data length:
    Approve value_size-aware bounding (`%.*s` or a bounded local echo buffer)
    instead of bare `%.32s`.

D3. One-line curve echo:
    Approve one-for-one replacement of ASCII control bytes with `?`, or provide
    another fixed sanitisation policy.
```

Coder recommendation: **approve D1, D2, and D3 as proposed**, retain TRAP 1 choice **(b)**, then authorise the two-file message-only patch described in section 12.

Until those decisions are issued, no patch should be produced.
