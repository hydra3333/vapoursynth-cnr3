# CNR3 — DESIGNER RESPONSE: DIAG.3c.2 confirm-before-patch report — DECISIONS

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** `CMS07-DIAG.3c.2-confirm-before-patch-report.md` against spec v2.3 §4/§7/§8/§12, scope
`CNR3_Patch_Scope_DIAG3c2_dump_on_bail_v1.md`, and live source.
**Verdict:** confirm report ACCEPTED; the SITE-TO-CATEGORY TABLE is APPROVED. Proceed to patch on the
decisions below. The patch returns for diff review before commit (R-PROCESS-21/25).

## 0. What I verified COLD (not accepted from the report)
- 65 call sites: 14 arInitial / 50 arAllFramesReady (raw grep 51 includes the AR helper DEFINITION) / 1
  top-level. Matches.
- Category counts sum to 65 across all 16 categories.
- Table sampled against the actual `cnr3_set_filter_error` MESSAGES/branches in live source (line numbers
  shifted post-3c.1, so I checked by message not line): the 14 arInitial messages map to their assigned
  categories; the 3 DISCHARGE_FAILED sites (K.1F cache-hit / K.1E.3 predecessor / D.3 recovery pin-list) are
  exactly present; the 5 COPYFRAME_SOURCE_ALIAS sites match the count; AI-06 is a genuine mixed site (the
  recovery-refusal path itself discharges pins, which can fail). Source-location categorization, not
  message-parsing — correct.

## 1. DECISIONS

**M1 — APPROVED: per-site additive FAILED-writer calls + shared builders. Do NOT centralize via a
`cnr3_set_filter_error()` category parameter.** The reasoning holds: the common bail helper receives only
`frame_ctx`/`vsapi`/`message`; centralizing the FAILED-record write would force `Cnr3FilterData`/`n`/
`frame_data`/`request_data`/progress/E-X through a widened, more coupled signature AND still need branch-local
state assembly at the sites. Per-site keeps the failure facts where they are still known. Each site adds ONE
gated call (e.g. `cnr3_diag_plantrace_observe_failed_*()`) before the existing `set_filter_error(...)` +
`return nullptr`; the fact-assembly lives in shared builders so the 65 edits stay minimal and additive.

**M2 — APPROVED: explicit once-guarded bail-arm dump. Do NOT rely on `cnr3_free_filter()` running after a
bail.** Correct conservative call — VS teardown timing after `setFilterError` is not provable from source, and
R-PROCESS-24 requires bytes flushed before the failing `return nullptr`. Use the shared `buffer.dumped`
once-guard; if `cnr3_free_filter()` later runs, it sees `dumped=true` and emits nothing. Exactly one block per
run, either way.

**GATE — APPROVED: reuse the master plantrace compute/print gates only; NO nested bail sub-gate.** A bail
sub-gate would add 65 preprocessor decision points for marginal value; master-off already gives the
R-PROCESS-19 byte-identical exit gate (the 65 sites revert to the original `set_filter_error` + `return
nullptr`). `build_config.h` is marker-only this cycle.

**Item 4 (E convention) — DECIDED: E = the ACTUAL frame the failing operation was on, everywhere (recovery
AND non-recovery). Do NOT force E to the record frame n.** So an acquire-ref failure on a predecessor/base is
`E = predecessor/base`, a hole failure is `E = that hole`, a target failure is `E = target`. Rationale: one
consistent rule; E always means "the frame it failed on"; the frame is already resolvable in the O record
(predecessor/anchor/floor/hole/target roles), and A1 can map E back to the record frame via the `frame=`
field. Precision beats the display-uniformity of forcing E=n — `fail_reason=ACQUIRE_REF_FAILED` + `E=n` would
hide WHICH dependency failed. Paired rule for X: **X = unreached RECOVERY-PLAN items only (holes + recovery
target); X = [] for cache_hit/frame0/pred_present** (they have no multi-item plan), which is why the report's
non-recovery rows correctly show X=[]. (Coordinator: this is a display-semantics convention — flag if you'd
prefer E=n-for-non-recovery uniformity; I've chosen precision.)

**Item 5 (AI-06 mixed site) — APPROVED, with a preference.** Resolve by a CONTROL-FLOW distinction, never by
message-string parsing. PREFER splitting AI-06 into TWO adjacent gated FAILED-writer calls — one on the
refusal path (15 RECOVERY_PLAN_FAILED_OR_REFUSED), one on the discharge-failure path (8 DISCHARGE_FAILED) — if
the two causes sit on separable code paths before the shared `set_filter_error`. If they genuinely converge at
one point, a local category variable set by the `discard_status` check (not the message) is acceptable. Either
way the category is code-derived, not text-derived.

**PROOF METHOD — APPROVED, with one refinement.** The two fixture triggers are good (non-null `frame_data`
slot -> arInitial INVALID_LIFECYCLE bail with a minimal record + no O; stub VSAPI null `getFrameFilter` ->
arAllFramesReady SOURCE_RETRIEVAL_FAILED with live `request_data` + an O record). REFINEMENT: make the
arAllFramesReady trigger hit a RECOVERY hole/target site so **X (plan remainder) is non-empty and exercised** —
that is the most complex derivation and must be proven, not just the empty-X non-recovery case. So prove at
least: one arInitial minimal bail, and one recovery-branch bail showing `outcome=FAILED` + correct
`fail_reason` + `E` on the failing item + non-empty `X` = unreached holes/target + progress-so-far from prior
outcomes.

## 2. Conditions for the patch (R-PROCESS-21/25 in force)
- The site-to-category table IS the proposal for the per-site edits; it is approved, so proceed — but the diff
  review will re-check every site's category and additive form against the patched source.
- ADDITIVE ONLY at each site: the gated FAILED-writer call, then the EXISTING `set_filter_error` + `return
  nullptr`. No reorder/restructure/merge of any bail path.
- The 3 DISCHARGE_FAILED sites (AR-04/13/40) + AI-06's discharge cause: copy the needed plan/progress facts
  into diagnostic-only locals BEFORE `cnr3_discard_frame_data_with_cache()` (it deletes `request_data`), then
  write the FAILED record after the `discard_status` check.
- Whole-patch DELETION SCAN at review (expect near-zero deletions — this is additive). Verify INVOCATION: all
  65 sites wired (a FAILED-writer defined but not called at some sites would silently drop those failures —
  trace every site's call, the D-1 reflex).
- Clean-run S1/S7/S8 must be BYTE-IDENTICAL to 3c.1 (no FAILED records, no `fail_reason`/X/E on success).
- Master-off: the 65 sites byte-identical to the pre-3c.2 originals; whole family compiles out; .vpy A/B still PASS.

## 3. Bottom line
APPROVED to patch on M1(a) / M2 / master-gate-only / E=actual-everywhere / AI-06 split-preferred / the refined
proof method, under the R-PROCESS-21/25 conditions above. Bring the patch back for diff review before commit;
the per-site additive form, the discharge-site + AI-06 lifecycle handling, and the recovery X-derivation are
what I will scrutinize hardest.
