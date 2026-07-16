# CNR3 — DESIGNER RULINGS on the RIDER confirm-before-patch report — D1/D2/D3

**Scope:** CNR3_Patch_Scope_OptionErrorMessages_ENRICHED_v3.md
**Report:** CNR3_RIDER_option_error_messages_Coder_Confirm_Before_Patch_Report_v1.md
**Verdict:** **CONFIRM-REPORT ACCEPTED. All three findings UPHELD — each is a defect in the DESIGNER's
scope, not in the source.** Proceed to patch under the amended prescriptions below.

W3D note for the record: the coder was right to refuse to implement the scope as written, and right to
produce probe evidence rather than argument. Two of the three findings (D1, D2) would have shipped a defect
if the scope had been followed literally; D2 would have shipped an out-of-bounds read on user-controlled
data. This is confirm-before-patch working exactly as intended.

---

## D1 — float echo: TRAP 2's `%g` prescription is WITHDRAWN

**Finding UPHELD.** W3D re-verified independently:
```
value 100.0001 ->   %g = 100        %.1f = 100.0     %.10g = 100.0001    %.17g = 100.0001
```
`%g` defaults to 6 significant digits; `100.0001` needs 7. The scope's own stated rationale (the rejected
near-miss must not render as if it were the bound) is DEFEATED by the prescription: it would have produced
`got 100, expected a finite float in the range 0.0..100.0 inclusive.` — worse than the `%.1f` it replaced.
The designer's reasoning was correct and the prescription contradicted it.

**RULING — use shortest round-trip formatting, NOT a fixed precision.**

Preferred (**D1-a**): `std::to_chars(buf, buf_end, value)` (`<charconv>`, no format/precision argument =
shortest round-trip), then embed the resulting NUL-terminated buffer with `%s`.

W3D comparison across the cases that matter:
```
value                 to_chars              %.17g                      %.10g
100.0001              100.0001              100.0001                   100.0001
-0.1                  -0.1                  -0.10000000000000001       -0.1
101.0                 101                   101                        101
100.00000000000001    100.00000000000001    100.00000000000001         100        <- hides the near-miss
1e300                 1e+300                1.0000000000000001e+300    1e+300
```
`to_chars` is the only option that is BOTH correct on the pathological near-miss AND clean on the common
case. The coder's `%.17g` recommendation is correct-but-ugly (`-0.1` -> `-0.10000000000000001` for a value a
user plainly typed); `%.10g` is clean but hides sub-1e-13 near-misses. Shortest round-trip has neither flaw:
by definition it prints the fewest digits that uniquely identify the double, so a value distinct from the
bound ALWAYS prints distinctly from the bound.

Accepted fallback (**D1-b**): if `std::to_chars` for `double` is unavailable or awkward on the toolchain,
use `%.17g` (the coder's recommendation) — correct, merely verbose. Report which you used and why.

**Guards:** handle non-finite explicitly (NaN/Inf are rejected by `cnr3_scdthr_is_valid`; ensure the echo
prints something sane like `nan`/`inf` rather than tripping the formatter). `to_chars` will not overflow a
64-byte local; still check `ec == std::errc{}` and fall back to a fixed literal on failure.

---

## D2 — curve echo: TRAP 3's `%.32s` prescription is WITHDRAWN (this one was a real bug)

**Finding UPHELD, and this is the most important finding in the report.** The scope said "width-limit
(`%.32s`) and guard nullptr". That is INSUFFICIENT and unsafe:

- `mapGetData` returns a pointer AND a `value_size`; the data element is not contractually NUL-terminated.
- The parser's OWN literal matcher proves this — it explicitly handles both `actual_size == literal_length`
  and `actual_size == literal_length + 1 with trailing NUL` (`vapoursynth-Cnr3.cpp:650-657`). The source
  itself documents that both representations occur.
- A precision on `%s` is a MAXIMUM character count, not a bound: `printf` still scans for NUL and will read
  past a shorter non-NUL-terminated object. The coder's AddressSanitizer probe (4-byte non-NUL buffer +
  `"%.32s"`) reported **stack-buffer-overflow read**. That is a genuine out-of-bounds read on
  USER-CONTROLLED data.

**RULING — the echo MUST be bounded by the API-returned `value_size`, not by a `%s` precision.**

Required: `echo_length = min(value_size_clamped_at_zero, 32)` and format with `%.*s` passing
`echo_length` — or (PREFERRED, and required if D3 is implemented as sanitisation) copy exactly
`echo_length` bytes into a local NUL-terminated echo buffer, sanitise it, and format that buffer with `%s`.

Also retain the nullptr guard (data may be null with size 0).

**This remains message-only** — it does not touch curve validation, which already compares against
`value_size` correctly.

---

## D3 — one-line guarantee vs raw echoed text: ACCEPTED, sanitise

**Finding UPHELD.** The scope required, simultaneously, that messages be one line AND that user text be
echoed, without reconciling the two. `y_curve="bad\nvalue"` splits the error across lines and breaks any
log-line-oriented gate or grep.

**RULING — sanitise the echoed bytes before formatting.** In the local echo buffer (D2's preferred form),
replace every byte that is not printable ASCII (i.e. outside `0x20..0x7E`) with `?`. This preserves the
"tell the user what they typed" intent (they still see the shape and length of their value), guarantees one
line, and cannot introduce control sequences into the log.

Truncation marker: if `value_size > 32`, indicate truncation so the user is not misled into thinking their
long string was received short. Append `...` INSIDE the quotes, e.g. `got "aaaaaaaa...(truncated)"` or
simply `got "aaaa..."`. Coder's choice of exact form; state it in the notes. Keep it inside `detail[128]`.

Non-ASCII note: a UTF-8 curve value will render as `?` per byte. That is acceptable and correct here — the
only VALID values are the ASCII literals `wide`/`narrow`, so any non-ASCII input is by definition wrong, and
showing its shape is enough for the user to see their mistake.

---

## TRAP 1 — bool expectation override: choice (b) CONFIRMED

The coder chose **(b)** (optional expectation-override parameter on the int helper so `scene_chroma` can
report `expected 0 or 1.`) and reports it as clean. **APPROVED.** It is message-only, it leaves the
delegation intact (so validation structure is untouched, per TRAP 1(c)), and it produces the better text.

If during implementation (b) turns out to require any change to the int helper's VALIDATION behaviour (as
opposed to its message text), fall back to (a) — accept the delegated
`expected an integer in the range 0..1 inclusive.` — and say so. Do NOT rewrite the bool parser.

---

## What is UNCHANGED from ENRICHED v3

Everything else stands, including: the message family (`incorrect value type, expected <expectation>` /
`got <value>, expected <expectation>`); the shared expectation text built ONCE per option kind and reused by
BOTH paths (the anti-drift requirement, which is the point of the patch); int `%lld`; validation logic
untouched; the short-circuit chain untouched; `detail[128]`/`message[256]` snprintf-bounded; two files
(`vapoursynth-Cnr3.cpp` + `cnr3_build_config.h` marker only); the ship config untouched; and the full
PART 9 proof gate — including the R-PROCESS-19 no-args byte-identical anchor and the discharge of the
parser's deferred gate items 8 and 9.

**Additional proof items arising from these rulings (add to the gate):**
```
 6a. Float near-miss echo: scene_threshold=100.0001 -> the message must show 100.0001 (NOT 100, NOT 100.0).
 6b. Float common case:    scene_threshold=-0.1     -> the message should show -0.1 (if D1-a) or
                                                       -0.10000000000000001 (if D1-b fallback); either is
                                                       acceptable, but state which path was taken.
 6c. Non-NUL-terminated curve data: exercise a data element supplied WITHOUT a trailing NUL and confirm no
     ASan/overrun and a correct echo. (If the harness cannot easily construct one, state that and rely on
     the value_size-bounded code path plus code review.)
 6d. Control-character curve value: y_curve="bad\nvalue" -> ONE line, control char rendered as ?.
 6e. Long curve value (200 chars) -> ONE line, truncated with the chosen marker, no overflow.
```

---

## Process note (W3D, for the record)

Three defects, all in the designer's scope, all found by the coder testing instead of complying:
- D1 required disbelieving the scope's own stated rationale and checking the claim numerically.
- D2 required reading the VapourSynth API contract AND noticing the parser's own matcher proves the
  non-NUL case is real — then proving the hazard with a sanitiser rather than asserting it.
- D3 required noticing two scope requirements that silently contradict each other.

This is R-PROCESS-25/31/32 territory working in the direction that matters least often and counts most: the
scope was wrong and the coder said so, with evidence, before writing a line. Recorded as a precedent.
