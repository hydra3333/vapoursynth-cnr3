# CNR3 — COORDINATOR DIRECTIVE to DESIGNER (ND): DIAG.3c.1 emission-format REFINEMENT → fold into the patch before commit, then re-review only the delta

**Nature:** a REQUIREMENTS REFINEMENT of the plan-trace EMISSION/FORMAT layer, specified after inspecting
real 3c.1 output. NOT a defect in your scope or the coder's patch — both built faithfully to spec v2.2,
which specified the multi-line labelled-list emission. The refinement supersedes that emission design.
Accountability sits with the SPEC (shared — the single-line-sortability intent from the rationale doc was
never hardened into a "one physical line per record" requirement across spec v1→v2→v2.2); it is not an ND
or coder failure. Frame it to the coder that way.

**Scope of change:** the WRITER / DUMP layer ONLY. Everything proven in the approved patch is RETAINED and
must NOT be touched: the capture machinery, the RAII O-guard, branch-local R capture, observe-only fencing
(the byte-identical proof), the enter_tick-outside-lock / action_seq-inside-lock invariant, the windowed
buffer, and the per-record field capture. This refinement changes only HOW a buffered record is RENDERED to
stderr at dump time, and REMOVES the three-view machinery. It SHRINKS the patch.

---

## R1 — Canonical record = ONE physical line (supersedes spec v2.2 §5 labelled-list emission)

Each record emits as a SINGLE physical line carrying every field; the indented per-role human lines
(target=/predecessor=/anchor=/…/codes= on separate lines) are REMOVED. Target form:

```
[DSUM-PLANTRACE] O seq=00000007 tick=00000197777409659100 frame=00000007 strategy=PRED_PRESENT enter_utc=2026-07-06T07:47:06.786Z run_ms=0.003 target=[7] predecessor=[6] anchor=[] floor=[] holes=[] sources=[7] pinned=[6] codes=[6=PN,7=TS]
[DSUM-PLANTRACE] R seq=00000240 tick=00000197777714244700 frame=00000117 outcome=RETURNED_CACHE_HIT enter_utc=2026-07-06T07:47:07.091Z run_ms=0.007 computed=[] adopted_skipped=[] post_compute_loser=[] unpinned=[117] codes=[117=U]
```

Rules:
- ONE line per record. No indented sub-lines. The `CNR3[n] INFO DSUM-PLANTRACE:` host prefix stays (one per
  line, unavoidable) — the parser strips it; keep the `[DSUM-PLANTRACE]` block tag right after it.
- FIXED SCHEMA — EVERY field present on EVERY record of that phase, even when empty (`anchor=[] floor=[]
  holes=[]`). Do NOT omit empty fields. Rationale (bind inline): a fixed-column schema is what makes the
  external A1 Python parser and plain `sort`/`awk` trivial; variable-presence fields force conditional
  parsing. Empty = `[]`.
- Field ORDER is fixed and identical across all records of a phase (O-phase order and R-phase order each
  fixed), so column-oriented tools can rely on position as well as key=value.

## R2 — Padding: sort-keys padded, parse-fields UNpadded (refines spec v2.2 §6 / D3C1-E width rules)

- ZERO-PAD only the fields that are EXTERNALLY SORTED, so lexical order == numeric order:
  `seq`, `tick`, `frame`. Keep their existing widths.
- Do NOT pad numbers inside `codes=[...]` or inside the role lists (`target/predecessor/anchor/floor/
  holes/sources/pinned/computed/adopted_skipped/post_compute_loser/unpinned`). Those are PARSED, not
  sorted, so padding is pure noise: `codes=[6=PN,7=TS]`, `holes=[33,34,35]`, NOT `[00000033,...]`.
  Rationale (bind inline): padding earns its cost only where lexical-must-equal-numeric ordering matters;
  everywhere else it inflates line length and hurts readability for zero gain.
- The `*` checkpoint-grid glyph still rides on the frame number wherever a role list or codes entry names a
  grid frame (e.g. `40*`), unpadded.

## R3 — Collapse the THREE views to ONE emission (supersedes spec v2.2 §8 / scope §8 / D3C1-F)

- REMOVE VIEW_DATETIME, VIEW_FRAME, VIEW_PHASE entirely — the three sorted views and their three sub-#ifdef
  gates go away.
- The dump becomes ONE block: the records in NATURAL CAPTURE ORDER (i.e. action_seq ascending — walk the
  buffer as-built; NO sort pass at all), preceded by the legend printed ONCE.
- Rationale (bind inline): the three in-plugin views existed to give a human pre-sorted perspectives without
  tooling. Two later commitments dissolve that: (i) single-line sortable records (R1/R2) make any view one
  `sort -k` away; (ii) the A1 Python analysis track will parse the records and produce arbitrary views far
  beyond three fixed sorts. Three hard-coded in-plugin sorts are now redundant with — and strictly weaker
  than — external `sort`/A1. Emitting the raw single block in capture order is the cleanest architecture,
  the least code, and the easiest observe-only proof (one pass, one legend, zero sort passes). No
  information is lost: every record carries seq/tick/frame, so temporal/frame/phase orderings are external
  re-sorts.
- The block start/end terminators and the CLEAN-END DUMP END marker are RETAINED.

## R4 — Legend: MANDATORY, printed ONCE at the top of the single block (retained from spec v2.2 §8)

The legend is the parse key for the codes and is non-negotiable. It moves from three-times (once per view)
to ONCE (at the head of the single block). Content unchanged: phase, strategy, outcome, O-item codes,
R-item codes, and the `*`=checkpoint-grid glyph. It must sit ABOVE the records so a reader/tool meets the
key before the coded data.

## What is explicitly OUT of scope for this refinement
- No change to capture, timing, the tick/seq invariant, the window, the observe-only fencing, or any O/R
  field's CONTENT. Only rendering + view-collapse.
- No new human-readable expanded view is retained (the single line under the legend IS the human-decodable
  form; consistency across the one block keeps A1 simple).
- Dump-on-bail + E/X + failure-reason writes remain DIAG.3c.2 (unchanged; still deferred).

## Process
1. Amend spec v2.2 → v2.3: rewrite §5 (emission = single canonical line, fixed schema), §6/§8 padding rule
   (R2), §8 dump (R3 one natural-order block) + legend-once (R4). Bind each change's rationale inline per
   the established convention. Record in the change log that this supersedes the multi-line emission and the
   three-view design, and WHY (real-output inspection + the A1 external-analysis commitment).
2. Revise the DIAG.3c.1 patch: emission/dump function only. Remove the two now-dead view passes and the
   three view sub-gates from build_config. Keep everything else byte-identical.
3. RE-REVIEW ONLY THE DELTA: confirm (a) observe-only still holds (the change is render-only; the macro-off
   build must still be byte-identical and the four-way still 56/56); (b) single-line records parse as a
   fixed-schema table; (c) padding is on seq/tick/frame only; (d) one natural-order block + one legend;
   (e) nothing in the proven capture path changed (a targeted diff proves this). The bulk of the prior
   approval (capture, fencing, invariant) stands unamended.
4. Proof gate for the revision: four-way all-on 56/56 (records now single-line; legend once); macro-off
   byte-identical; a quick S1 eyeball that one clean sortable block emits with the legend; then S8 spot-
   check that the single-line records `sort` cleanly by tick/seq/frame. Then commit 3c.1 ONCE (the revised
   form) — no separate 3c.1b needed.

## Note on reuse
This is a writer-layer revision that SHRINKS the patch (less code, fewer gates). Do not let it be scoped as
a rebuild. The proven capture is reused wholesale; only the rendering and the view-collapse change. Expect
the re-review to be small and confined to the emission function + the build_config gate removal.
