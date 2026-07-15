Designer blurb v3.0 (2026-07-15)

Please read the paste starting with "Hello" first.

Hello. This chat is continuing from a prior "designer" chat which hit a hard limit. We had been, and will
continue here from where that chat left off, developing a VapourSynth DLL plugin (`vapoursynth-cnr3`)
according to specifications, where the choices in each step get validated against the intent of the
specs and sensibility.

I have Visual Studio 2026 with latest updates (we call vs2026) installed on my PC, with a local git
repository (dev branch `dev_cache_manager`) connected to the GitHub repository having the same dev
branch — https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager — where local VS2026
commits are pushed at the end of every agreed successful phase/subphase (sometimes called steps).

This chat takes the DESIGNER/REVIEWER (W3D) role for the CNR3 project — a VapourSynth DLL plugin
(vapoursynth-cnr3), VS2026, x64 /arch:AVX2, git dev branch dev_cache_manager pushed to
https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager at each agreed step.

You are responsible for the CMS specification and the associated rules in it.
In general - the coder is a separate entity responsible for responding to scope documents which
you develop and put to it, and assessing and generating patch proposals which you assess and
if appropriate approve or provide advice/correction/revised-scopes to the coder.

Three-party discipline: **W3X** = coordinator (me — relays artifacts between chats, runs builds/tests/
commits, holds decisions), **W3D** = you (designer/reviewer — investigate source, write patch scopes,
review coder confirm-reports and DIFFS, issue amendments/decisions, gate commits, and own the .vpy/.bat
test harnesses), **W3C** = coder (a separate memoryless chat — investigates, confirms before patching,
generates patches). You never write production patches; you scope and you review. The coder never
self-approves; you are the diff authority. I relay everything; upload-attached files sometimes arrive
empty in-context — they are still on disk at /mnt/user-data/uploads/ and must be read from there
(standing fallback, hit ~6x last session).

Read all attachments in order; do not act until I prompt you.

Read CNR3_Designer_Reviewer_Role_Handover_v3_0.md first (the role + method + playbook), then
CNR3_Handover_Introduction_to_new_reviewer_chat_v5_0.md (its currency note is the precise current state).

Attachments (latest versions), in read order:
  1. CNR3_Designer_Reviewer_Role_Handover_v3_0.md                (the role + method, in depth — read first)
  2. CNR3_Handover_Introduction_to_new_reviewer_chat_v5_0.md     (reviewer orientation; currency note = current state)
  3. Document_A_CNR3_Project_Context_and_Standing_Rules_v5_0.md  (standing rules incl. R-PROCESS-19..32)
  4. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v5_0.md (work plan / current state; top block authoritative)
  5. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v6_0.md          (detailed current-state delta)
  6. cnr3_cache_manager_design_v7_15.md                           (CMS design authority; UNCHANGED by all recent work)
  7. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v3_0.md(decisions ledger; all arcs + precedents)
  8. CNR3_CMS_Future_Investigations_and_Open_Questions_v9_0.md   (FI ledger + the parked queue)
  9. CNR3_cnr2_option_mapping_and_spec_v6.md                     (the cnr2<->cnr3 option surface authority)
 10. CNR3_Rider_Scope_OptionErrorMessages_v2.md                  (the IN-FLIGHT approved scope — no patch yet)
 11. A2_first_findings_v3.md                                     (fmParallel + plan-retry + HALF-500 evidence)
 12. CNR3_README_draft_user_options_v1.md                        (banked draft for the deferred doc patch)
 13. cnr3_diagnostics_specification_v1_5.md                      (D-SUM programme spec)
 14. src.zip                                                     (committed source baseline)

## WHERE WE ARE (precise)

**Committed marker: `CMS07-FEATURE.cnr2-descriptive-option-parser`** (pushed).

The plugin now has a real user-facing option surface. Eleven descriptive options
(y/u/v_threshold, y/u/v_strength, y/u/v_curve, scene_threshold, scene_chroma) are parsed at create time,
strictly validated (throw on out-of-range/wrong-type/bad-curve-string), applied to the existing config,
and echoed live in a `response_config:` log line alongside `edit_version=`. Defaults are cnr2-equivalent
(35/192/wide, 47/255/narrow, 47/255/narrow, 10.0, false).

**SHIP CONFIG (decided on evidence, for the PyPI-distributed DLL):**
`fmParallelRequests` + `CNR3_CACHE_PROFILE_HALF` (500) + `CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**.
Measured: 337 fps null-run / 274 fps end-to-end encode (5.5x realtime), duplicates ~0, re-churn 0.
Do not casually change these three; each is backed by a measured decision recorded in A2_first_findings_v3.

**CANONICAL SELFTEST COUNT IS CONFIG-DEPENDENT: currently 57/57** (56 base + diag3c2_induced_live_bail_
plantrace, which `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` enables). Forced-fail = 56/57 invariant_violation e1.
Under HALF, two NORMAL-geometry hot-zone tests visibly SKIP and the count stays 57. Do not re-trip on this:
a changed proof number must be LOCATED IN CODE, never hand-waved (this cost a round last session).

IN FLIGHT: **CNR3_RIDER.option-error-messages** — scope written, coder-APPROVED, **no patch exists**. The
prior coder chat degraded at its limit (emitted an inline "patch" then retracted it as "unvalidated draft
content"). A fresh coder chat must implement it from the scope. It is message-text-only: range errors echo
the received value (`got 256, expected an integer in the range 0..255 inclusive.`), type errors read
`incorrect value type, expected <same expectation>`, with the expectation text built ONCE per option kind
and shared by both paths. Validation logic must not change. Marker on success:
`CMS07-RIDER.option-error-messages`.

## NEXT STEPS IN ORDER

1. **Fresh coder chat implements the rider** from CNR3_Rider_Scope_OptionErrorMessages_v2.md. You cold-review
   the diff (shared expectation helper genuinely shared; `%.32s` width-limit + nullptr guard on the curve
   echo; deletion scan shows message text only).
2. **Run the invalid-option set ONCE against the rider build** — it simultaneously discharges the parser
   commit's DEFERRED gate items 8 and 9 and proves the new messages:
   `y_threshold=256`, `u_strength=-1`, `y_curve="o"` (cnr2 spelling must be REJECTED), `scene_threshold=101`,
   `scene_chroma=2`, `y_threshold="bad"` (type path) — each must fail the run cleanly with a clear message
   and non-zero exit. Then `y_threshold=0` and `u_threshold=0` must **SUCCEED** (0 is valid; the table
   builder already special-cases it centre-only) and show `y=0/192/wide` in response_config.
   If ANY invalid value is ACCEPTED, that is a parser bugfix, not the rider — stop and report.
3. **README doc patch** `CMS07-DOC.cnr2-descriptive-options-readme` — the user documentation half of the
   option surface. Draft banked (CNR3_README_draft_user_options_v1.md); v6 spec carries the same text.
4. **Reservation table** (parked, designed): the real fix for the fmParallel predecessor-in-flight race.
   Constraint recorded by W3X: the reservation table, CNR3_ENABLE_PLAN_RETRY_BIAS, and the profile gating
   must all COMPOSE (work separately or together).
5. **Residual desaturation** (3 frames / 957 on real interlaced footage, hard-cut/flashing content only).
   Users can now self-serve with `scene_chroma=True`. Only revisit if it proves to matter.
6. **A1 plan-trace analysis tool** / **A3 real-footage campaign** — unchanged, still parked.

## THE CODER RELATIONSHIP (what works — keep doing this)

- **Confirm-before-patch, always.** Scope = proposal; the coder's confirm-report = reconciliation with real
  source; patch only after they agree. That pass routinely catches what a scope missed — last session it
  caught a stale `348` in my own scope; my cold-verify of their catch then found a `grid_floor 25->15` THEY
  missed. Both directions matter.
- **GAP ANALYSIS BEFORE PATCH** (new, worked extremely well): for surface/parameter work, require a
  per-item table (parse/default/validate/apply — EXISTS file:line / MISSING / PARTIAL) plus definitive
  answers to the specific unknowns, BEFORE any patch. The later diff must map one-to-one onto the reported
  gaps; diff content with no corresponding gap is a review finding.
- **Verify the coder's claims COLD against source** (file:line); re-derive, don't accept summaries.
- **Verify INVOCATION, not just definition.** A defined observer with zero call sites compiles and silently
  breaks a balance.
- **Whole-patch deletion scan** on every diff.
- **R-PROCESS-25:** any touch of a proven line must be PROPOSED first.
- **Objective backstops beat argument:** macro-off byte-identical (R-PROCESS-19), marker-by-content, and
  hand-checks against raw counters have validated every "provably correct" claim.
- **The patch is the arbiter, not its notes.** Last session the coder's notes were wrong about their own
  patch three times (wrong debt content, notes listing a README the patch lacked, and calling a standalone
  patch a "delta"). An apply-test settles it in seconds; a description does not.
- **CODER-CHAT DEGRADATION IS REAL.** Both coder chats so far degraded near their limits (mangled run
  instructions; an inline unvalidated "patch"). Watch for it; escalate scrutiny; recommend a fresh chat.
- Scrutiny level: NORMAL-HARD, rising to exceptional for any patch delivered at/after a coder-session limit.

## HARD-WON MECHANICS (do not rediscover these)

- **Upload the ACTUAL current committed src** to coder and designer. Baseline drift cost a long, ugly
  session: the coder cut a patch against a reconstructed baseline that differed from the real tree, and
  every apply failed. Marker-check confirms commit identity but NOT working-tree edits.
- **DIFF BEFORE DIAGNOSING an apply failure.** I twice asserted a cause (CRLF; a 3-line offset) without
  diffing; both were wrong. The actual diff named it in seconds.
- **Verify the marker landed AFTER a commit.** The operational-defaults fix shipped without its marker
  bump — its logs under-claimed for a whole commit. Every proof gate should surface the marker somewhere.
- **Patch apply:** `git apply --ignore-whitespace` (with `--check` first); fallback `patch -p1 --binary`
  (tolerates CRLF and offsets). `git apply --3way` fails on untracked blobs. `git apply` is all-or-nothing:
  "Applied cleanly" for five files still means NOTHING applied if a sixth failed — check `git status`.
- **Use `/t:Rebuild`** for gate builds after any gate-macro/header change; incremental builds leave stale
  objects and silently mix configurations.

The full scope and context may not be totally clear until you have read all of the documents.
Note: this is a MID-TASK resume — the rider scope is approved and awaiting a fresh coder chat.

Read all documents in order; do not comment until I prompt you.

After reading, confirm your understanding of: the committed state (option parser live; ship config
fmParallelRequests+HALF-500+plan-retry-off) and the in-flight rider (scope approved, no patch); the
config-dependent 57/57 selftest count; the deferred invalid-option/threshold-zero gate items and that they
discharge against the rider build; the forward order (rider -> README doc patch -> reservation table); and
the R-PROCESS rules, especially gap-analysis-before-patch and the-patch-is-the-arbiter. Then I will hand
you the next artifact.
