# CNR3 — PATCH SCOPE (DELTA): DIAG.3c.1 EMISSION-FORMAT REFINEMENT (writer/dump layer only)

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Controlling input:** `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v2_3.md` (read the v2.3 revision
note + §4 Set 4, §5, §6, §8, §9). Where this scope and the spec disagree, the spec wins and you flag it.
**Status:** PROPOSAL for coder investigate/confirm (usual process — confirm against real source BEFORE patching).
**Nature:** a REQUIREMENTS REFINEMENT of the emission/writer layer, specified after inspecting real 3c.1
output and given the A1 external-analysis commitment. NOT a defect in the prior scope or your patch — both
built faithfully to spec v2.2 (which specified the multi-line labelled-list emission and three views).
Accountability sits with the spec (the single-line-sortability intent was never hardened to "one physical
line" through v1->v2->v2.2). This SHRINKS the patch.

## 0. Apply-on-top, not a re-issue
This is a SEPARATE follow-on patch applied ON TOP of the already-applied 3c.1 patch (which is applied but NOT
yet committed). It is not a rebuild and not a re-issue. We commit 3c.1 ONCE, in this revised form (no 3c.1b,
no separate commit of the first patch). It shrinks the tree (less code, fewer gates).

## 1. What changes (writer/dump + gates + selftest fixture ONLY)

**R1 — single canonical line per record (supersedes v2.2 §5 multi-line).** Each buffered record renders as ONE
physical ASCII line with every field present (fixed schema; empty = `[]`, never omitted). Host prefix
`CNR3[n] INFO DSUM-PLANTRACE:` stays; then `[DSUM-PLANTRACE]`, then phase `O`/`R`, then the fields in fixed
order. Field order (spec §5):
```text
O: enter_tick seq frame exit_tick strategy enter_utc exit_utc run_ms target predecessor anchor floor holes sources pinned codes
R: enter_tick seq frame exit_tick outcome  enter_utc exit_utc run_ms computed adopted_skipped post_compute_discarded codes
```
**R2 — padding (supersedes D3C1-E width application):** zero-pad ONLY `enter_tick, exit_tick, seq, frame`
(existing widths; ticks fixed-20). Do NOT pad numbers inside `codes=[...]` or the role lists — parsed, not
sorted. `*` grid glyph rides on frame numbers inside role-lists/codes only, never on a padded key.
**R3 — one block, no views (supersedes v2.2 §8 / D3C1-F view machinery).** The dump is a SINGLE block, records
walked from the buffer AS-BUILT (action_seq order — already physical; NO sort pass). REMOVE the three views
(VIEW_DATETIME / VIEW_FRAME / VIEW_PHASE) and their three sub-#ifdef gates from `build_config.h`.
**R4 — legend once.** Print the legend ONCE at the head of the single block (content per spec §8 R4, ASCII).
COLUMN-ALIGNED with ASCII spaces (no tabs), name column padded to the widest name (`post_compute_discarded`,
22); uses `O-item`/`R-item` labels (NOT "field"). Alignment is LEGEND-ONLY — record lines stay unaligned
`key=value`.
**R5 — BEGIN/END per instance.** Bracket each instance's trace with:
```text
[DSUM-PLANTRACE] BEGIN schema=3c1v1 instance=<n> window=[<from>,<to>] records=<count>
   ... legend (once) ... records (natural order) ...
[DSUM-PLANTRACE] END   schema=3c1v1 instance=<n> records=<count> truncated=<0|1>
```
One block per instance (no cross-instance merge). `records=` on BEGIN (expected) vs END (actual) detects a
cut-off dump; `truncated=1` iff the buffer `reserve_failed` tripwire fired. Order: BEGIN -> legend -> records
-> END. Retain per-line flush (R-PROCESS-24).

**Vocabulary (display layer only — source enums unchanged):**
- Set 4 `L` display label -> `post_compute_discarded` (source enum `adopted_post_compute_loser` unchanged).
- Set 4 `U` / `unpinned=` REMOVED from the R record entirely — remove the `unpinned_frames` list from the R
  emission AND its population at the capture sites (the `unpinned_frames.push_back(...)` calls). Removing a
  capture-side write is permitted here ONLY because it is deleting a now-unused diagnostic field; it changes
  no cache/pin behaviour (those pushes only fed the diagnostic list). Confirm that in your report.

## 2. FENCE — do NOT touch (all proven in the approved patch; must stay byte-identical)
```text
- The capture MECHANISM: the arInitial RAII O-guard; the four arAllFramesReady branch-local R captures;
  the top-level enter_tick sampling + the frameData enter_tick field.
- The enter_tick-OUTSIDE-lock / action_seq-INSIDE-lock invariant (spec v2.2 §8).
- The windowed buffer, the window bound test, the reserve()/reserve_failed logic.
- Observe-only fencing: NO cnr3_set_filter_error touch, NO dump-on-bail, NO X/E, NO Set 5. (All 3c.2.)
- Every O/R field's CONTENT and its branch-specific derivation (sources=[n]/vector; pinned branch-derived).
The ONLY capture-path edit permitted is deleting the now-dead unpinned_frames writes (see 1, vocabulary).
```

## 3. What you must confirm (before patching)
1. The emission/dump function is the single place rendering records; confirm the field-order rewrite there
   produces the R1 lines with the fixed schema and R2 padding, and that removing the 3 view passes leaves one
   natural-order walk.
2. The three VIEW_* sub-gates in build_config.h are removed and nothing else references them (no stale
   `#if defined(...VIEW_...)`).
3. The `unpinned_frames` field, its emission, AND its capture-side pushes are all removed, and those pushes
   fed ONLY the diagnostic list (deleting them changes no pin/cache behaviour). Cite the sites.
4. `L` label -> `post_compute_discarded` everywhere it renders (record + legend); source enum untouched.
5. The selftest reference fixture emits the new single-line format, the aligned legend once, and a
   BEGIN/END-bracketed block (update it to match).
6. BEGIN/END are emitted per instance with matching `records=` counts and `truncated` wired to the
   `reserve_failed` tripwire; `schema=3c1v1` present on both.
7. Nothing in the fenced set (§2) is touched — a targeted diff proves the capture path is byte-identical
   except the unpinned_frames deletions.
8. Anything this scope got wrong or missed — call it out (highest-value part of your report).

## 4. Proof gate for the revision (re-review ONLY the delta)
```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; records now single-line; ONE block; legend once.
2. R-PROCESS-19 macro-off byte-identical: master gate OFF => plantrace compiles out, four-way IDENTICAL,
   .vpy byte-identical on/off. (Render-only change, but re-prove it — the A/B fc /b on S1/S7/S8.)
3. S1 eyeball: one clean natural-order block bracketed by BEGIN/END (matching `records=` counts,
   `truncated=0`), legend once at head (column-aligned, `O-item`/`R-item`), single-line records, fixed schema
   (empty=[]), padding on enter_tick/exit_tick/seq/frame only, no unpinned=, L shown as post_compute_discarded.
4. S8 spot-check: the single-line records `sort` cleanly by the padded keys (enter_tick/seq/frame/exit_tick).
5. Prior DSUM01-14 and the 3c.1 capture path unchanged (targeted diff).
Then commit 3c.1 ONCE (revised form). Marker (paste-ready, pre-commit working tree):
  CMS07-DIAG.3c.1-plantrace-clean-end-capture   (unchanged string; still the 3c.1 marker)
```

## 5. Out of scope (unchanged)
Dump-on-bail + Set 4 X/E + Set 5 failure-reason across the 65 bail sites remain DIAG.3c.2.
