# Document B v3.2.9 — UPDATE BLOCK (prepend to your existing v3.2.8 file)

**How to apply this:** Document B is maintained additively — each generation adds a status note on
top of the previous ones; the body (§§0–11) is deliberately left as the historical record and
self-flags as stale ("always confirm current build state from the repository"). This file is the
**v3.2.9 update block**. To produce the full v3.2.9 document:
1. Replace the document's **title/version/date front-matter** with the **NEW FRONT-MATTER** below.
2. Insert the **NEW v3.2.9 STATUS NOTE** below immediately ABOVE the existing
   "**v3.2.8 status note**" header.
3. Leave everything from the v3.2.8 status note downward (including the entire body §§0–11)
   **unchanged** — it remains the historical record, exactly as the prior generations were kept.

A new chat should treat the DELTA (`CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md`) as
the authoritative companion for current state; this block folds that state into Document B's own
format. The repository remains authoritative on build state over any document.

---

## NEW FRONT-MATTER (replaces the existing title/version/date block)

# Document B — CNR3 Work Plan and Current Build State (CMS07.7, RESUME)
**Version:** v3.2.9 (RESUME-state work plan; focused status update. The version label is kept at
the "3.2" generation to stay aligned with Document A v3.2; the `.9` patch level marks this update.
Records the **keystone now under way and committed through K.1D** — K.1A (request-plan structures +
temporary KDT dev-trace), K.1B (direct cached-output-return ownership, synthetic), K.1C (live
getFrame passthrough scaffold), and **K.1D (the first REAL output frame: copyFrame fresh-start
store/return)** — on top of the caller-supplied pixel path (P.10A–P.11C), the scalar→native bridge
(P.7A–P.9A), the scalar pixel pipeline (P.1A–P.6A), and the C.14A cache-core milestone. **Selftest
count is now 47/47** (forced-fail 46/47, exit 1; verbose 47/47). The next phase is **K.1E branch-(c)**
(live predecessor-present frame-1 compute), in flight at acknowledgement-accepted / pre-patch. The
full delta of the keystone work is recorded in the companion document
`CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md`; the §8 status notes here summarise it in
Document B's format. Earlier sections carry forward except the version pointers below.)
**Date:** 2026-06-23
**Role:** Current-state / work-plan document. It states the controlling authority, the **current
build state**, the working method, the immediate next phase, the proof obligations, and what must
not be implemented yet.
**Generation source:** repository git history (authoritative build state) + Production Spec v2.6 §3A
+ CMS07.7.
**Precedence:** volatile. If this document ever conflicts with the latest prevailing CMS, the CMS
wins. If it conflicts with Production Spec §3A on register-owned rules, §3A wins.

---

## NEW v3.2.9 STATUS NOTE (insert immediately ABOVE the existing "v3.2.8 status note")

**v3.2.9 status note (the KEYSTONE is under way — committed through K.1D; K.1E branch-(c) in flight):**
The controlling CMS is now **CMS07.7** (`cnr3_cache_manager_design_v7_7.1.md`); the Production Spec is
**v2.6**; the diagnostics spec is now **v1.5**; the non-normative companion is now **v7.8**
(`CNR3_CMS_Future_Investigations_and_Open_Questions_v7_8.md`, which contains **FI-04**). On top of the
caller-supplied pixel path (P.10A–P.11C) and the C.14A cache-core milestone, the **cache↔pixel /
getFrame keystone is now under way** — the hard designer gate where the proven cache core meets the
proven pixel chain inside VS getFrame scheduling. **Four keystone phases are committed and pushed;
selftest count is now 47/47** (forced-fail 46/47, exit 1; verbose 47/47, all priors present). The
keystone is being decomposed K.1A–K.1G.

- **K.1A — keystone request-plan structures + temporary KDT dev-trace** (count →46). Request-plan
  branch enum/struct; recovery request representation is a holes-list / source-set (never a blanket
  span); the hard-status branch is a **carrier** for existing C.13B guard results, not a new validator;
  `[KDT]`/`[KDT-SUMMARY]` formatting is driven by the plan structure. Guarded by `CNR3_KEYSTONE_DEV_TRACE`
  in `cnr3_build_config.h`. No getFrame wiring, no source lifecycle, no pixel-path call, no cache-semantic
  change, no VS header edit.
- **K.1B — direct cached-output-return ownership proof** (count →47), **synthetic-first**, using the
  **real** `Cnr3OwnedFrameRef` and **real** cache lookup/addref operations (counters OBSERVE real ops).
  Three cases: success 1/0/1 (acquired/released/transferred); cleanup-before-transfer 1/1/0; no-acquire
  miss 0/0/0. The synthetic sink models the getFrame-return boundary. The **real `VSFrame`
  return-to-VapourSynth was explicitly OWED** here — and is now expected to retire INSIDE branch-(c)
  work (the internal cached-predecessor hit), not via a separate getFrame-re-entry proof.
- **K.1C — live getFrame passthrough scaffold** (plugin-only; count stays 47). First live getFrame step,
  with **five R-ARCH-06 fences**: removable guard; a **distinct callback that gets replaced not extended**;
  the scaffold frame is **never cached / never a predecessor / never checkpointed** (structurally prevents
  contamination); a `[KDT] SCAFFOLD_NOT_FILTERED` marker; a return-point comment. **[KDT] is emitted ONLY
  inside getFrame, never at plugin load/registration.** A/B byte-compare harness green. Files:
  `src/vapoursynth-Cnr3.cpp` + `src/cnr3_build_config.h` only.
- **K.1D — live frame-0 fresh-start store/return** (plugin-only; count stays 47). **The first REAL CNR3
  output frame**: output[0] created, stored as cache-authoritative checkpoint, and returned through live
  getFrame; N>0 cleanly refused. Reached via `copyFrame(source, core)` (a bitwise, writable, caller-owned
  duplicate) because frame-0 fresh-start output[0] = source[0] byte-for-byte (no predecessor, no blend;
  luma always source-copy, chroma source-copy when no predecessor) — so **no proven code is touched**
  (zero contact with `cnr3_frame_processing.cpp` / P.11C). Verified against five review bars (ownership:
  `copyFrame` ref + `addFrameRef` ref = two owners each freed once, source freed-and-nulled after copy,
  post-store failure frees only the returned ref while the moved owned-ref handles the cache ref; defensive
  null/alias guards; proven code untouched; KDT `FRAME0-FRESH-START` / `REAL_OUTPUT_FRAME0`, N>0
  `NOT-YET-IMPLEMENTED branch=nonzero-before-predecessor-wiring`, guarded by
  `CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`, stderr-only; N>0 gated before arInitial). Four-way clean
  (47/47); A/B harness green (frame-0 byte-identical; N>0 clean refusal leaves a header-only y4m, no FRAME
  marker).

**THE K.1D REORIENTATION (durable lesson — see DELTA §2).** The FIRST K.1D patch was **DROPPED** because
it silently rewrote the body of the proven, selftested P.11C function and introduced a second
source-to-output copy orchestration, with undisclosed scope broadening into `cnr3_frame_processing.cpp`.
The standard sharpened and now load-bearing: **proven, selftested code is never modified — behaviour OR
internals — without explicit visible planning and designer approval IN ADVANCE; a passing four-way after
swapping internals is NOT proof of equivalence; if reuse appears to require touching proven code, RAISE it
as a design question, do not route around it.** The patch was withdrawn to the proposal stage (not
patched-and-fixed); the copyFrame solution above was the reorientation outcome — smaller, safer, no
proven-code contact.

**THE IMMEDIATE NEXT PHASE — K.1E branch-(c)** (`CMS07-K.1E-live-predecessor-present-frame1-compute-proof`),
in flight at **acknowledgement-accepted / pre-patch**. N==1 after K.1D stored output[0]: at `arInitial`
acquire cached output[0] as predecessor (real lookup/addref, carried in frameData) and request source[1];
at `arAllFramesReady` retrieve source[1], compute output[1] via the **proven P.11B** composition,
**release** the predecessor after use, store output[1] per existing checkpoint policy, return output[1].
Ownership (OPPOSITE tail to K.1B): acquired=1, released=1, transferred=0, balance=0. **Dependency
declaration changes `rpStrictSpatial` → `rpGeneral`** (resolves FI-04; conservative-correct for a
recursive filter; `fmUnordered` stays — `requestPattern` is a separate layer from `filterMode` and does
not affect the CMS7 cache). N>1 clean refusal (`branch=after-frame1-before-recovery-wiring`). Proves N==1
only.

**Designer review of the K.1E branch-(c) proposal: three confirmations accepted; a FOURTH is drafted but
NOT yet sent** (see DELTA §4):
  1. **Defer scene-change** — K.1E is predecessor-present composition only; P.11C already proves reset
     for a given threshold.
  2. **Frame-1 acceptance = predecessor WIRING proof, not blend math** (P.11B owns the math). KDT must
     prove the predecessor was specifically cached output[0] (`pred=0`, `pred_source=output_cache`,
     `pred_lookup=hit`) and was released (`pred_released=1`, `pred_balance=0`); AND there must be at
     least one **known-answer vector** so frame 1 has a real byte-check, not pure KDT self-report.
  3. **P.11B-call scope = thin exposure of proven code only** — P.11C body untouched; no re-routing of
     proven internals; no new pixel/copy algorithm; report-before-broadening. (The bar to watch hardest,
     per the dropped K.1D patch.)
  4. **[NOT YET SENT — the immediate next action]** Temporary live-path code (the N==1 gate / N>1 refusal
     control-flow, any scene-change-deferral stub, the KDT line) must be **uniformly, greppably marked**
     and annotated with **what replaces it and when** (cleanup = grep-and-remove, not archaeology); AND
     ask the coder to confirm the K.1C scaffold is fully removed from the committed tree.

**OPEN VERIFICATION carried into the next chat (DELTA §7):** confirm the K.1C scaffold (old guard
`CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD`, the passthrough callback, `SCAFFOLD_NOT_FILTERED`) is fully gone
from the **actual committed** `src/vapoursynth-Cnr3.cpp` and `src/cnr3_build_config.h` — it was renamed/
replaced at K.1D and verified in the K.1D DIFF, but NOT re-verified against the committed tree.
(Caution: a `vapoursynth-Cnr3_cpp.txt` circulating in uploads is a STALE 9,361-line copy that is NOT the
K.1D-committed file; its "scaffold" hits are a different family, `CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_
WARMUP_*_SCAFFOLD`. Audit the repo, not that file.) Also confirm the full set of files the keystone
touched against repo history (it appears larger than the two-file K.1D patch implied).

**Temporary diagnostics:** the keystone KDT dev-trace (`CNR3_KEYSTONE_DEV_TRACE`, `[KDT]`/`[KDT-SUMMARY]`)
is intentionally present and is scheduled for removal **post-K.1G** per diagnostics spec v1.5 §2.8 — both
the K.1A plan-driven formatter and the live frame-0 / scaffold formatters, plus the temporary guards.

**Confirmed error-frame signature (useful for all future N>0 / error checks):** when vspipe pulls a
frame the plugin refuses via `setFilterError`, it leaves a **header-only y4m** (a single `YUV4MPEG2 …`
line, ~49 bytes, `XLENGTH` reflecting the requested range, **no `FRAME` marker, no payload**) and exits
nonzero. So "header-only y4m, no FRAME marker = clean refusal." The robust N>0 signals are the nonzero
exit + the `NOT-YET-IMPLEMENTED` line; the output-file check is belt-and-braces.

**Owed after K.1E (carried):** branch (d) bounded recovery live wiring; multi-frame VS-LIFECYCLE-01
request-set proof; live scene-change threshold derivation + reset wiring; longer sequential recursive run
beyond N==1; post-K.1G KDT cleanup; the real `VSFrame` return (from K.1B, now retiring inside branch-(c));
fmParallel (a correctness phase); typed-row-pointer-vs-memcpy (a measured fmParallel performance phase,
proven bit-exact-output identical).

**Acceptance method unchanged:** four-way (Debug N/N exit 0; Release N/N exit 0; Release
`--force-fail-for-harness-proof` (N-1)/N exit 1; Release `--verbose` N/N exit 0, all priors) + the
coordinator-side harness (frame-0 A/B byte-identical; for K.1E add the frame-1 KDT check + known-answer
byte-check; retain the N>0/N>1 clean refusal). The K.1E-shaped harness is **not yet built** and is a
different shape (frame-1 is not byte-identical to source) — likely needs a constructed deterministic
source so output[1] is computable by hand (DELTA §5). The body of this document (§§4–5, 11) still
describes the H.1A-era state; always confirm current build state from the repository (§3).

— End of v3.2.9 update block.
