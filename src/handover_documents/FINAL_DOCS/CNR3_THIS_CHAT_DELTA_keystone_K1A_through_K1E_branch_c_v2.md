# CNR3 — THIS-CHAT DELTA (Keystone K.1A → K.1E branch-(c))

**Companion to:** `Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2_8.md`
**Status of this document:** the delta of everything done in the designer/reviewer chat that ran
*after* Document B v3.2.8 and Role Handover v1.5 were written. Those two documents were authored
at the **P.11C / pre-keystone doorstep**; this chat then drove the keystone from K.1A through
K.1D (committed) and K.1E branch-(c) (in flight). Read this alongside Document B; where this
document and Document B differ on current state, **this document is newer** — but the
**repository is always the final authority on build state** (check `CNR3_EDIT_VERSION` and the
selftest count in the committed source).

**How a new chat should use this:** read the Role Handover (role + disciplines) and the CMS
first, then this DELTA for current state, then run the **scaffold audit in Section 7 as the very
first action** (it is the one open verification this chat could not complete). Then pick up at
the immediate next action in Section 4.

---

## 0. ONE-SCREEN SUMMARY

- **Committed/pushed through `CMS07-K.1D`. Selftests 47/47** (forced-fail 46/47 exit 1; verbose
  47/47 showing all priors). Branch `dev_cache_manager`.
- **Keystone progress:** K.1A (request-plan + KDT) → K.1B (cached-output-return ownership, synthetic)
  → K.1C (live getFrame passthrough scaffold) → **K.1D (first REAL output[0] via copyFrame)** — all
  committed. **K.1E branch-(c)** (predecessor-present frame-1 compute) is **in flight at
  acknowledgement-accepted / pre-patch**.
- **Major safety episode:** the first K.1D patch was **DROPPED** for silently rewriting proven
  P.11C internals; reoriented to a `copyFrame` solution that touches no proven code. See Section 2.
- **Immediate next action:** send the **fourth K.1E confirmation** to the coder (temporary-code
  marking + scaffold-removal question — drafted, NOT yet sent). See Section 4.
- **One open verification:** confirm the K.1C scaffold is fully gone from the **actual committed
  tree** (this chat verified it only in the K.1D *diff*, not the committed file). See Section 7.
- **Doc updates this chat is producing** (so you can check them): Role Handover **v1.5 → v1.6**
  (state pointers + additive D16 / Example F / Part 4 trigger + two defect fixes — see §9),
  Introduction **v2.0 → v3.0** (large — its baseline was the obsolete CMS06.11/H15.6B era), and
  this DELTA as the Document B companion. See Section 9.

---

## 1. COMMITS SINCE THE LAST DOCUMENTS

All four are committed and pushed to `dev_cache_manager`. The keystone (cache core ↔ proven pixel
chain, inside VS getFrame scheduling) is being decomposed K.1A–K.1G; these are the first four.

### K.1A — `CMS07-K.1A: prove keystone request-plan dev trace` (selftest count → 46)
Request-plan branch enum + struct + temporary KDT dev-trace. Holes-only request-set proven
structurally (recovery request representation is a holes-list / source-set, never a blanket span).
Hard-status branch is a **carrier** for existing C.13B guard results, not a new validator. `[KDT]`
/ `[KDT-SUMMARY]` formatting is driven by the plan structure. No getFrame wiring, no source
lifecycle, no pixel-path call, no cache-semantic change, no VS header edit. Guarded by
`CNR3_KEYSTONE_DEV_TRACE` in `cnr3_build_config.h`. Four-way clean.

### K.1B — `CMS07-K.1B: prove direct cached-output return ownership` (selftest count → 47)
Direct-cached-output-return **ownership** proof, **synthetic-first**, using the **real**
`Cnr3OwnedFrameRef` and **real** cache lookup/addref operations (the counters OBSERVE real ops, not
a substitute). Three cases: success 1/0/1 (acquired/released/transferred); cleanup-before-transfer
1/1/0; no-acquire miss 0/0/0. The synthetic sink models the getFrame-return boundary. **Real
`VSFrame` return-to-VapourSynth was explicitly OWED** at this point — and is now expected to retire
*inside* branch-(c) work (the internal cached-predecessor hit), not via a separate getFrame-re-entry
proof (see Section 3, the K.1E investigation).

### K.1C — `CMS07-K.1C: live getFrame passthrough scaffold` (plugin-only; count stays 47)
First live getFrame step: a **passthrough scaffold** with **five R-ARCH-06 fences**: (1) removable
guard; (2) a **distinct callback** that gets *replaced*, not extended; (3) **[LOAD-BEARING] the
scaffold frame is never cached / never a predecessor / never checkpointed** (structurally prevents
contamination); (4) a `[KDT] SCAFFOLD_NOT_FILTERED` marker; (5) a return-point comment. Designer
added the requirement: **[KDT] is emitted ONLY inside getFrame, never at plugin load / registration**
(else fencing leaks and the harness negative check breaks). A/B byte-compare harness green. Files:
`src/vapoursynth-Cnr3.cpp` + `src/cnr3_build_config.h` only.

### K.1D — `CMS07-K.1D: prove live frame-0 fresh-start store/return` (plugin-only; count stays 47)
**The first REAL CNR3 output frame**: output[0] created, stored as cache-authoritative checkpoint,
and returned through live getFrame. Reached via the `copyFrame` reorientation (Section 2), so it
touches **no proven code**. N>0 is cleanly refused. Verified against five review bars (Section 2),
four-way clean (47/47), harness green. This is the bootstrap frame that branch-(c) builds on.

---

## 2. THE K.1D REORIENTATION (the key safety episode — read this)

This is the most important episode in the chat and the clearest demonstration of the
proven-code-stays-proven discipline. A new chat should internalise it.

**What was proposed and DROPPED.** The first K.1D patch
(`CMS07-K1D_live_frame0_fresh_start_store_return_proof.patch`) had a correct N>0 fence and correct
ownership plumbing, BUT it:
1. **silently rewrote the body of the proven, selftested P.11C function**
   (`cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl`) — replacing the reset branch's
   proven chroma-scalar assignment with a new full-native-plane-copy call;
2. **introduced a second source-to-output copy orchestration** (forbidden), hand-setting reset-summary
   flags (`scene_change_reset_output_used = true`) to *mimic* the reset path without *being* it;
3. **broadened scope undisclosed** into `cnr3_frame_processing.cpp` (+351) / `.h`, against the
   "report before broadening" commitment.

**The standard established/sharpened (now a durable rule).** Proven, selftested code is **never**
modified — behaviour OR internals — **without explicit visible planning and designer approval IN
ADVANCE**. A passing four-way after swapping internals is **NOT** proof of equivalence (it shows the
selftest did not *detect* a difference, not that there is none). If reuse appears to require touching
proven code, that is a **design question to RAISE, not a license to modify**. The patch was withdrawn
and reset to the proposal stage (not patched-and-fixed) — withdrawal is the correct response to a
proven-code breach.

**The reorientation (better than the original).** Step-back question: does frame-0 need a "copy
operation" at all? Frame-0 fresh-start output[0] = source[0] **byte-for-byte** (no predecessor, no
blend; luma always source-copy; chroma source-copy when no predecessor). So VapourSynth's own
`copyFrame(source, core)` primitive — a bitwise, writable, caller-owned duplicate (copy-on-write) —
suffices. Confirmed against the R76 header (`VSFrame *copyFrame(const VSFrame *f, VSCore *core)`;
duplicates the frame not just the reference; ownership transferred to caller) **and** against the
design line + CMS07.7 floor-fallback text (fresh-start = copy source chroma / reset semantics, same
operation as frame 0). Result: **copyFrame K.1D, NO K.1D.0 subphase, two-file scope, ZERO contact
with `cnr3_frame_processing.cpp` / P.11C.** The coder also refused a bogus self-predecessor shortcut
(source[0] as its own predecessor) unprompted — correct R-ARCH-06 instinct.

**The five review bars the copyFrame patch passed** (verified against the diff):
1. **Ownership** — `copyFrame` ref + `addFrameRef` ref = two owners (cache + caller), each freed
   once; source freed-and-nulled right after copy; **post-store failure frees only the returned ref
   (the moved owned-ref handles the cache ref)** — the subtle case, correct; `duplicate` store status
   accepted for return.
2. **Defensive guard** — `copyFrame` null-check + `output == source` alias guard present.
3. **Proven code untouched** — zero `frame_processing`/P.11C contact; scope = two files.
4. **KDT** — `FRAME0-FRESH-START` / `flag=REAL_OUTPUT_FRAME0`; N>0 `NOT-YET-IMPLEMENTED
   branch=nonzero-before-predecessor-wiring`; guarded by `CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`
   (renamed from the K.1C scaffold guard); stderr-only; no `SCAFFOLD_NOT_FILTERED`.
5. **N>0 gated before arInitial** — refused before any source request, no passthrough fallback.

**Acceptance achieved:** four-way unchanged (47/47, 46/47, 47/47) + harness green (frame-0
byte-identical; N>0 clean refusal — see Section 6 for the confirmed error-frame signature).

---

## 3. K.1E — FROM STANDALONE BRANCH (b) TO BRANCH (c) (the investigation)

**First proposed** as a standalone live **branch (b)** cached-output direct-return proof (request
frame 0 twice; second = cache hit). **Investigated and DEFERRED into branch (c)**, for three
documented reasons (all answered from R76 documentation, not testing):

1. **Option A (force outer-cache eviction) is unproven.** Dave's real-operation scenario — deep jump
   evicts frame 0 from the VS outer cache, rejump re-enters CNR3 getFrame — is a *genuine* trigger,
   so getFrame CAN be entered twice for one N. BUT the cache controls (`max_cache_size`,
   `SetVideoCache`, `setCacheMode`) are **pressure hints**; the docs say some calls **may be silently
   ignored**; none guarantees re-entry for a specific already-served frame index. Not reliable as a
   proof trigger without demonstration.
2. **The cached-hit is NOT a single caller-independent code path.** getFrame-return and internal
   predecessor-consumption share the lookup/addref **acquisition**, but diverge at the **disposition**:
   getFrame-return **TRANSFERS** the frame to VapourSynth; predecessor-consumption **USES it as input
   and RELEASES it**. Different ownership tail = different end-to-end path. Proving one does not prove
   the other.
3. **The arInitial-return contract is unfavourable.** R76 `VSFilterGetFrame`: `arInitial` requests
   inputs and returns NULL; a frame is returned only at `arAllFramesReady`. The source-filter
   exception does not apply to CNR3 (it is a dependency filter with an input node). "Request nothing
   at arInitial → still receive arAllFramesReady" is also not a documented guarantee. So the cached
   hit cannot be cleanly expressed in the getFrame activation lifecycle without leaning on undocumented
   behaviour.

**Conclusion (Option C):** defer/merge branch (b) into **branch (c)**, where the cache hit is
exercised **internally** (recursion lookup of output[N-1]) — which sidesteps the activation-lifecycle
problem AND exercises the use-and-release disposition. This also retires the K.1B "real VSFrame return"
owed item, in its correct context.

**K.1E re-proposed as branch (c):** `CMS07-K.1E-live-predecessor-present-frame1-compute-proof`.
- **Scenario:** N==1 after K.1D stored output[0]; at `arInitial` acquire cached output[0] as
  predecessor (real lookup/addref, carried in frameData) and request source[1]; at `arAllFramesReady`
  retrieve source[1], compute output[1] via the proven P.11B composition, **release** the predecessor
  after use, store output[1] per existing checkpoint policy, return output[1].
  **[SUPERSEDED — predecessor handling is now PIN-CARRY; see the 2026-06-23 pin-carry decision note at
  the end of this section.]**
- **Ownership (OPPOSITE tail to K.1B):** acquired=1, released=1, transferred=0, balance=0.
  **[SUPERSEDED — restated in pin-ledger terms by the 2026-06-23 pin-carry decision note at the end of
  this section.]**
- **Dependency declaration change:** `rpStrictSpatial` → `rpGeneral` (this resolves FI-04). Rationale:
  `rpStrictSpatial` ("only requests frame N to output frame N") stops being truthful once the filter
  is recursive and later requests bounded source ranges for recovery; `rpNoFrameReuse` is too
  optimistic for jump/recovery; `rpGeneral` is the conservative, correct declaration. **`fmUnordered`
  stays.** (See Section 8 for why `requestPattern` is a separate layer that does NOT affect the CMS7
  cache design — Dave raised this fear explicitly and it was resolved.)
- **N>1 refusal:** `NOT-YET-IMPLEMENTED branch=after-frame1-before-recovery-wiring`. K.1E proves N==1
  ONLY.

**Designer review → three confirmations (coder ACKNOWLEDGED all three; acknowledgement ACCEPTED):**
1. **Defer scene-change.** K.1E = predecessor-present composition only; P.11C already proves reset for
   a given threshold. Live scene-change threshold derivation + reset wiring deferred.
2. **Frame-1 acceptance reframed and strengthened.** K.1E proves the **predecessor WIRING** (cached
   output[0] correctly sourced / fed / released), **not** the blend math (P.11B owns that). Therefore
   require: (a) KDT proves the predecessor was **specifically cached output[0]** (`pred=0`,
   `pred_source=output_cache`, `pred_lookup=hit`) and was released (`pred_released=1`,
   `pred_balance=0`); and (b) **at least one known-answer vector** so frame 1 has a real byte-check, not
   pure KDT self-report (choose source[0]/source[1] so the recursive output is predictable and could
   only match if cached output[0] was the predecessor). KDT self-report alone is insufficient for a
   load-bearing claim.
3. **P.11B-call scope = thin exposure of proven code only.** Any `frame_processing` contact must be
   ONLY to expose/call the proven P.11B path; **P.11C body untouched**; no re-routing of proven
   internals; no new pixel/copy algorithm; **report-before-broadening** if a public entry/adaptation is
   needed. This is the bar to watch hardest, given the dropped K.1D patch.

**Coder's enriched frame-1 KDT line (agreed):**
`[KDT] instance=1 N=1 PREDECESSOR-PRESENT-COMPUTE pred=0 pred_lookup=hit pred_source=output_cache
src_req=1 src_get=1 pixel=1 store=ok pred_acquired=1 pred_released=1 pred_transferred=0 pred_balance=0
ret=1 flag=REAL_OUTPUT_FRAME`

**[2026-06-23 — PIN-CARRY DECISION (additive; supersedes the predecessor-handling and ownership wording in this section).]**
K.1E branch-(c) sources the predecessor by PIN-CARRY, not by taking a second VSFrame reference. The
foundational locking/pinning cross-check returned GREEN LIGHT — all Tier-1 fatals PASS on two independent
reads, and the per-invocation pin-list is caller-owned (INV-D1), so this is thin USE of already-proven
machinery, not a cache-core internals change.
- **Predecessor step** (supersedes the Scenario bullet's "acquire cached output[0] as predecessor (real
  lookup/addref, carried in frameData)" and the earlier "share the lookup/addref **acquisition**" framing):
  at `arInitial`, PIN cached output[0] via the proven AS1 fused `lookup_frame_and_record_pin` — it returns
  a BORROWED `const VSFrame*` and records a consumer-pin on the per-invocation pin-list, atomically; carry
  {borrowed pointer + predecessor frame number + pin-list} in frameData; request source[1]; return NULL.
  At `arAllFramesReady`, use the borrowed (still-pinned) predecessor into the proven P.11B, then DISCHARGE
  the pin. Discharge is wired on BOTH the `arAllFramesReady` arm AND the `arError` arm; the doubly-abandoned
  case (activation abandoned AND the frameData free callback never runs) is the benign residual below. No
  second VSFrame reference is ever taken for the predecessor — it is borrowed, kept alive by the pin
  (liveness comes from the pin, INV-B2; NOT from output[0]'s checkpoint status — leaning on the checkpoint
  flag would be the retired checkpoint-as-pin reasoning, R-RETIRED-03).
- **Ownership tail** (supersedes "acquired=1, released=1, transferred=0, balance=0", the same figures in
  the agreed KDT line above, and the ownership-balance figures in the §4 patch-review bars): the proof
  obligation is a PIN-LEDGER, not a ref-ledger — pin taken=1, discharged=1, `pin_count` balance=0, with
  ZERO predecessor VSFrame refs acquired or freed (borrowed). `transferred=1` applies to output[1] ONLY
  (the K.1B-proven return path), not to the predecessor.
- **KDT consequence:** the agreed frame-1 KDT line's ownership fields (`pred_acquired` / `pred_released` /
  `pred_transferred` / `pred_balance`) move to pin-ledger terms (e.g. `pred_pinned` / `pred_discharged` /
  `pred_pin_balance`, with no acquired/transferred for the borrowed predecessor); exact field names to be
  settled when the K.1E text plan is produced.
- **Benign residual (confirmed from committed code):** an abandoned activation's worst-case residual is an
  UNDISCHARGED PIN — frame-safe and crash-safe (`~Cnr3OutputCacheCore` is `= default` and RAII-frees each
  slot's `Cnr3OwnedFrameRef`; `clear()` is not on the free path → no frame-ref leak, no lifecycle_violation
  at teardown) but SILENT today; it is surfaced by the future end-of-run integrity report (deferred owed
  item, §10).
- **New K.1E work (not a pre-existing fact):** discharge must be WIRED INTO the frameData free callback —
  the single point covering normal completion AND a VS-freed abandon — new getFrame-side glue in
  `vapoursynth-Cnr3.cpp` (cross-check INV-F3).
This note does NOT change CMS §8.7 or any cache-core code; it records how K.1E sources the predecessor.

---

## 4. IMMEDIATE NEXT ACTIONS (in order)

1. **[NOT YET SENT — do this first] Send the FOURTH K.1E confirmation to the coder.** It was drafted
   in-chat but never sent. It requires, before any K.1E code:
   - **Temporary live-path code must be uniformly, greppably marked** (a single consistent comment
     prefix) AND annotated with **what replaces it and when**, so cleanup is grep-and-remove, not
     archaeology. The temporary elements in K.1E are: the KDT line (→ removed post-K.1G); the
     **N==1 gate / N>1 refusal control-flow** (→ replaced progressively as branches (c-extended)/(d)
     fill in); any **scene-change-deferral stub** (→ replaced when live scene-change wires in).
   - **Ask the coder to confirm the K.1C scaffold is fully removed from the committed tree** (this also
     feeds Section 7).
2. **After the fourth confirmation is accepted:** the coder produces the **read-first K.1E patch**
   (predecessor-present frame-1 compute). Designer reviews against: P.11B-call scope (diff-verified,
   P.11C untouched), ownership balance (acquired=1/released=1/transferred=0) **[now a PIN-ledger: pin
   taken=1/discharged=1/pin_count balance=0, zero predecessor refs — see the 2026-06-23 pin-carry note
   in §3]**, the five-fence pattern,
   and the four confirmations.
3. **Build the K.1E-shaped harness** (coordinator-side, parallel to the patch — see Section 6).
4. **Run K.1E:** four-way unchanged (47/47) + harness. Commit if green; advance baseline to K.1E.

---

## 5. ACCEPTANCE HARNESS — CURRENT STATE

- **`test_K1D_once_only_harness_AB.vpy` + `.bat`** (produced this chat; in the chat outputs, may need
  re-saving to disk). Frame-exact: scenario machinery bypassed, output = `clip_denoised_base`
  directly, vspipe `--start N --end N` (confirmed correct for R76) picks the exact output frame. Arg
  `mode=passthrough|processing`. The `.bat` does 3 runs (frame-0 processing, frame-0 bypass, frame-1
  processing = N>0 fence) + checks: **A** `fc /b` byte-identical; **B** `FRAME0-FRESH-START` present +
  no `SCAFFOLD_NOT_FILTERED`; **C** bypass has no `[KDT]`; **D** inverted (frame-1 **nonzero** exit =
  PASS) + `NOT-YET-IMPLEMENTED` present + **D3** no-frame. Redirection rule: echo lines `>>"file"`
  (stdout); vspipe lines `2>>"file"` (stderr, where KDT goes); `findstr /C:"[KDT]"` — the `/C:` is
  ESSENTIAL (literal match).
- **Predecessor harness:** `test_K1C_once_only_harness_AB.{vpy,bat}` (S1–S8 / B1–B7 scenario dict).
  A `p50` standard source (~7250 frames) satisfies all scenarios; `p25`/`i25` (~3625) would fail S6's
  5000-frame need. Field-split does not change frame count.
- **K.1E harness — NOT YET BUILT, and a DIFFERENT shape** (frame-1 is not byte-identical to source once
  the recursive blend is active). Plan: keep frame-0 A/B byte-identical; add a frame-1 **KDT-based**
  check (`PREDECESSOR-PRESENT-COMPUTE`, `pred=0`, `pred_lookup=hit`, `pred_source=output_cache`,
  `pred_released=1`, valid frame produced not header-only) **plus** the **known-answer byte-check**;
  retain the N>1 refusal check. Likely needs a **constructed deterministic source** (e.g. `BlankClip`
  with controlled chroma) so output[1] is computable by hand. The known-answer vector + the exact KDT
  field set must be settled with the coder before this harness is finalised.
- Harness is kept as an **independent** check; describe it to the coder, do not share the literal files
  unless a debugging need arises.

---

## 6. CONFIRMED ERROR-FRAME SIGNATURE (useful for all future N>0 / error checks)

When vspipe pulls a frame that the plugin refuses via `setFilterError`, it leaves a **header-only
y4m** and exits nonzero. Confirmed empirically at K.1D: a ~49-byte file containing exactly one line —
`YUV4MPEG2 C420 W720 H576 F50:1 Ip A0:0 XLENGTH=1` — with **no `FRAME` marker and no plane payload**.
(The `XLENGTH=1` reflects the requested `--start/--end` range, written before the frame was pulled;
it is not a delivered frame.) So **"header-only y4m, no FRAME marker = clean refusal = PASS."** The
robust N>0 signals are the **nonzero exit** + **`NOT-YET-IMPLEMENTED` present**; the output-file check
is belt-and-braces and its label can be read as PASS whenever the file is header-only.

---

## 7. OPEN SCAFFOLD AUDIT (run this FIRST in the new chat)

There are **two families** of temporary code:
- **Family 1 — KDT dev-trace** (`CNR3_KEYSTONE_DEV_TRACE`; `[KDT]` / `[KDT-SUMMARY]`). **Intentionally
  still present.** Planned removal **post-K.1G** per diagnostics spec v1.5 §2.8. Correct as-is.
- **Family 2 — the K.1C live-getFrame scaffold** (old guard `CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD`, the
  passthrough callback, `SCAFFOLD_NOT_FILTERED`). **REPLACED at K.1D** — the guard was renamed to
  `CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`, the passthrough `return source_frame` was replaced by the
  `copyFrame` → store → return path, and `SCAFFOLD_NOT_FILTERED` was removed from the live path. **This
  was verified in the K.1D DIFF at the time, but NOT re-verified against the actual committed tree.**

**The required first action:** grep the **actual committed** `src/vapoursynth-Cnr3.cpp` and
`src/cnr3_build_config.h` for any surviving K.1C-scaffold remnant — the old guard name
`CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD`, any `SCAFFOLD_NOT_FILTERED`, any orphaned passthrough-return
fragment. Clean result confirms Family 2 fully gone; any hit is flagged.

**Caution about uploads:** the `vapoursynth-Cnr3_cpp.txt` that was in this chat's uploads is **STALE**
— 9,361 lines, with **zero** `FRAME0_PROOF` / `FRAME0-FRESH-START`, so it is **NOT** the K.1D-committed
file. Its 11 "scaffold" hits are a **different family** (`CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_*_
SCAFFOLD`) unrelated to the K.1C live scaffold. Do not audit from that file — audit the repo.

**Related, also for the new chat:** Dave's recollection that **other `.cpp`/`.h` were touched (e.g. the
cache manager)** beyond the two-file K.1D patch is worth checking — the committed file set appears
larger than the K.1D patch implied. Confirm the touched-file set against the repo history rather than
inferring from the patches.

---

## 8. WHY rpGeneral DOES NOT THREATEN THE CMS7 CACHE DESIGN (Dave raised this fear; resolved)

Two **independent** knobs are set at `createVideoFilter`:
- **`filterMode`** (`fmUnordered` / `fmParallelRequests` / `fmParallel`) — the **concurrency/threading**
  question ("may VapourSynth call getFrame in parallel?"). **This is the layer the CMS7 cache design
  lives on, and it is unchanged: `fmUnordered` now, `fmParallel` later as a correctness goal.**
- **`dependencies[].requestPattern`** (`rpStrictSpatial` / `rpGeneral` / `rpNoFrameReuse`) — a **caching
  HINT** to VapourSynth about which **upstream INPUT-frame numbers** the filter will request, used to
  size/manage **VapourSynth's outer cache of frames flowing INTO CNR3**. It is **not** about CNR3's
  internal CMS7 output cache, not about threading, not about correctness.

These are orthogonal. The CMS7 cache is CNR3's **own internal output cache** (segmented pools, recovery,
pins) holding CNR3's filtered outputs; `requestPattern` concerns the **graph plumbing** between the
upstream node and CNR3. Getting `requestPattern` wrong can at worst cost performance (VapourSynth
evicting an input frame CNR3 will re-request) — **never** a correctness failure of the cache logic.
`rpStrictSpatial` was correct while K.1C/K.1D were genuinely 1:1 (passthrough, then frame-0 verbatim);
`rpGeneral` is the honest declaration now that CNR3 is becoming recursive. **The CMS7 cache design is
intact.** One-line model: `filterMode` = "may you call me in parallel?"; `requestPattern` = "what
input-frame-numbers will I ask upstream for?".

---

## 9. WHAT THE SUBSEQUENT DOC UPDATES CHANGE (so the new chat can verify them)

This chat is producing/updating the following. A new chat should **check each against this DELTA** to
confirm it was done correctly and that nothing was lost.

- **This DELTA** — the irreplaceable artifact; companion to Document B v3.2.8.
- **Role/Reviewer Handover `v1.5` → `v1.6`** — a **state-pointer plus small additive** update. Changes:
  (1) Part 2 "current state" advanced from "P.11C done (45/45), keystone not yet proposed" to
  "**K.1A–K.1D committed (47/47); K.1E branch-(c) in flight, pre-patch**"; (2) the document-version
  table (CMS **v7.8**, companion **v7.8** [FI-04 resolved into CMS §9.7.7], diagnostics **v1.5**,
  Document A **v3.3**, Document B **v3.2.9**, this DELTA); (3) Part 6B keystone hunting list annotated to
  mark **what K.1A–K.1D proved** (request-plan shape; synthetic ownership accounting; live passthrough
  scaffold; first real output[0] via copyFrame; N>0 clean refusal) and **what is still owed** (branch (c)
  predecessor consumption — in flight; branch (d) recovery; multi-frame VS-LIFECYCLE-01; live
  scene-change; post-K.1G KDT cleanup); (4) **additive** content — new discipline **D16
  (proven-code-stays-proven)**, worked **Example F (the K.1D reorientation)**, and a matching **Part 4
  trigger**; (5) two **defect fixes** carried in v1.5 — the closing version stamp (was "v1.1") and a
  duplicated v1.2 change-note line. **The existing disciplines D1–D15, worked Examples A–E, the accuracy
  rule, and the recorded vsCnr2 divergences are unchanged.** If a v1.6 you are handed differs from v1.5
  anywhere outside the documented changes above (the three state-pointer/annotation spots plus the
  additive D16 / Example F / Part 4 trigger and the two defect fixes), treat that as suspect.
- **Coder Introduction `v2.0` → `v3.0`** — a **large** rewrite, because v2.0's baseline (Part 2/3) is the
  **obsolete CMS06.11 / H15.6B / Document_A-B-C v2.0 cache era** — far older than even the pixel arc.
  The rewrite: Parts 2–3 re-pointed to the **keystone era** (current document set, K.1A–K.1D done, K.1E
  branch-(c) in flight, the keystone audit requirements); Part 1 (the reviewer role) and Part 4 (standing
  rules — dual-ownership proof, parallel-readiness caution, state quarantine) kept substantially as-is
  since they remain correct in spirit. Verify the new Part 2 matches Section 1 and Section 4 of this
  DELTA.
- **Document B `v3.2.8`** — **may not need regeneration.** This DELTA is effectively the Document B
  state update; a new chat can apply this DELTA's Sections 1–7 to the existing v3.2.8, or Document B can
  be bumped to fold them in. If Document B is regenerated, its new "current state" and "owed items" must
  match Sections 1, 3, 4, and 10 of this DELTA.

---

## 10. OWED-ITEMS LEDGER (carried forward)

- **branch (c) — predecessor consumption** — IN FLIGHT as K.1E (pre-patch).
- **branch (d) — bounded recovery live wiring.**
- **multi-frame VS-LIFECYCLE-01 request-set proof** — for recovery; K.1C/K.1D/K.1E prove SINGLE-frame
  request lifecycle only.
- **live scene-change threshold derivation + reset wiring** — deferred from K.1E; must reproduce
  vsCnr2's `diff_max` and match P.11C accumulation units.
- **longer sequential recursive run beyond N==1.**
- **post-K.1G KDT cleanup** — remove BOTH the K.1A plan-driven formatter AND the live frame-0 / scaffold
  formatters, plus the temporary guards, per diagnostics spec v1.5 §2.8.
- **real `VSFrame` return** (from K.1B) — now expected to retire INSIDE branch-(c) work.
- **fmParallel** — a CORRECTNESS phase, after the keystone wires single-threaded getFrame.
- **typed-row-pointer vs memcpy** — a measured fmParallel performance phase; any optimisation must be
  proven bit-exact-output identical to the memcpy path.
- **AS4 vs `discharge_all` atomicity-wording discrepancy** *(recorded 2026-06-23; CMS-text decision)* —
  `discharge_all()` takes one lock per token via `unpin_frame()`, while CMS §8.7 AS4 specifies one lock
  acquisition for the whole list. Correctness-safe today (INV-B2 guarantees no slot being discharged can
  vanish mid-walk); a register-vs-code WORDING discrepancy only. RULE ON before multi-pin recovery relies
  on it. Options: (a) relax AS4 wording to per-token, or (b) add a single-lock batch-discharge variant to
  match the register. Tied to the branch-(d) recovery step where multi-pin carriage first appears.
  (Records the decision as owed; does NOT change CMS §8.7 or `discharge_all`.)

---

## 11. DISCIPLINES REINFORCED THIS CHAT (additions/sharpenings to the existing D1–D15 set)

- **PROVEN-CODE-STAYS-PROVEN (sharpened).** Never modify proven/selftested code — behaviour OR internals
  — without explicit visible planning + designer approval IN ADVANCE. A passing four-way after swapping
  internals is NOT proof of equivalence. If reuse appears to need touching proven code, RAISE it as a
  design question; do not route around it. (The dropped K.1D patch is the worked example.)
- **Temporary code near the live path must be uniformly greppable-marked + annotated with its
  replacement phase** — so cleanup is grep-and-remove, never archaeology.
- **Lifecycle-contract questions are answered from DOCUMENTATION, not testing.** "Undocumented but works"
  is version-fragile and dangerous under fmParallel. (The arInitial-return question is the worked
  example — settled from the R76 contract, not from a passing test.)
- **`requestPattern` is a separate layer from `filterMode`** and cannot invalidate the CMS7 cache design
  (Section 8).
- **`copyFrame` = bitwise, writable, caller-owned duplicate** — the right primitive for verbatim frame-0.
- **Confirmed error-frame signature:** header-only y4m (no `FRAME` marker) = clean refusal (Section 6).

---

## 12. CONSTANTS (unchanged, restated for convenience)

- **Repo:** `github.com/hydra3333/vapoursynth-cnr3`, branch `dev_cache_manager`.
- **Local repo root:** `E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github`; build dir `vs\cnr3`
  (`x64\Debug\` and `x64\Release\cnr3_cache_core_selftest.exe`).
- **Build:** Visual Studio 2026, x64, Debug + Release of BOTH `cnr3` (the plugin DLL) and
  `cnr3_cache_core_selftest`.
- **VapourSynth:** R76 portable at `D:\TEST\Vapoursynth_x64_R76`.
- **Cardinal rule R-ARCH-06:** `output[N] = f(source[N], output[N-1])`; the predecessor is the previous
  **FILTERED OUTPUT** from the cache, **NEVER** source[N-1], and the cache is never seeded with a
  synthetic/bogus frame.
- **Four-way:** Debug normal (N/N exit 0); Release normal (N/N exit 0); Release
  `--force-fail-for-harness-proof` ((N-1)/N exit 1); Release `--verbose` (N/N exit 0, must show ALL
  priors). A fifth macro-off run is added only when a D-SUM compute gate is involved.
- **Authority chain:** CMS → Production Spec §3A → diagnostics spec → handover pack; **the repository
  wins on build state.**

— End of THIS-CHAT DELTA (Keystone K.1A → K.1E branch-(c)).
