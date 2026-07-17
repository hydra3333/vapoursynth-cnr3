# CNR3 — PATCH SCOPE (ENRICHED, self-contained): self-explanatory option error messages — v3

**Marker on success:** `CMS07-RIDER.option-error-messages`
**Baseline:** committed `CMS07-FEATURE.cnr2-descriptive-option-parser` (W3X uploads the real `src.zip`).
**Supersedes:** CNR3_Rider_Scope_OptionErrorMessages_v2.md (technically identical; this version adds the
due-process wrapper, the verified code facts, and the traps a fresh chat cannot know).
**Nature:** MESSAGE TEXT ONLY. No validation logic changes. No pixels. Two files.

---

# PART 0 — READ THIS FIRST (why this document is shaped like this)

You are a fresh coder chat. Your predecessor approved this scope, then hit its context limit and degraded:
it emitted an inline "patch" in chat and immediately retracted it as "unvalidated draft content". **Nothing
from that exchange exists or is usable.** You are implementing from scratch, from this document.

This scope is deliberately over-specified. The reason is not distrust — it is that this project has learned,
expensively, that a memoryless chat cannot know the traps, and that the cost of re-discovering them is
measured in whole sessions. Everything below marked **VERIFIED** was checked cold against the real source by
the designer (W3D); you may rely on it, but you are welcome (encouraged) to re-verify and to CORRECT the
designer if the source disagrees. That has happened before and it is the process working.

---

# PART 1 — THE TASK IN ONE PARAGRAPH

The option parser rejects bad values correctly, but its error messages do not tell the user enough. Range
errors do not echo the value they rejected; type errors do not say what would have been acceptable. Fix the
TEXT so every rejection is self-explanatory in one line. Change nothing about WHICH values are rejected.

---

# PART 2 — CURRENT BEHAVIOUR (VERIFIED against committed source)

All option errors funnel through one helper (`vapoursynth-Cnr3.cpp` ~line 478):

```cpp
void cnr3_set_option_error(VSMap* out, const VSAPI* vsapi, const char* option_name, const char* detail) noexcept {
    char message[256]{};
    std::snprintf(message, sizeof(message), "CNR3: invalid %s option: %s",
        option_name != nullptr ? option_name : "<unknown>",
        detail  != nullptr ? detail  : "invalid value.");
    cnr3_set_create_error(out, vsapi, message);   // -> vsapi->mapSetError(out, message)
}
```

So every message reads: `CNR3: invalid <option_name> option: <detail>`. Only `<detail>` changes in this patch.

**The parse helpers (VERIFIED, with their current detail strings):**

| helper | ~line | type-error detail | range/value-error detail |
|---|---|---|---|
| `cnr3_option_present_once` | 495 | — | `"expected exactly one value."` (~513) |
| `cnr3_parse_optional_int_option` | 525 | `"expected an integer value."` (~548) | `"expected an integer in the range %d..%d inclusive."` (~554-561) |
| `cnr3_parse_optional_bool_option` | 569 | *(delegates — see TRAP 1)* | *(delegates — see TRAP 1)* |
| `cnr3_parse_optional_float_option` | 594 | `"expected a float value."` (~617) | `"expected a finite float in the range %.1f..%.1f inclusive."` (~623-630) |
| `cnr3_parse_optional_curve_option` | ~660 | `"expected string value \"wide\" or \"narrow\"."` (~687) | `"expected exactly \"wide\" or \"narrow\"."` (~701) |

**The eleven options and their bounds (VERIFIED — the call sites in `cnr3_parse_create_options` ARE the
specification; ranges are passed as arguments, not hardcoded in the helpers):**

```cpp
cnr3_parse_optional_int_option  (in, out, vsapi, "y_threshold",     0, 255, options.y_threshold) &&
cnr3_parse_optional_int_option  (in, out, vsapi, "y_strength",      0, 255, options.y_strength) &&
cnr3_parse_optional_int_option  (in, out, vsapi, "u_threshold",     0, 255, options.u_threshold) &&
cnr3_parse_optional_int_option  (in, out, vsapi, "u_strength",      0, 255, options.u_strength) &&
cnr3_parse_optional_int_option  (in, out, vsapi, "v_threshold",     0, 255, options.v_threshold) &&
cnr3_parse_optional_int_option  (in, out, vsapi, "v_strength",      0, 255, options.v_strength) &&
cnr3_parse_optional_curve_option(in, out, vsapi, "y_curve",                 options.y_curve) &&
cnr3_parse_optional_curve_option(in, out, vsapi, "u_curve",                 options.u_curve) &&
cnr3_parse_optional_curve_option(in, out, vsapi, "v_curve",                 options.v_curve) &&
cnr3_parse_optional_float_option(in, out, vsapi, "scene_threshold", 0.0, 100.0, options.scene_threshold) &&
cnr3_parse_optional_bool_option (in, out, vsapi, "scene_chroma",            options.scene_chroma);
```

It is a short-circuit `&&` chain: the FIRST bad option throws and the rest are not evaluated. That is correct
and must stay.

---

# PART 3 — REQUIRED BEHAVIOUR

## 3.1 The message family

```text
type error  : CNR3: invalid y_threshold option: incorrect value type, expected an integer in the range 0..255 inclusive.
range error : CNR3: invalid y_threshold option: got 256, expected an integer in the range 0..255 inclusive.
```

Same tail, differing head. **The expectation text ("expected an integer in the range 0..255 inclusive.") must
be built ONCE per option kind and reused by BOTH paths**, so the two messages can never drift apart. That
shared-helper requirement is the point of the patch, not a stylistic preference — today the type message and
the range message are independent string literals that could silently disagree.

## 3.2 Per-kind requirements

| kind | type error | range/value error |
|---|---|---|
| int | `incorrect value type, expected an integer in the range 0..255 inclusive.` | `got 256, expected an integer in the range 0..255 inclusive.` |
| float | `incorrect value type, expected a finite float in the range 0.0..100.0 inclusive.` | `got 101, expected a finite float in the range 0.0..100.0 inclusive.` |
| curve | `incorrect value type, expected exactly "wide" or "narrow".` | `got "wobbly", expected exactly "wide" or "narrow".` |
| bool | see TRAP 1 | see TRAP 1 |

Value formatting: int `%lld` (the getter returns `std::int64_t`); float **`%g` NOT `%.1f`** (see TRAP 2);
curve string quoted and width-limited (see TRAP 3).

---

# PART 4 — THE TRAPS (each is a real hazard found by cold inspection — do not skip)

## TRAP 1 — `scene_chroma` delegates to the int parser. DO NOT restructure it.
**VERIFIED:** `cnr3_parse_optional_bool_option` (~569) does NOT parse independently — it calls
`cnr3_parse_optional_int_option(..., 0, 1, integer_value)` and converts. Therefore `scene_chroma=2` currently
produces **"expected an integer in the range 0..1 inclusive."**, not "expected 0 or 1."

**W3D RULING — choose ONE, and say which in your confirm-report:**
- **(a) ACCEPTABLE (zero risk, preferred if (b) is not clean):** let bool keep delegating. Its messages become
  `got 2, expected an integer in the range 0..1 inclusive.` / `incorrect value type, expected an integer in
  the range 0..1 inclusive.` Honest and correct; slightly clinical.
- **(b) PREFERRED IF CLEAN:** add an OPTIONAL expectation-override parameter to the int helper (e.g. a
  `const char* expectation_override = nullptr`) so bool can pass `"0 or 1"` and get
  `got 2, expected 0 or 1.` — message-only, no validation change.
- **(c) FORBIDDEN:** rewriting `cnr3_parse_optional_bool_option` to parse independently. That touches
  validation structure for a cosmetic gain — R-PROCESS-25 (propose before touching proven code) and outside
  this rider's boundary.

## TRAP 2 — float must echo with `%g`, never `%.1f`.
The *expectation* text uses `%.1f` for the bounds (0.0..100.0) and that is fine. The *received value* must
use `%g`. Reason: `scene_threshold=100.0001` under `%.1f` renders as **"got 100.0, expected a finite float in
the range 0.0..100.0 inclusive."** — which reads as though the filter is broken. The near-miss is exactly when
the echo matters most.

## TRAP 3 — the curve echo puts USER-SUPPLIED TEXT into a fixed buffer.
`detail` is `char[128]`; `message` is `char[256]`. A 200-char `y_curve="..."` must not push the useful part of
the message out or overflow. **Required:** width-limit the echo (`%.32s`) and guard nullptr. Everything must
remain `snprintf`-bounded. Add a proof run with a long/odd string.

## TRAP 4 — type errors have NO value to echo. Do not invent one.
When `mapGetInt`/`mapGetFloat`/`mapGetData` fails, the value could not be retrieved *as that type*. There is
nothing truthful to print. Say `incorrect value type` + the expectation. **Never** print a garbage/zero/
default value as if it were what the user passed.

## TRAP 5 — `mode` is not a CNR3 option.
cnr2's `mode="oxx"` was retired in favour of three explicit `y_curve`/`u_curve`/`v_curve` params. If you see
`mode` anywhere in a message, that is a bug. cnr2's terse names (ln/lm/un/um/vn/vm/scdthr/sceneChroma) are
reference/equivalence documentation only.

## TRAP 6 — `y_threshold` does NOT filter luma. Do not "improve" its message to say it does.
The Y response is a **luma-change GUARD that gates chroma blending** via cross-plane weighting
(`weight_u = table_y[diff_y] * table_u[diff_u]`). Y output is never filtered. This is the least obvious
semantic in the filter; cnr2's own docs never stated it. Messages must not imply Y is denoised.

## TRAP 7 — `threshold == 0` is VALID. Do not tighten the range to 1..255.
cnr2 documents 0..255 and CNR3 keeps that range. The table builder already special-cases zero (centre entry
only, never divides by zero) — **VERIFIED** at `cnr3_response_tables.cpp:50-53`. A message change must not
become a range change.

## TRAP 8 — the short-circuit chain means only the FIRST bad option is reported.
That is correct and expected. Do not "improve" it into collecting all errors — that changes control flow.

---

# PART 5 — OUT OF SCOPE (do not touch; if you believe you must, STOP and report)

```text
which inputs are accepted or rejected     defaults              resolved config construction
response_config emission content          response tables       threshold-zero handling
scene_threshold math                      scene_chroma plumbing pixels / blend math
cache / recovery / PlanRetry behaviour    filter-mode behaviour README / user documentation
the short-circuit && chain                the 11 call-site ranges
```

**SHIP CONFIG — do not change as a side effect:** `fmParallelRequests` + `CNR3_CACHE_PROFILE_HALF` +
`CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**. Each is an evidence-backed decision.

---

# PART 6 — STOP CONDITIONS (report, do not proceed)

1. **Any invalid option turns out to be currently ACCEPTED.** That is a parser validation BUGFIX, not this
   rider. Stop immediately and report with evidence.
2. Fixing a message would require changing validation logic.
3. A typed-getter failure cannot be cleanly distinguished from a range/value failure.
4. Buffer-safe curve-string echoing cannot fit within the existing `detail[128]` / `message[256]`.
5. The real source disagrees with any **VERIFIED** claim in this document. (Say so plainly — the designer
   expects to be corrected by the confirm pass; it has caught designer errors before.)

---

# PART 7 — DUE PROCESS (how this project works; follow it exactly)

## 7.1 Confirm before patching
Do NOT deliver a patch first. Deliver a **confirm-report** that reconciles this scope against the REAL
uploaded source, with **file:line evidence**, stating:
- each helper's current detail strings and line numbers (correct my PART 2 table if it is wrong);
- your TRAP 1 choice ((a) or (b)) and why;
- confirmation that all four kinds' expectation text can be shared between type and range paths;
- confirmation the curve echo can be width-limited within the buffers;
- a FINAL PATCH SHAPE statement (files touched, what changes, what does not).
W3D reviews that, then you patch. This pass routinely catches what a scope missed — in both directions.

## 7.2 Cut the patch against the ACTUAL uploaded source
**Never reconstruct a baseline.** A reconstructed baseline that differed from the real tree by a few edited
lines caused every apply to fail and burned a long session (R-PROCESS-28). Use exactly the uploaded `src.zip`.
Verify its marker first: it must read `CMS07-FEATURE.cnr2-descriptive-option-parser`.

## 7.3 Your patch notes must match your patch
Last session's notes were wrong about their own patch three times (described the wrong content; listed a file
the patch did not contain; called a standalone patch a "delta"). The designer apply-tests everything, so
errors surface — but they cost rounds. **State the exact changed-file list and verify it against your own
diff before sending** (R-PROCESS-32: the patch is the arbiter, not its notes).

## 7.4 Include the apply block in the notes
```bat
git status --short
git apply --check --ignore-whitespace CMS07-RIDER.option-error-messages.patch
git apply --ignore-whitespace CMS07-RIDER.option-error-messages.patch
git diff --check
git status --short
```
Fallback (tolerates CRLF and small offsets): `patch -p1 --binary < CMS07-RIDER.option-error-messages.patch`
Notes: `git apply --3way` fails on untracked blobs. `git apply` is ALL-OR-NOTHING — "Applied cleanly" for
some files still means NOTHING applied if another failed; always check `git status`.
**NEVER `git stash`** (standing project rule): use `git switch -c wip-name` or `git checkout -- <files>`.

## 7.5 Do not chase the VapourSynth headers
They are absent from your sandbox by design. Validate what you can (apply-check, grep, `-fsyntax-only` where
headers permit) and explicitly defer the VS2026 Debug/Release builds and the runtime selftest to the
coordinator (W3X), saying so plainly. This is expected and correct — not a failure.

## 7.6 If you cannot produce a proper downloadable patch file, SAY SO IMMEDIATELY
Do not improvise an inline patch. That is the signal that the chat is at its limit and a fresh one is needed.
Your predecessor did exactly this and the work had to be discarded.

---

# PART 8 — EXPECTED PATCH SHAPE

```text
Files:
  src/vapoursynth-Cnr3.cpp    (the message text + shared expectation helpers)
  src/cnr3_build_config.h     (marker bump ONLY)

Work:
  1. Add a small expectation-text builder per option kind (int / float / curve / bool-per-TRAP-1).
  2. Type-error paths: "incorrect value type, expected <expectation>".
  3. Range/value-error paths: "got <value>, expected <expectation>".
  4. Value formatting: int %lld; float %g; curve quoted + %.32s + nullptr guard; bool per TRAP 1.
  5. All snprintf-bounded within detail[128] / message[256].
  6. Validation logic UNCHANGED.
  7. Marker -> CMS07-RIDER.option-error-messages
```

---

# PART 9 — PROOF GATE

This rider's runtime tests ALSO discharge two items deferred from the option-parser commit (its gate items 8
and 9). They are the same runs. **Do them here.**

```text
 1. Debug   x64 build PASS.
 2. Release x64 build PASS.  (use msbuild /t:Rebuild after any header/gate change — incremental builds
                              leave stale objects and silently mix configurations)
 3. Canonical 4-way selftest: 57/57 UNCHANGED (forced-fail 56/57 invariant_violation e1).
    NOTE: the count is CONFIG-DEPENDENT (56 base + diag3c2_induced_live_bail_plantrace, enabled by
    CNR3_DIAG_COMPUTE_DSUM_PLANTRACE). If it differs, LOCATE THE CAUSE IN CODE — never hand-wave.
 4. RANGE/VALUE errors — each must fail the run cleanly (clear one-line message, non-zero exit, no frames):
      y_threshold=256        -> got 256, expected an integer in the range 0..255 inclusive.
      u_strength=-1          -> got -1, ...
      scene_threshold=101.0  -> got 101, expected a finite float in the range 0.0..100.0 inclusive.
      y_curve="wobbly"       -> got "wobbly", expected exactly "wide" or "narrow".
      u_curve="o"            -> MUST be REJECTED (cnr2's old spelling is deliberately not accepted)
      scene_chroma=2         -> per TRAP 1 choice
 5. TYPE errors — each must fail cleanly with "incorrect value type" + the SAME expectation text as its
    range counterpart (this is the proof the shared helper is real):
      y_threshold="bad"      scene_threshold="bad"      y_curve=123      scene_chroma="bad"
 6. Long/odd curve string (e.g. 200 chars) truncates safely: one clean line, no overflow, no crash.
 7. threshold==0 MUST SUCCEED (it is valid):
      y_threshold=0  -> run completes; response_config shows y=0/192/wide
      u_threshold=0  -> run completes
    (This discharges the parser's deferred gate item 9.)
 8. Valid spot-check still succeeds (no-options run completes normally).
 9. R-PROCESS-19 ANCHOR: no-args output byte-identical to the committed parser build.
    Message text cannot affect pixels; this proves nothing else moved. Use the BEFORE/AFTER fc harness;
    verify the two edit_version= markers DIFFER (proving you compared the right builds) while frames MATCH.
10. Marker visible: a run's log shows edit_version=CMS07-RIDER.option-error-messages:<mode> alongside the
    response_config: line. (R-PROCESS-30 — verify the marker actually landed.)
```

**If ANY invalid value is ACCEPTED at step 4 or 5 -> STOP. Parser bugfix, not this rider.**

---

# PART 10 — WHAT "DONE" LOOKS LIKE

- A downloadable `.patch` file, cut against the uploaded committed source, applying cleanly.
- Notes whose changed-file list matches the diff exactly, containing the apply block and an honest statement
  of what was and was not validated in your sandbox.
- Every rejection message names the option, says what was received (or that the type was wrong), and states
  what would be acceptable — in one line, inside the buffers.
- Nothing else in the diff. The reviewer will scan every removed line; anything beyond message text is a
  finding.
