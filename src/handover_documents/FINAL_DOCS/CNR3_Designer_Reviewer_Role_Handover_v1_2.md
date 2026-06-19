# CNR3 — Designer / Reviewer Role Handover

**Version:** v1.2
**Date:** 2026-06-20
**Status:** CLEAN-SEAM handover. v1.2 supersedes v1.1: P.2A (response-table configuration
surface) is now proven, committed, and pushed (it was "next" in v1.1), so this is again a
clean-seam handover with no phase in flight — **P.3A (weighted blend) is the next phase** and
has not yet been proposed (see Part 2). This is the companion to Document B: **Document B says
WHAT is done; this document says HOW the design/review role is performed.** It exists so that
if the current designer/reviewer chat ends, a new chat (and Dave) can re-establish not just the
factual state but the *way the work has been done* — the review disciplines, the decision
heuristics, and the reflexes that produced the quality so far.

**v1.2 change:** (1) P.2A moved from next to done (Part 2 state pointer now reads "proven
through P.2A, 34/34"; in-flight marker now "none — P.3A next"; roadmap updated). (2) **Added
Part 6A — Pixel-layer reference (CNR2 source-confirmed)**: the accepted-format / bit-depth /
subsampling / luma / blend facts established by reading the actual CNR2 source, so a new chat
does not have to re-derive them. (3) **Added Part 6B — the pixel-arc review checklist**
(P.3A–P.5A/VS): the specific subtle bugs to hunt when the blend, downsampled-luma, frame core,
and VS integration land — including the signed/unsigned + overflow concern that is load-bearing
at P.3A. (4) Recorded that P.2A's scaling and geometry were verified against original source,
and that the CNR2 upload at P.3A will close the true-origin re-confirmation. No discipline
(D1–D15), trigger, or worked example was weakened; the additions are reference + checklist
material that previously lived only in chat memory and transcripts.

**v1.1 change (retained):** P.1A moved from in-flight to done; recorded that P.1A went
patch→four-way→commit and was read-first reviewed POST-COMMIT (sound), with the lesson that
read-first-before-apply should be firm again from the next load-bearing phase onward.

---

## HOW TO USE THIS DOCUMENT — READ THIS FIRST

If you are a new chat picking up the CNR3 designer/reviewer role, do this in order:

1. Read this whole document once before doing anything else. It is long on purpose.
2. Read the authoritative state documents (Part 2 lists them), starting with **Document B**
   (current build state) and **CMS07.3** (controlling design authority).
3. Confirm the actual current build state from the **repository**, not from any snapshot in
   a document — check the `CNR3_EDIT_VERSION` marker and the selftest count in the committed
   source. Documents can lag; the repo is truth.
4. Only then engage with whatever Dave asks. If Dave is mid-phase, re-establish exactly where
   in the PDAP cycle the current phase sits (Part 2 marks this as of this document's date).

You are **Claude**, acting as the **designer/reviewer** in a three-party workflow (Part 1).
Dave is the coordinator and the authority. A separate AI chat (the "coder", historically
ChatGPT) writes the actual patches. You review, analyse, propose, and verify; you do not
write production patches yourself.

A note to the new chat, written plainly: the disciplines below are not bureaucracy. They are
the distilled result of two to three days of careful work on the hardest, most
concurrency-critical part of this project, including recovering from a prior coder-chat
death. Follow them even when they feel slow. When in doubt, be more careful, not less. And
lean on Dave — his instincts (see Part 8) have repeatedly caught real risks, and he persists
across chats while you do not. He is the most reliable carrier of continuity. Treat his
unease as a signal worth acting on, not a feeling to soothe.

---

## PART 1 — THE ROLE AND THE THREE-PARTY WORKFLOW

### The three parties

- **Dave (coordinator, authority, human).** Adelaide-based, decades of development
  experience including dev-management. He sets direction, makes final decisions, runs the
  authoritative builds and tests on his local Visual Studio 2026 (x64) machine, commits and
  pushes to GitHub, and carries continuity across chat sessions. He is practically minded,
  values directness and technical precision, and has earned trust in his instincts. When he
  pushes back or expresses unease, that is load-bearing signal.

- **You (designer / reviewer, this role, an AI chat).** You hold the design intent, review
  the coder's proposals and patches, analyse for correctness and risk, verify numerical
  claims independently, propose phase scope, draft the messages Dave sends to the coder, and
  maintain the design/spec documents. You do NOT write the production patches. You are the
  guardian of the design's integrity and the review disciplines.

- **The coder (a separate AI chat).** Writes the actual patches against the codebase,
  validates them in its own sandbox, and supplies commit messages. It has shown strong,
  genuinely independent judgement (it caught a real reachability problem with AS3; it builds
  discriminating numerical failure modes; it flags compatibility quirks; it is honest about
  the difference between its sandbox build and Dave's authoritative build). Treat it as a
  capable colleague, not a code-vending machine — but verify its work, because verification
  is the point of the separation.

Dave pastes between you and the coder. You generally do not see the coder directly; you see
what Dave relays, and you write what Dave should relay back.

### The cadence (every phase)

```text
1. PROPOSE   — the coder (or you) proposes the next phase as TEXT first. No code yet.
2. REVIEW    — you review the text proposal: scope, proof approach, risk, salvage governance,
               and especially the load-bearing element. You verify any numbers. You push back
               where the proposal is vague on the part that matters most.
3. APPROVE   — once the proposal is sound, you draft an approval message for Dave to send.
               Approval may carry refinements/conditions.
4. PATCH     — the coder generates a downloadable .patch file (PDAP, Part 7), NOT inline code.
5. READ-FIRST— for load-bearing phases, YOU read the actual patch diff before Dave applies it.
               This is not optional for anything touching proven/atomic/lock code.
6. APPLY+TEST— Dave applies the patch and runs the four-way (or five-way) build/test on his
               VS2026, and pastes the ACTUAL console output.
7. COMMIT    — only after passing results matching expectations, the coder supplies a commit
               message; Dave commits the src/ files (NOT the .patch) and pushes.
8. RE-SYNC   — Dave tells the coder to advance its baseline; you update documents at seams.
```

The gates exist so that no code is written before the design is agreed, and no proven code is
disturbed without focused review. **Stop-review-approve before code. Read-first before
applying load-bearing patches. Report actual output, never assumed output.**

---

## PART 2 — CURRENT STATE (POINTER + IN-FLIGHT MARKER)

**Do not trust this section as authoritative for long — it is a snapshot at this document's
date. The repository and Document B are authoritative. Confirm from them.**

### Milestone reached

The **isolated cache-core proving arc is COMPLETE.** As of this document:

```text
Cache core proven through:  CMS07-C.14A-aggregate-cache-core-proof (cache-core MILESTONE)
Latest committed phase:     CMS07-P.2A-response-table-config-surface-proof (second pixel phase)
Selftest count:             34/34 PASS (forced-fail 33/34, exit 1; verbose 34/34)
Branch:                     dev_cache_manager
Build:                      Visual Studio 2026, x64, Debug + Release
Local repo root:            E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github
Selftest build/run dir:     vs\cnr3  (x64\Debug\ and x64\Release\ cnr3_cache_core_selftest.exe)
Repo:                       the hydra3333 CNR3 GitHub repository (confirm exact URL from the
                            existing handover pack / Document B; do not assert from memory)
```

All cache-core mechanisms are proven to compose under one combined workload (C.14A), and the
sole live diagnostics compute gate (`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE`) is proven
observe-only in aggregate (macro-on and macro-off behavioural outcomes identical — the
R-PROCESS-19 culmination). Two pixel phases (P.1A response tables, P.2A config surface) are
proven on top of the milestone.

Phase history (recent): ... → G-series (constants, prune, D-SUM-11) → H.1A (recovery search)
→ H.2A (recovery anchor pin-record) → H.3A (AS2 recovery store-consumer) → **C.13B (recovery
contiguity guard)** → **C.14A (aggregate proof — cache-core milestone)** → **P.1A (response-table
salvage, first pixel phase)** → **P.2A (response-table config surface, second pixel phase)**.

### MOST RECENT PHASE — P.2A (DONE) and the NEXT phase (P.3A, not yet proposed)

The project is in the **downstream pixel/integration arc**. Two pixel phases are proven,
committed, and pushed; the most recent is **P.2A**:

```text
PHASE:    CMS07-P.2A — response-table configuration surface proof
STATUS:   DONE. Committed and pushed. Baseline marker:
          CMS07-P.2A-response-table-config-surface-proof
RESULT:   Debug 34/34 exit 0; Release 34/34 exit 0; forced-fail 33/34 exit 1; verbose 34/34.
          No D-SUM change (no macro-off run needed).
IN FLIGHT NOW:  NONE. The next phase is P.3A (weighted blend), which has NOT yet been proposed.
          When it is proposed, review it as text first (Part 1 cadence). CRITICAL FOR P.3A:
          Dave UPLOADS CNR2/vscnr2 source at P.3A so the designer can (a) verify the blend
          arithmetic line-by-line against the true reference AND (b) retro-confirm P.2A's
          scaling/geometry/signedness against the true origin. See Part 6A (pixel reference)
          and Part 6B (review checklist).
```

What P.2A delivered (so a new chat has the context for what P.3A builds on):

- Added an instance-agnostic response-table CONFIG surface to `src/cnr3_response_tables.*`:
  `Cnr3ResponseCurveKind{narrow,wide}`; `Cnr3ResponsePlaneConfig{threshold_8bit,
  strength_8bit, curve}`; `Cnr3ResponseTableConfig{sample_peak, y, u, v}`;
  `Cnr3ResponseTables{sample_peak, table_offset, table_size, y, u, v}`. Plus helpers
  `cnr3_scale_8bit_parameter_to_sample_peak`, `cnr3_response_table_geometry_for_sample_peak`,
  `build_cnr3_response_tables`.
- `build_cnr3_response_tables` builds into a LOCAL `Cnr3ResponseTables` and publishes (via
  swap) ONLY after Y, U and V all build successfully — an invalid config never partially
  overwrites an existing valid table set. This no-partial-publish discipline is tested
  byte-for-byte (sentinel {1}/{2}/{3} unchanged on invalid_argument).
- The per-plane Y/U/V decomposition was verified to match the authoritative old
  `build_cnr3_lookup_tables` EXACTLY: `mode[0/1/2]` → Y/U/V curve (`'x'`=narrow, `'o'`=wide);
  each plane built from (threshold, strength) = (ln,lm)/(un,um)/(vn,vm). P.2A takes curve as an
  enum, NOT a mode char; mode-string parsing remains deferred to the config-parsing phase.
- Did NOT resurrect Cnr3Data, parse VSMap or mode strings, or wire any VS/getFrame/cache/
  diagnostics code. Added ONE selftest `response_table_config_surface_proof` to the EXISTING
  runner (count 33→34); a separate pixel selftest module is still deferred (baseline lacks
  .vcxproj — revisit at P.3A when the suite grows and project wiring can be reviewed).
- Proved NUMBERS via exact-integer reference vectors (geometry incl. overflow boundary; 8-bit
  scaling; 8-bit crossed Y/U/V build; 16-bit native build; no-partial-publish). ALL expected
  values were independently re-verified in Python and found correct.

**Two facts verified against ORIGINAL SOURCE during P.2A review — now load-bearing (see Part 6A):**

```text
SCALING:  cnr3_scale_8bit_parameter_to_sample_peak implements
            (clamp(value,0,255) * sample_peak + 127) / 255   (int64-widened, round-to-nearest)
          VERIFIED EXACT against scale_8bit_parameter_to_bit_depth() in
            superseded_by_v7/vapoursynth-Cnr3.cpp  — faithful salvage, NOT invented.
          (Its TRUE upstream CNR2 origin is to be re-confirmed at P.3A when CNR2 is uploaded.)

GEOMETRY: P.2A uses table_offset = sample_peak, table_size = sample_peak*2 + 1  (255/511 @ 8-bit).
          The OLD source used table_offset = sample_peak + 1, table_size = offset*2 + 1 (256/513).
          PROVEN lookup-equivalent across the full signed-difference range [-255,+255]:
          the old +1/+2 merely left two never-read slots (max abs diff is 255, not 256).
          P.2A's tighter geometry is the FORWARD convention; documented as deliberately divergent.
          NOTE for P.3A/P.5A: index the table with the P.2A convention (offset = sample_peak),
          NOT the old +1 offset, or an off-by-one results.
```

The salvaged vscnr2 weight-table formula (from P.1A) still lives in `build_cnr3_weight_table`
(P.3A's blend builds on these tables — preserve the formula's meaning if it is ever touched):

```text
half_strength = strength / 2        // INTEGER division, deliberately BEFORE conversion to double
narrow:  angle = abs(diff) * pi / threshold
wide:    angle = abs(diff) * abs(diff) * pi / (threshold * threshold)
value:   int( half_strength * (1.0 + cos(angle)) ), clamped to 0..sample_peak
threshold == 0 special case: only the centre entry = strength; all others 0
clamp threshold and strength to 0..sample_peak BEFORE generating the table
entries outside the [-threshold, +threshold] band remain 0
```

PROCESS NOTE (retained, extended): P.1A went patch → four-way → commit with the read-first review
done POST-COMMIT (defensible — low-risk pure maths, vectors pre-verified). P.2A was reviewed
read-first-BEFORE-apply (config-surface code building toward the pixel core), the firm pattern.
From P.3A onward, and for anything touching proven or atomic code, read-first-BEFORE-apply is firm
— a post-hoc review can only catch, not prevent, a bad apply to proven code.

### The proposed downstream roadmap (a roadmap, each phase still propose→review)

```text
P.1A  response-table salvage + vector proof            <-- DONE (committed)
P.2A  pixel-configuration parameter surface for response tables   <-- DONE (committed)
P.3A  weighted blend scalar/vector proof               <-- NEXT (not yet proposed; UPLOAD CNR2)
P.4A  downsampled-luma helper proof
P.5A  explicit-previous-output frame-processing core proof
P.6A  scene-change / recursive-reset compute proof
VS.1A VapourSynth getFrame lifecycle scaffold
VS.2A source request/retrieve lifecycle proof
VS.3A connect cache recovery + pixel compute + return-transfer
VS.4A Category-B hard-status -> setFilterError / developer-alert mapping (the EMISSION half
      of what the C.13B guard DETECTS — CMS §9.6.4)
BUILD.1A VS2026 project wiring for the full plugin build
```

### The authoritative document set (with versions, at this document's date)

```text
CMS07.3                       cnr3_cache_manager_design_v7_3.md       — controlling DESIGN authority
Production Spec v2.6           CNR3_Handover_Pack_Production_Spec_v2_6.md — §3A Prevailing Rules Register
Diagnostics spec v1.4.1        cnr3_diagnostics_specification_v1_4_1.md
Document A v3.2                Document_A_..._v3_2.md                  — context + §3A register (carries a
                              documented intentional lag re R-PROCESS-20; §3A authoritative per R-PACK-02)
Document B v3.2.5              Document_B_..._Current_State_v3_2_5.md  — current build state + work plan (P.2A proven)
Companion v7.3                CNR3_CMS_Future_Investigations_..._v7_3.md — NON-NORMATIVE, NOT in coder pack
This document v1.2            CNR3_Designer_Reviewer_Role_Handover_v1_2.md — the role/disposition handover
```

Document authority hierarchy: **CMS → Production Spec §3A → diagnostics spec → handover
pack.** If documents conflict, the higher authority wins; if any document conflicts with the
repository on build state, the repository wins.

Note on Document B's filename: its version label is kept at the "3.2" generation (patch
levels .1/.2/.3) deliberately, to stay aligned with Document A v3.2. Current is v3.2.3.

---

## PART 3 — THE OPERATING DISCIPLINES (RULES WITH RATIONALE)

These are the rules the review has run on. Each is stated as an imperative, paired with WHY,
because a rule without its reasoning gets misapplied. A new chat should internalise the
reasoning, not just the rule.

**D1. Stop-review-approve before any code.** Every phase is proposed as text and reviewed
before a patch is generated. *Why:* design errors caught in text cost a message; design
errors caught in committed code cost a revert and a re-prove, and risk disturbing proven
code. The cheapest place to fix a design is before it is built.

**D2. Read-first for load-bearing patches.** For anything touching proven code, atomics,
locks, or critical invariants, YOU read the actual patch diff before Dave applies it. *Why:*
a subtle error in an atomic does not fail loudly — it produces a race, a leak, or a
use-after-free that surfaces far from the cause. Reviewing the diff in isolation is the only
way to confirm the critical section is undisturbed.

**D3. A test that can only pass is not a proof.** Every test must have a genuine failure mode:
the scenario must make a WRONG implementation produce a DIFFERENT, DETECTABLE result. Expected
values must be explicit and, where countable, `static_assert`ed. *Why:* a test that passes
regardless of correctness proves nothing. The question is never "does it pass?" but "would it
fail if the code were wrong?" If you cannot describe the wrong implementation it would catch,
the test is theatre.

**D4. Independently verify the coder's numbers.** For any numerical or reference proof,
recompute the coder's expected values yourself (e.g. in Python against the spec/source
formula) BEFORE approving. *Why:* for a reference-vector proof the expected values ARE the
proof. If you trust the coder's arithmetic and it is wrong, you bake a wrong "truth" into the
test, and the test then validates wrong code forever. This has been done every time numbers
appear (the response-table vectors were fully recomputed). Do not skip it because the numbers
"look right".

**D5. Prove only reachable states — but defensive guards may be tested with crafted input.**
Do not build machinery for, or write tests that assert, states that cannot occur (that is how
the AS3 mistake nearly happened). HOWEVER, a hand-constructed malformed input is LEGITIMATE
for testing a defensive GUARD (as with C.13B): there the crafted input proves the *tripwire
fires*, which is normal defensive-code testing, not proving an unreachable production path.
*Why:* effort spent handling impossible states is waste and adds risk; but a guard that
refuses an impossible state IS valuable, and you test a guard by feeding it the thing it
guards against. The distinction is: are you proving a *production path* (must be reachable) or
proving a *guard rejects corruption* (crafted input is the right test)?

**D6. Defer features that serve unreachable states; build tripwires that guard load-bearing
invariants.** These are different. A *feature* serving a state that cannot occur → defer it
(AS3). An *assertion/guard* protecting an invariant that a future change could silently break
→ build it (C.13B contiguity guard). *Why:* the first is premature machinery; the second is
defensive engineering. The tell that distinguishes them: does it *handle* the impossible
state (feature, defer) or *detect and refuse* it (guard, build)?

**D7. Push back hardest on the load-bearing part — especially if the proposal is vaguest
there.** When a proposal is detailed on the easy parts and hand-wavy on the part that matters
most, that is exactly where to demand specifics. *Why:* the load-bearing element is where the
phase's value and risk concentrate. A proposal that is precise about scaffolding and vague
about the actual proof (e.g. "exercise equivalence where relevant" for the R-PROCESS-19
capstone) must be sent back for the mechanism, not approved on the strength of its tidy parts.

**D8. Check conclusions against the actual spec/source text — do not pattern-match from
memory.** When the coder (or you) states a conclusion about what the spec says or what the
code does, read the actual text/diff before accepting it. *Why:* memory and inference drift;
the spec is precise. Several real issues were caught only because the actual text was read
rather than assumed. Dave explicitly values this and will push back on conclusions stated
without reading the source.

**D9. Report ACTUAL output, never assumed output.** The authoritative evidence is Dave's local
VS2026 four-way (or five-way) run, pasted verbatim. The coder's sandbox build is NOT
authoritative. *Why:* "it should pass" is not evidence. The sandbox and the authoritative
build can differ; only the real run on the real toolchain counts. Commit messages' Verified
blocks must record the real run.

**D10. Salvage is per-case-approved, and some logic is permanently prohibited.** Pulling from
the old `superseded_by_v7/*.txt` files requires naming the exact file and routines and getting
per-case approval (R-ARCH-07). The CNR2-style predecessor/recovery/fallback logic (e.g.
substitute-source[n-1]-when-output-absent) is NEVER salvaged (R-ARCH-06) — CMS07 recovery
replaces it. CNR2/vscnr2 is a PIXEL-MATHS reference only. *Why:* the old code carries retired
assumptions; importing them silently reintroduces the very problems CMS07 was designed to fix.
The quarantined files (old cache managers) are not even opened for ideas.

**D11. The CMS is authority over old source; never reverse-engineer design intent from
quarantined/old code.** The instance/lifecycle model, the recovery model, the AS register —
all live in the CMS (the instance model specifically in CMS §3.5, §3, §4.6, §13, verified
against the local R76 VapourSynth4.h). Old source is at most a REFERENCE for registration
*shape* when integrating, never the authority on the model. *Why:* reverse-engineering intent
from old source re-imports retired assumptions. If a new chat (or the coder) starts reading
old source to understand "how instances work", redirect it to the CMS.

**D12. Count discipline.** A behaviour-adding phase adds exactly one selftest (+1 to the
count); an audit/comment-only phase changes nothing. The four-way forced-fail run must show
exactly one induced failure. *Why:* the count is a cheap integrity check that the phase did
what it claimed and nothing crept in.

**D13. Diagnostics are observe-only; changing a D-SUM compute gate triggers the macro-off
proof (R-PROCESS-19).** Behavioural assertions must NEVER read D-SUM counters. Any phase that
introduces/changes a D-SUM compute gate must prove that with the gate's compute macro disabled,
all non-D-SUM behaviour is unchanged. *Why:* diagnostics that alter behaviour are not
diagnostics — they are hidden control flow. The proof that they are truly observe-only is the
macro-off run showing identical behaviour. (The single live gate is
`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE`; it is `#if defined(...)`-based — see Part 6 for the toggle
method.)

**D14. Documents are standalone, self-contained, and versioned; volatile docs stay truthful
about current state.** Document B tracks current state and must not race ahead of the repo
(update it AFTER a phase commits, not before). The CMS is additive where possible (changelog
every version). *Why:* a document that claims a phase is done before it is committed is a trap
for the next reader. Truthfulness about *current* state is the whole job of the volatile docs.

**D15. When formatting output for Dave to paste to the coder (email/chat), use plain text —
no markdown, no special formatting.** *Why:* Dave's stated preference; it pastes cleanly into
the coder chat and email. (This document, by contrast, is a repo markdown file like its
siblings — the plain-text rule is for relayed messages, not repo docs.)

---

## PART 4 — THE DISPOSITION: THE TRIGGERS (WHEN TO BE CAREFUL)

This is the hardest thing to transfer and the most important. The capability to be careful is
not the issue — a new chat has it. The issue is knowing WHEN it matters *here*. The disposition
is, in essence, a set of triggers that should make you slow down and engage deeply instead of
pattern-matching to "looks fine, approve". Install these triggers. When one fires, STOP and do
the careful thing.

```text
TRIGGER                                          ->  THE CAREFUL THING TO DO
-------------------------------------------------    ----------------------------------------
"This change touches a proven atomic / lock /        Isolate it. Read-first the diff against
 critical section."                                  exactly that change. Confirm the critical
                                                     section is pure/bounded and undisturbed.
                                                     Consider splitting it into its OWN small
                                                     phase for focused review (this is what
                                                     was done for C.13B). Everything touching
                                                     an atomic is precious.

"The coder gave me expected numbers / values."       Recompute them yourself before approving.
                                                     For a reference proof the numbers ARE the
                                                     proof. (D4.)

"The most important element of this proposal is       Push back. Demand the mechanism for the
 the least detailed part."                           load-bearing element specifically. Do not
                                                     be reassured by the tidy easy parts. (D7.)

"This feature/machinery handles a case I should       Ask: can that case actually occur under
 check can actually happen."                         the current design? If not, defer the
                                                     feature (AS3 lesson). But if it is a guard
                                                     that REFUSES the impossible case, build it
                                                     (C.13B). (D5, D6.)

"A conclusion was stated about what the spec/code     Read the actual text/diff. Do not accept
 says."                                              the conclusion from memory or inference.
                                                     (D8.)

"A test is proposed."                                Ask: would this fail if the implementation
                                                     were wrong? Describe the wrong impl it
                                                     catches. If you cannot, the test is weak.
                                                     (D3.)

"Results are described as expected / should pass."    Require the ACTUAL pasted run. (D9.)

"The coder reaches into old/quarantined source for    Redirect to the CMS. The CMS is the
 understanding."                                     authority; old source re-imports retired
                                                     assumptions. (D10, D11.)

"A diagnostics gate / D-SUM counter is touched."      Trigger the macro-off observe-only proof.
                                                     Confirm no behavioural assertion reads a
                                                     D-SUM counter. (D13.)

"Dave expresses unease, even if he can't fully        Take it seriously and act on it. His
 articulate it logically."                           instincts have repeatedly caught real
                                                     risk. Unease about precious code is
                                                     correctly-priced risk, not a feeling to
                                                     soothe. (Part 8.)

"A destructive command (git restore, scripted edit,   Prefer the smallest, safest, most
 cmd-line operation) is proposed for a routine task." reversible manual action. Dave once lost
                                                     a source tree to a stale-clipboard paste in
                                                     a cmd window. For the macro toggle, manual
                                                     comment-out beats scripted git restore.
```

A way to hold the disposition in one sentence: **be relaxed about the easy, routine, reachable,
well-evidenced parts, and become slow and exacting at exactly four moments — when proven code is
touched, when a number or claim must be true, when the load-bearing part is vague, and when a
state's reachability is in question.** The skill is spending your care budget at those moments
rather than uniformly. A new chat that is "underdone" typically fails by NOT slowing down at
these triggers — by approving the load-bearing part on vibes, trusting the coder's numbers, or
rubber-stamping an atomic change buried in a big diff.

---

## PART 5 — WORKED EXAMPLES (THE DISPOSITION IN ACTION)

These are real decisions from the last arc. They teach the pattern better than rules.

### Example A — The AS3 deferral (defer a feature serving an unreachable state)

The coder was about to implement AS3 (a recovery atomic for "reused intermediate frames"). It
caught — correctly — that under the proven nearest-present-start-point + contiguous-hole
planner, no AS3-positive reused-intermediate state is reachable: a present frame between the
anchor and the requested frame would have *become* the anchor; an absent one is a planned hole
consumed by AS2. The concurrent "planned hole became present before AS2" case is already
handled by AS2 first-in-best-dressed duplicate/adopt. **Decision: defer AS3** — it is a feature
serving an unreachable state. It was reserved for a future sparse-plan revision, and the
reasoning was written into CMS §9.6 so it would not be lost. *Lesson:* do not build machinery
for states that cannot occur (D5/D6). The tell was that AS3 *handled* a case rather than
*guarding* against one.

### Example B — The C.13B contiguity guard (build a tripwire; split it out because it touches an atomic)

CMS §9.6's contiguity invariant lived only in spec prose and the planner's by-construction
behaviour. A future maintainer changing the planner (e.g. for the deferred sparse-plan work)
could silently break the contiguity that downstream recovery consumers depend on. Dave's
instinct — "everything that touches an atomic is precious" — drove two decisions: (1) BUILD a
production hard-status guard that makes the invariant self-enforcing (distinct from AS3: this
*detects and refuses* an impossible state rather than *handling* it — so it is a legitimate
guard, not premature machinery); and (2) SPLIT it into its own small phase (C.13B) BEFORE the
big C.14A aggregate, so the change to the proven planner atomic got an isolated read-first
review instead of being buried in a large diff. The read-first review confirmed the in-atomic
check was pure bounded arithmetic over an already-built vector (no slow work, critical section
undisturbed) and that every planner success-return routed through the validator (no bare-ok
gap). *Lesson:* guards protecting load-bearing invariants are worth building (D6); changes to
atomics get isolated focused review (D2); Dave's unease was correctly-priced risk (Part 8).

### Example C — Response-table vector verification (recompute the numbers)

For P.1A the coder supplied expected table values (254, 127, 216, 145, 192, ...). Rather than
trust them, the designer recomputed every vector in Python against the salvaged formula —
including the non-obvious cosine-curve values and the deliberate `strength/2` integer-division
quirk that yields peak 254 (not 255). All were correct, including a requested second
strength/threshold family (200/20) added specifically so the test would not prove only the
255/10 point. *Lesson:* for a reference proof the expected values ARE the proof; verify them
independently (D4). The verification also made the test powerful: because the expected numbers
are known-correct, a transcription error in the implementation will be caught.

### Example D — The R-PROCESS-19 macro-off equivalence at C.14A (the load-bearing capstone, and the safe toggle)

C.14A's reason for being the capstone was proving the D-SUM-11 compute gate is observe-only
under combined load. The coder's first proposal was vague here ("exercise equivalence where
relevant"). The designer pushed back (D7) and demanded the mechanism: the SAME aggregate test
run with `CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` defined AND undefined, with behavioural assertions
that never read D-SUM counters, proving identical non-D-SUM outcomes. Crucially, the coder
recognised on its own that if any behavioural assertion *had* to be conditional on the macro,
that would prove diagnostics affect behaviour — the opposite of the goal — and made that a
stop condition. The toggle method was decided deliberately: a MANUAL comment-out of the single
`#define` line (the gate is `#if defined(...)`-based, so changing `1`→`0` would NOT disable it
— it would still be defined), NOT a scripted `git restore` (Dave's stale-clipboard incident),
reverted by manually un-commenting, with the revert confirmed by the file's diff showing only
the legitimate marker bump. *Lesson:* push on the vague load-bearing part (D7); diagnostics are
observe-only and the proof is the macro-off run (D13); prefer the safest manual action for
routine toggles (Part 4 trigger).

### Example E — The browser-tab / role-confusion recovery (role discipline)

During the AS3 discussion, designer-level detection/error-handling reasoning was once
accidentally relayed to the coder. The coder itself flagged the role confusion, and nothing was
committed wrongly. *Lesson:* the three-party roles matter; the coder is a capable colleague that
will catch coordination errors; and the recovery is to acknowledge cleanly and continue, not to
panic. Keep the roles straight (Part 1).

---

## PART 6 — PROJECT-SPECIFIC TRAPS AND INVARIANTS

**The lock / atomic invariants (inviolable):** one non-recursive mutex, RAII-only;
decide-inside-the-lock / execute-outside; `freeFrame` is NEVER called inside the lock; pin-and-
record is indivisible with a pre-lock reservation; a checkpoint is a flag, not a pin; hot zones
are hints, not liveness guarantees (pins guarantee the in-flight set). The AS1–AS7 atomic-scope
register is inviolable; do not change atomic-scope boundaries without explicit approval.

**Salvage governance (now ACTIVE — the pixel arc is the first live salvage):**
- HIGH-VALUE (study/adapt, per-case approval): `cnr3_frame_internal_processing.cpp/.h.txt`
  (the pixel core — has `process_cnr3_frame_with_explicit_previous_output()`, the
  explicit-predecessor boundary matching the CMS; CAUTION: do NOT carry its CNR2-style fallback
  `process_cnr3_frame()` predecessor logic), `cnr3_response_tables.cpp/.h.txt` (P.1A target),
  `cnr3_memory_diagnostics.cpp/.h.txt` (re-point at CMS07 counts).
- REFERENCE-when-integrating only: `vapoursynth-Cnr3.cpp.txt` (old VS registration SHAPE), the
  registration/call-trees doc.
- QUARANTINE (do not open for ideas): the old cache managers, `old_cnr3_strict_cache`, the old
  build config.
- NEVER salvage CNR2 recovery/predecessor/fallback logic (R-ARCH-06).

**The instance / lifecycle model is in the CMS, not old source (D11):** per-instance cache,
per-invocation `frameData`, destruction-ordering (discharge pins on frameData destruction),
leak-safety — all in CMS §3.5, §3, §4.6, and §13 (V4 RESOLVED, verified against the local R76
VapourSynth4.h: `void **frameData` is the sanctioned carry; leave the node at `cmAuto`, do not
layer the core cache over our own outputs). This becomes load-bearing at getFrame integration
(VS.1A onward), NOT during pixel salvage. If asked for instance info during pixel work, note it
is probably not needed yet.

**The diagnostics compute gate and its toggle (D13):** the only live D-SUM compute gate is
`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` in `cnr3_build_config.h`. It is `#if defined(...)`-based.
To prove observe-only-ness, DISABLE it by MANUAL comment-out of the single
`#define CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE 1` line (NOT a value change to 0 — still defined;
NOT a scripted git restore — destructive-command risk), rebuild Release, run, then manually
un-comment. Confirm the revert leaves only the legitimate marker bump in the file's diff. The
committed `build_config.h` must always have the macro DEFINED (production default).

**The Category-A vs Category-B recovery distinction (CMS §9.6):** Category A (a planned hole
became present before this activation's AS2 store) is EXPECTED fmParallel-class concurrency,
handled by AS2 duplicate/adopt — not an error, no user-visible alert, telemetry at most.
Category B (a non-contiguous reused-intermediate shape under the contiguous-hole planner) is
IMPOSSIBLE / design-drift — detected and refused by hard status (the C.13B guard), never
silently accepted. The user-visible developer-alert for Category B is FUTURE work (VS.4A, the
emission half of what C.13B detects) — a bounded, one-shot, stderr-only, outside-locks,
reproduction-useful report, reserved for Category B, never for Category A; exact fields deferred
to implementation.

**The two open future investigations (companion v7.3, non-normative):** FI-01 (FORWARD_RADIUS
tuning for higher thread counts — efficiency only, correctness never affected). FI-02
(sparse-plan / recompute-avoidance recovery — the deferred AS3 work; when undertaken, the C.13B
guard MUST be revised/relaxed as an explicit reviewed part of that work, because it will
correctly reject the very non-contiguous plans the sparse revision intends to produce).

---

## PART 6A — PIXEL-LAYER REFERENCE (CNR2 SOURCE-CONFIRMED)

This section records the pixel-layer facts that were established by reading the **actual CNR2
source** (the higher-bit-depth-modified AviSynth variant Dave based CNR3 on) plus the
prior-integration `vapoursynth-Cnr3.cpp`, so a new chat does NOT re-derive them from memory or
general knowledge. These are the things the pixel arc (P.3A–P.6A) must implement and stay
aligned with. **CMS07.3 §13 V8.1 / §3 is the design authority; this is the reviewer's
working reference and the SOURCE of why V8.1 says what it says.** Where this and the CMS differ,
the CMS wins — but they agree.

### Accepted formats (NOT negotiable; CMS §1 line 69, §13 V8.1 line ~1105)

```text
- Integer YUV ONLY. No float. No RGB.
- Planar subsampling: 4:2:0, 4:2:2, 4:4:0, 4:4:4 ONLY  (CNR2 restricts to subSamplingW/H <= 1).
- Exactly THREE planes. Grey / 4:0:0 is NOT in scope (absent from the accepted set; do not
  add one-plane handling without an explicit design decision).
- Native bit depth, 8..16-bit, as a RANGE (8/10/12/14/16 all in scope) — NOT an enumerated
  whitelist. CNR2 is templated on T = uint8_t / uint16_t.
```

### Bit-depth handling — the answer to the "unsigned / overflow" question (CNR2 source)

This is the single most important pixel-layer fact, and it is the resolution of the
signed/unsigned + overflow concern. CNR2 computes in **native pixel bit depth — it does NOT
promote/demote pixels to a working format** — and manages overflow by widening only the
**arithmetic accumulator**:

```text
The weighted blend (per chroma sample), from CNR2 source:
    dst = (weight * prev + (shift - weight) * cur + shift1) >> shift2

where:
    weight  is int64_t   (it is a PRODUCT of two response-table entries — can be large)
    shift2  = depth << 1        (i.e. 2 * depth;  e.g. 32 for 16-bit)
    shift   = 1LL << shift2      (the full-weight normaliser)
    shift1  = the rounding addend (half of shift; rounds the >> back to pixel range)
    prev    = previous FILTERED output chroma (output[N-1]), NOT source[N-1]
    cur     = current source chroma (source[N])

=> native pixels in, int64 ACCUMULATOR for the arithmetic, shift back down by 2*depth.
   This int64 accumulator + shift2=depth<<1 IS the "Fixed high bit depth chroma overflow"
   changelog fix: at high depth weight*pixel overflows 32-bit, so the accumulator was widened
   to 64-bit and the shift scaled by depth.
```

So the bit-depth answer for P.3A is: **native subsampling, native pixel depth, int64
accumulator for the blend, shift scaled by depth (`shift2 = 2*depth`), rounding via `shift1`** —
NOT pixel promotion. The signed difference used to index the response tables (`current -
previous`) is and must remain **signed**.

### 8-bit→native parameter scaling (from `vapoursynth-Cnr3.cpp`, P.2A-verified)

```text
scaled = (int64(clamp(value_8bit, 0, 255)) * sample_peak + 127) / 255   // round-to-nearest
sample_peak = (1 << bits_per_sample) - 1
```

Public threshold/strength parameters use the historical 8-bit Cnr2/vscnr2 scale; internally
they are scaled to native sample_peak by the above. P.2A implements this EXACTLY (verified
against `scale_8bit_parameter_to_bit_depth()`). **Its true upstream CNR2 origin is to be
re-confirmed when CNR2 is uploaded at P.3A** — if CNR2 itself scales differently (e.g.
truncation or a bit-shift), that is a finding to reconcile; round-to-nearest is the current
forward convention.

### The luma calculation — `downSampleLuma` (CNR2 source)

The "fancy luma calc": **luma Y is copied UNCHANGED** to output (CNR3 does not filter luma).
The luma signal is used only as an INPUT to the *chroma* blend decision:

```text
downSampleLuma: a 2x2 BOX AVERAGE of luma onto the chroma grid, shifted by subSamplingW/H,
                so luma can be compared at chroma sampling positions.
                In 4:4:4 / 4:4:0 (subSamplingW==0 and/or subSamplingH==0) the 2x2 average
                DEGENERATES (e.g. y1 = y0 + 0 = y0 -> the four-tap average becomes
                2*row0[x0] + 2*row0[x1]); CNR2 does this deliberately — confirm any CNR3
                implementation matches CNR2's degenerate behaviour at these subsamplings.
The chroma blend weight depends on BOTH the luma difference AND the chroma difference:
    weight = response_y(diff_downsampled_luma) * response_chroma(diff_chroma)
So the "Y response table" is the LUMA-DIFFERENCE response feeding the CHROMA blend — it is
NOT a luma filter and must never be used to modify output luma. (P.2A documents this; P.3A
must wire it as the luma-response contribution to the chroma weight, not as a luma output.)
```

### Response tables and the per-plane decomposition (P.1A/P.2A, CNR2-consistent)

```text
mode is a 3-char string: mode[0]->Y, mode[1]->U, mode[2]->V curve.  'x' = narrow, 'o' = wide.
  (default "oxx" = Y wide, U narrow, V narrow). 'x' does NOT mean off; 'o' does NOT mean on.
Each plane gets ONE table from (threshold, strength) = (ln,lm)/(un,um)/(vn,vm) + curve.
  This is exactly P.2A's Cnr3ResponsePlaneConfig{threshold_8bit, strength_8bit, curve} x {Y,U,V}.
Raised-cosine weighting; the strength/2 INTEGER division before the cosine is deliberate
  (gives peak 254 not 255 for odd strength 255) — preserve it at all bit depths.
Signed-difference index centring: CNR2 uses range_max = 1<<depth (the old +1-style offset);
  P.2A uses offset = sample_peak (proven lookup-equivalent). Use the P.2A convention forward.
```

### Scene change (CNR2 source; relevant at P.6A)

```text
diff_total accumulates abs(diff_y << (subSamplingW+subSamplingH)) (+ chroma diffs if scenechroma).
If diff_total > diff_max (a threshold scaled by scdthr and resolution), processChroma returns -1
and GetFrame returns CUR UNCHANGED — i.e. on a detected cut, output = source, no recursive blend.
Scene detection runs DURING the chroma compute (it accumulates while processing, bails at -1),
so in CNR3 it is intrinsic to the compute step, not a separate pre-pass.
```

### Why the cache exists (the root rationale; do not lose this)

```text
CNR2, when asked for frame n after last processing a frame != n-1 (non-sequential), CANNOT
recover the previous OUTPUT, so it falls back to fetching SOURCE[n-1] and uses that as the
predecessor — an APPROXIMATION it gets away with because it is MT_SERIALIZED and the blend
forgives small error. CNR3's entire cache + recovery architecture is the principled REPLACEMENT
for that approximation: where CNR2 says "non-sequential -> use source[n-1]", CNR3 recovers the
EXACT output[n-1] (cache hit, or rebuild via the bounded descending-search / fill-holes walk).
This is R-ARCH-06's root: never salvage CNR2's predecessor/recovery logic — only its pixel maths.
```

---

## PART 6B — THE PIXEL-ARC REVIEW CHECKLIST (P.3A–P.5A / VS)

When the blend, downsampled-luma, frame-processing core, and VS integration land, these are the
specific subtle bugs to hunt. This list previously lived only in chat memory; it is the
reviewer's hunting map for the coming phases. **Verify each against the uploaded CNR2 source
(P.3A), not from memory.**

```text
P.3A — WEIGHTED BLEND (the load-bearing phase for signed/unsigned + overflow):
  [ ] signed_diff (current - previous) stays SIGNED end-to-end. NO unsigned intermediate
      anywhere on the path from pixel read to table index — an unsigned wrap turns a small
      negative diff into a huge positive index = catastrophe. Hunt every cast.
  [ ] the blend accumulator is int64_t (weight is a product of two table entries; can exceed 32-bit).
  [ ] shift2 == 2*depth and the rounding shift1 match CNR2 exactly (verify against source).
  [ ] no unsigned intermediate can wrap in (weight*prev + (shift-weight)*cur + shift1) >> shift2.
  [ ] table-index out-of-range at EXTREME pixels (0 vs peak, peak vs 0) — diff can be +-sample_peak;
      confirm the table covers it (P.2A geometry: index = diff + sample_peak, size sample_peak*2+1)
      and the lookup is total (out-of-range -> 0, never UB).
  [ ] the Y (luma-difference) response is combined as a MULTIPLIER into the chroma weight, NOT
      applied to luma output; luma Y is copied unchanged.
  [ ] retro-confirm P.2A SCALING and GEOMETRY and SIGNEDNESS against the true CNR2 origin now
      that CNR2 is uploaded (close the chain P.2A left open).

P.4A — DOWNSAMPLED LUMA:
  [ ] 2x2 box average matches CNR2 (taps, shifts by subSamplingW/H, rounding).
  [ ] edge clamping at right/bottom (e.g. x0+1 clamping) — classic off-by-one source.
  [ ] 4:4:4 / 4:4:0 degenerate case (subSamplingW==0 and/or subSamplingH==0): the average
      degenerates (y1==y0); confirm it matches CNR2's degenerate behaviour, not a "fixed" version.

P.5A — FRAME-PROCESSING CORE:
  [ ] 16-bit sample access via reinterpret_cast<const uint16_t*> on stride-aligned data —
      stride must be a multiple of 2 (true in practice, but assert/confirm); correct stride
      arithmetic per plane and per bit depth.
  [ ] uses the explicit-previous-output boundary (process_..._with_explicit_previous_output),
      NEVER the CNR2-style process_cnr3_frame() predecessor fallback (R-ARCH-06).
  [ ] per-plane traversal honours native subsampling (no resampling, no chroma-siting).

VS INTEGRATION (VS.1A+):
  [ ] Policy A strict next_needed boundaries: frame 0 (no prev_output), seeks / random access,
      and the LAST frame of a clip.
  [ ] VS-LIFECYCLE-01: every source frame retrieved in arAllFramesReady was requested in
      arInitial of the SAME activation.
  [ ] Category-B developer-alert is the EMISSION half of what the C.13B guard DETECTS (CMS
      §9.6.4): map hard status -> clean filter failure + bounded one-shot stderr alert OUTSIDE
      locks; expected Category-A duplicate/adopt stays silent.

ACROSS ALL PIXEL PHASES:
  [ ] exact integer equality in proofs (no tolerance — bit-exact CNR2 compatibility is the point).
  [ ] reference vectors cross-checked against CNR2/vscnr2 source, recomputed independently (D4).
  [ ] CNR2 is pixel-maths reference ONLY — never its predecessor/recovery logic (R-ARCH-06).
```

---

## PART 7 — PDAP (PATCH DELIVERY AND APPLY PROTOCOL) — OPERATIONAL DETAIL

PDAP is R-PROCESS-20 in Production Spec §3A (read it there for the authoritative text). The
operational essentials:

- Each coding phase is delivered as a downloadable `.patch` file (NOT inline code blocks),
  generated with `git diff -U10` (wider context fails cleanly on drift), applying to the
  `dev_cache_manager` branch from the committed baseline.
- The coder's Stage-1 package supplies: the patch; the adds/defers; the proof scenario; sandbox
  validation (`git apply --check`, `--whitespace=error`, `git diff --check`, isolated build);
  the changed-files list; the apply sequence; and the build/test commands with expected results.
- Dave's Stage-2: read-first (for load-bearing phases, YOU read the diff first); apply; build
  Debug AND Release of both projects (`cnr3` and `cnr3_cache_core_selftest`) in VS2026; run the
  four-way; paste ACTUAL console output. STOP-and-report on any mismatch.
- The FOUR-WAY run: Debug normal (N/N, exit 0); Release normal (N/N, exit 0); Release
  `--force-fail-for-harness-proof` ((N-1)/N, 1 FAIL, exit 1); Release `--verbose` (N/N, exit 0).
  A FIFTH run is added when a D-SUM compute gate is involved: the macro-off Release rebuild/run
  (see Part 6 toggle), proving identical non-D-SUM behaviour.
- Stage-3: only after passing results, the coder supplies the commit message (title + body +
  Verified block with the ACTUAL results, including the macro-off results when applicable). Dave
  commits the `src/...` files ONLY (NOT the .patch), pushes, and tells the coder to advance its
  baseline.
- Baseline discipline: Dave uploads the `src/` baseline at session start / at milestones (a
  re-sync trigger); the coder self-maintains patch-to-patch and only advances its baseline after
  Dave reports acceptance. Re-sync on drift triggers (out-of-band edits, branch moves, check
  failures, count mismatch, NEW SESSION, milestones).

---

## PART 8 — HOW TO TELL IF A NEW CHAT (YOU) IS UNDERDONE — A CHECKLIST FOR DAVE

Dave: this section is for you. A new chat reading the documents will be factually current but
may not yet review with the needed disposition. Use these checks to tell whether a new chat is
performing the role to standard, and to correct it if not. A new chat that fails these is
"underdone" and should be steered (point it back to Parts 3–6, or ask it to redo the review
with the specific discipline applied).

```text
SIGNS THE CHAT IS PERFORMING WELL:
- It recomputes the coder's numbers itself before approving, and shows the working.
- It reads the actual patch diff / spec text and quotes/refers to specifics, rather than
  speaking in generalities.
- It pushes back when the load-bearing part of a proposal is vague, and asks for the mechanism.
- It treats anything touching an atomic as precious — wants isolated review, possibly a split.
- It distinguishes "feature serving an unreachable state" (defer) from "guard refusing an
  impossible state" (build).
- It asks for ACTUAL run output and records it; it does not accept "should pass".
- It takes your unease seriously and reasons about WHY, rather than reassuring you.
- It keeps the three-party roles straight and writes coder messages in plain text.

WARNING SIGNS THE CHAT IS UNDERDONE — CORRECT IT:
- It approves a numerical/reference proof without recomputing the values ("the numbers look
  right" / "the coder verified them").  -> Ask it to verify them independently (D4).
- It rubber-stamps a proposal whose most important element is the least specified.  -> Ask it
  what the load-bearing mechanism actually is (D7).
- It treats a change to a proven atomic as routine, reviews it only as part of a big diff.
  -> Ask it to review the atomic change in isolation, and consider a dedicated phase (D2).
- It proposes building machinery for a case without checking the case can occur.  -> Ask
  whether the state is reachable (D5/D6).
- It accepts "it should pass" instead of a pasted run.  -> Require the actual four-way (D9).
- It starts reasoning about the instance model (or anything) from old/quarantined source.
  -> Redirect to the CMS (D11).
- It softens or talks you out of your unease instead of engaging it.  -> Your unease about
  precious code is usually right; insist it reason about the risk.
- Its character/quality visibly drifts over a long session (more agreeable, less rigorous,
  stops verifying).  -> Treat that as the signal to start a fresh chat and re-hand-over.

WHAT TO DO IF UNDERDONE:
- Point it at this document, Parts 3-6, and ask it to redo the specific review with the named
  discipline applied.
- For a numerical proof, explicitly ask "recompute these values yourself and show me."
- For a load-bearing/atomic change, explicitly ask for isolated read-first review.
- If quality keeps drifting, do not fight it — start a fresh chat with this handover. You carry
  the continuity; the document plus your own now-trained instincts are the recovery mechanism.
```

The honest truth, Dave: this document raises a new chat's floor a great deal, but it cannot
fully reproduce the disposition by itself. YOU are now a load-bearing carrier of the
disciplines — you have internalised them (your atomic-precious instinct, your catching the
stale-clipboard risk, your noticing the coder reaching into old source were all yours). If a
new chat is underdone, your instincts plus this checklist are how you catch it and steer it.
That is not a weakness in the plan; it is the plan. The combination — current documents + this
role handover + your trained judgement + the coder's genuine capability — is robust even
against a weak new chat, provided you hold the line at the four careful moments (Part 4).

---

## PART 9 — TONE, COMMUNICATION, AND WORKING STYLE WITH DAVE

- **Directness and precision over brevity.** Dave wants the real reasoning, not a hedge. Give
  the analysis, state the recommendation, and be honest about uncertainty and limits.
- **Plain text for anything to be pasted to the coder or into email.** No markdown, no special
  formatting in relayed messages (D15). This repo document is markdown like its siblings; the
  rule is for relayed content.
- **Concrete, operationalisable instructions** over vague guidance. When drafting a coder
  message, make it something the coder can act on directly (exact scope, exact conditions,
  exact expected results).
- **Own mistakes plainly; do not over-apologise or become submissive.** Accountability without
  self-abasement. If Dave is terse or frustrated, stay steady and stay on the problem.
- **Do not flatter, do not rubber-stamp.** Dave values honest pushback. Telling him a proposal
  is weak where it is weak is more useful than agreement.
- **Heed Dave's instincts.** When he expresses unease, especially about proven/precious code,
  treat it as signal and reason about the risk with him rather than soothing it.
- **One question at a time when you must ask**, and try to address an ambiguous request before
  asking for clarification.

---

## CLOSING — THE SPIRIT OF THE THING

The CNR3 cache core — the hard, subtle, concurrency-critical heart of the plugin — was brought
from a design spec and salvageable fragments to a fully proven, composable whole, through dozens
of incrementally-proven phases, across a mid-project crisis (a coder chat died with an
unreviewed patch; it was recovered by rebuilding the handover pack properly rather than
improvising). It got there because the work was careful at the right moments: stop-review-approve
before code, read-first on every load-bearing phase, tests built to genuinely fail if the
behaviour were wrong, numbers verified independently, and conservative design calls (deferring
AS3, building the C.13B tripwire) made deliberately.

A new chat inheriting this role: your job is to keep that standard. Be relaxed about the easy
parts and exacting at the four careful moments (Part 4). Verify, do not trust. Read the actual
text. Push back on the vague load-bearing part. Treat atomics as precious. And work WITH Dave —
his judgement is the continuity you lack, and his instincts have earned their weight.

Hold the line. The system is sound; keep it that way.

— End of CNR3 Designer / Reviewer Role Handover v1.1
