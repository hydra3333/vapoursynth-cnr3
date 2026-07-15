# CNR3 — RIDER SCOPE: self-explanatory option error messages — v2

**Rides on:** CMS07-DOC.cnr2-descriptive-options-readme (or its own small patch if W3X prefers).
**Baseline:** committed CMS07-FEATURE.cnr2-descriptive-option-parser.
**Rationale (W3X):** users want to see what they did wrong immediately, without cross-referencing their own
script. Especially valuable when the value is computed rather than literal, and for type errors where the
current message does not say what was actually received.

## Current behaviour

`cnr3_set_option_error()` formats: `CNR3: invalid <option_name> option: <detail>`
Detail strings today state the expectation but NOT the offending value. Example:
```
CNR3: invalid y_threshold option: expected an integer in the range 0..255 inclusive.
```

## Required behaviour

Include the received value. Target form:
```
CNR3: invalid y_threshold option: got 256, expected an integer in the range 0..255 inclusive.
CNR3: invalid scene_threshold option: got 101.0, expected a finite float in the range 0.0..100.0 inclusive.
CNR3: invalid y_curve option: got "wobbly", expected exactly "wide" or "narrow".
CNR3: invalid scene_chroma option: got 2, expected 0 or 1.
```

## Sites (four; all already build `detail` via snprintf and already hold the value)

1. **Int range error** (~line 552): add the value to the existing snprintf.
   `"got %lld, expected an integer in the range %d..%d inclusive."` with the int64 value.
2. **Float range error** (~line 621): likewise.
   `"got %g, expected a finite float in the range %.1f..%.1f inclusive."`
   Use %g (not %.1f) for the received value so the user sees what they actually passed.
3. **Curve string error** (~line 687/701): echo the received string, quoted.
   `"got \"%s\", expected exactly \"wide\" or \"narrow\"."`
   Guard: the received string may be long or contain odd bytes — truncate to a sane width
   (e.g. %.32s) so it cannot overflow `detail[128]`, and handle nullptr.
4. **Bool/int 0..1 error**: echo the value, `"got %lld, expected 0 or 1."`

## Type-error sites (W3X ruling — improved approach)

When the typed get fails (e.g. user passed a string to an int option) there is NO typed value to echo.
Do NOT invent one. Instead report the type problem AND the same valid range/allowed values that the range
error reports:
```
CNR3: invalid y_threshold option: incorrect value type, expected an integer in the range 0..255 inclusive.
CNR3: invalid scene_threshold option: incorrect value type, expected a finite float in the range 0.0..100.0 inclusive.
CNR3: invalid y_curve option: incorrect value type, expected exactly "wide" or "narrow".
CNR3: invalid scene_chroma option: incorrect value type, expected 0 or 1.
```
This tells the user what went wrong AND what to type, from one line, without echoing anything unavailable.

**Implementation note:** the expectation text ("expected an integer in the range %d..%d inclusive." etc.)
is now used by BOTH the type-error and the range-error paths. Build it ONCE per option kind in a small
helper and reuse it at both sites, so the two messages cannot drift apart. The resulting message family:
```
type error  : incorrect value type, expected <expectation>
range error : got <value>, expected <expectation>
```
Same tail, differing head.

## Constraints

- `detail[128]` and `message[256]` are fixed buffers: all additions must remain snprintf-bounded and
  truncation-safe. Verify no format/precision combination can overflow; keep string echoes width-limited.
- No change to WHICH inputs are rejected — this is message text only. Validation logic untouched.
- Messages remain one line, specific to the offending parameter (do NOT list other params' ranges).

## Proof

1. Debug+Release build; canonical 4-way selftest count unchanged.
2. Invalid-option runs, RANGE errors: received value echoed, in each of the four classes
   (int range, float range, curve string, bool).
3. Invalid-option runs, TYPE errors: message reads "incorrect value type, expected <expectation>" and
   carries the SAME expectation text as the corresponding range error (proving the shared helper).
4. Long/odd curve string (e.g. a 200-char value) truncates safely, no overflow, still one clean line.
5. R-PROCESS-19: no-args output byte-identical to the committed parser build (message text cannot affect
   pixels; this is a cheap confirmation that nothing else moved).
6. No change to the set of accepted/rejected inputs (spot-check a valid run still succeeds).
