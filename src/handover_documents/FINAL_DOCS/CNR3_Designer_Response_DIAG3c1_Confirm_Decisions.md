# CNR3 — DESIGNER RESPONSE: DIAG.3c.1 confirm-before-patch report — DECISIONS

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** `CNR3_DIAG3c1_confirm_before_patch_report.md` against spec v2.2 + scope v2 + post-DIAG.3b source.
**Verdict:** confirm report ACCEPTED. Proceed to patch after the decisions below. The patch returns for diff
review before commit (R-PROCESS-25 applies — see conditions).

---

## 0. What I verified COLD (not accepted from the report)

Re-derived against the committed post-DIAG.3b source, because the report's central recommendation turns on it:

- **The R-side pivot is CORRECT.** All three relevant branch helpers call `cnr3_discard_frame_data_with_cache()`
  BEFORE the success return: cache_hit discard @1020 -> return @1057; recovery discard @2052 -> return @2086;
  frame0 discard @2286 -> return @2309. So `request_data` is gone by the success return, and R facts MUST be
  copied before the discard. The hybrid (O = top-level RAII, R = branch-local explicit) is justified by source,
  not preference. This is the scope's §4.1 getting corrected by the confirm pass — §4.1 treated O and R
  symmetrically; they are not symmetric. Good catch.
- **The `sources` derivation is better-grounded than stated.** The existing D-SUM-06 production helper
  `cnr3_live_source_frame_was_requested_in_activation` ALREADY implements the exact rule: if
  `source_request_frame_numbers.empty()`, the requested source is `requested_frame` ([n]); else search the
  vector. So `sources = [n]` for cache_hit/frame0/pred_present and `= the vector` for recovery is not a new
  invention — it mirrors live production semantics. Confirmed.
- **arInitial bail-safety for the O-RAII guard.** arInitial returns `const VSFrame*`; the four success routes
  are the publish-helper tail-returns (671/692/718/738). The bail paths call
  `cnr3_delete_unpublished_frame_data(...)` then `cnr3_set_filter_error` + `return nullptr`, leaving
  `*frame_data` NULL (never published). So the guard's arming condition (frame_data published, branch != none,
  requested_frame == n, source_requested == true) cleanly distinguishes success from bail and cannot read
  freed/unpublished data. Safe.
- **Clean-end dump site.** `cnr3_free_filter` (vapoursynth-Cnr3.cpp) emits the D-SUM summaries before
  `freeNode`/`delete data`. Placement after D-SUM-14 is sound.
- **Fence held.** The report proposes no touch to cache_core, project files, the private pin-list surface, or
  any `cnr3_set_filter_error` bail site. R capture sits on the SUCCESS path only (after successful discard,
  before the success return); the discard-failure bails (1044/2064) stay untouched (3c.2).

---

## 1. DECISIONS (D3C1-A .. F)

**D3C1-A — APPROVED.** Patch 3c.1 standalone; 3c.2 is the next, separate cycle. (The commit-vs-combine choice
at the 3c.1 boundary remains the coordinator's, as already flagged — this decision only approves building 3c.1.)

**D3C1-B — APPROVED, with R-PROCESS-25 condition.** Hybrid capture:
- O = top-level scope-exit (RAII) guard in `cnr3_arInitial`. Declare it at TRUE top-level entry so `enter_tick`
  is real function-entry time (not post-validation), armed only on the published-success condition above.
- R = explicit branch-local capture on each of the four successful branch-return paths.
- CONDITION: the R branch-local captures touch proven getFrame success paths. Under R-PROCESS-25 the patch must
  present the EXACT per-branch insertions for review; do not fold them in as "obviously additive." I will run
  the whole-patch deletion scan and verify INVOCATION at all four branches (the D-1 reflex: a capture helper
  defined but not called at, say, the frame0 branch would silently drop frame0's R records — trace all four
  live call sites). The S-series content check (below) is the objective backstop for a missed branch.

**D3C1-C — APPROVED.** R `enter_tick` sampled at top-level `cnr3_arAllFramesReady` entry, stored in a
diagnostic-only, gate-compiled frameData field, read by the branch helper. Preferred over expanding the four
helper signatures.
- CONDITION: the field must be COPIED into a local BEFORE `cnr3_discard_frame_data_with_cache` deletes
  frameData; emit R from locals AFTER the successful discard. (Your §1.2/§8.2 sequence already does this — the
  decision ratifies it.)
- NOTE: R `exit_tick` will be captured at emission (branch-helper near-return), a hair before true top-level
  arAllFramesReady exit. Acceptable — `enter_tick` is the load-bearing view-(a) sort key and is correctly
  top-level; `exit_tick`/`run_ms` are display/derived. Do not add machinery to chase a truer exit tick.

**D3C1-D — gate structure APPROVED; the display-tag label is a COORDINATOR call.**
- APPROVED: the gate names `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` / `CNR3_DIAG_PRINT_DSUM_PLANTRACE`, the two-gate
  `#error` pattern, and the nested sub-flags (`..._VIEW_DATETIME/_VIEW_FRAME/_VIEW_PHASE`, `..._FROM_FRAME/
  _TO_FRAME`) with numbers INSIDE the feature `#if`. Do not alter DSUM01-14 gates.
- DESIGNER RECOMMENDATION on the display tag: nonnumeric `[DSUM-PLANTRACE]` (it is a per-frame TRACE, not a
  `[DSUM-SUMMARY]` aggregate — keep it visibly distinct from D-SUM-01..14). **W3X to ratify** numeric ID
  (e.g. D-SUM-15) vs nonnumeric. The gate NAME is independent of the tag, so this ratification does not block
  investigation; assume nonnumeric unless the coordinator says otherwise.

**D3C1-E — APPROVED, with the tick-width decided.**
- `frame_width = max(8, digits(max(abs(from), abs(to))))` — approved.
- `action_seq_width = max(8, digits(2 * window_frame_count))` — approved (<= 2 records per in-window frame in
  3c.1).
- `tick_width` — DECIDED: fixed 20 (uint64 max decimal width). Rationale: a fixed width guarantees
  lexicographic == numeric order for the external sort across the whole run, with no two-pass max and no risk
  of a late large tick breaking a precomputed width. Keep the defensive "never truncate; width-expand if the
  value exceeds nominal" rule.

**D3C1-F — APPROVED.** Clean-end dump in `cnr3_free_filter`, after D-SUM-14, before `freeNode`/`delete data`.
- CONDITIONS: once-guarded with ONLY the clean-end arm implemented (the bail arm is 3c.2 — do not add it);
  flush-always (R-PROCESS-24), explicit flush at end of the block; each of the three views independently
  sub-#ifdef gated with its legend, numbers/flags INSIDE the feature `#if`.

---

## 2. Additional designer notes / patch-time items (not blockers)

- **N1 — O-without-R must be graceful.** If arInitial records an O and that frame's arAllFramesReady later
  bails, the frame has an O and no R. A clean run has no bails so this should not arise, but the dump and the
  pairing views MUST handle a lone O without crashing or mis-pairing. Confirm in the patch.
- **N2 — RECOVERY_FLOOR O-`pinned` is likely empty at plan time.** exact/pred/cache have a concrete initial
  pin; the floor branch's pins are result-time (hole/floor adoption). At arInitial exit the RECOVERY_FLOOR
  O-record `pinned` may legitimately be empty. Confirm whether the floor branch takes any plan-time pin; if
  not, an empty O-`pinned` for FLOOR is correct, not a gap.
- **N3 — enter_tick symmetry.** Sample BOTH the O guard's enter (arInitial entry) and the R field's enter
  (arAllFramesReady entry) OUTSIDE the diagnostics mutex; bump `action_seq` INSIDE the mutex in the same
  critical section as the append (spec v2.2 §8 invariant — the load-bearing reason is the fmUnordered
  read-increment-write collision, not incidental).
- **N4 — no `[DSUM-SUMMARY]` for trace records.** Agreed; the per-frame O/R records use the plan-trace tag, not
  the summary tag.

---

## 3. Proof gate reminder for the 3c.1 patch (unchanged from scope §7)

- R-PROCESS-19 MACRO-OFF is THE exit gate: master gate off => buffer/records/writes/dump compile out, four-way
  identical, .vpy byte-identical on/off.
- Matrix presence: the new family block absent when off / present when on; DSUM01-14 matrix not regressed.
- S-series content sanity (the objective backstop, incl. for missed-invocation per D3C1-B): S1 all
  CACHE_HIT/RETURNED_COMPUTED, no recovery; S7/S8 show RECOVERY_EXACT plans with holes/sources consistent with
  the same run's D-SUM-12. O/R pair correctly by frame; Set 4 shows only C/K/L/U/N; window respected.

---

## 4. Bottom line

APPROVED to patch on D3C1-A..F with the conditions above. The one open item for the coordinator is the
display-tag naming (numeric vs nonnumeric) — recommend nonnumeric `[DSUM-PLANTRACE]`; not a blocker for
starting the patch. Bring the patch back for diff review before commit; the R branch-local insertions and the
frameData enter_tick field are the parts I will scrutinize hardest (R-PROCESS-25 + verify-invocation at all
four branches).
