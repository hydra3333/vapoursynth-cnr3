Keystone (CMS07-K.1) proposal — designer review: policy answers and CMS conformance.

(This is the second round of designer feedback on the keystone proposal. Round one was the
substance approval relayed earlier — predecessor contract right, two-phase split right,
ownership table right, Q16/Q18/Q19 ratified, the four tightenings, the §1 stutter. This
round gives the policy answers and the CMS-conformance points; there is no separate "part 1"
file to look for.)

This message is in LABELLED SECTIONS because it covers several distinct things: the three
policy questions you raised (Q14/Q15/Q17), five places where the proposal re-opens or
under-specifies something the CMS has already decided, a note about a new CMS version that
consolidates these, and a request for development diagnostics on this phase. Read the
section headers; they separate the topics.

Overarching point first, because it explains all of Section B: the keystone proposal is
strong and the predecessor contract is exactly right. But several keystone decisions were
reasoned forward from the getFrame path as if open, when the CMS had already decided them in
the recovery-design (§6, §9) and reference-count (§4) sections. None of these are new
behaviour to invent — they are existing CMS decisions to CONFORM to. Where I give a section
number, build to that section rather than re-deriving.

================================================================================
SECTION A — POLICY ANSWERS (Q14, Q15, Q17)
================================================================================

--- A1. Q14 (cold random seek, no anchor): CONFORM to CMS §9.5 — do NOT re-open it ---

Your §2.5/§11.6 framed this as an open choice between "reconstruct from frame 0" and
"controlled failure," with source[N-1] correctly excluded. That framing is off-spec: the
CMS already specifies the cold-seek predecessor policy in §9.5/§9.6, and it is NEITHER of
those two options. The answer is not yours (or mine) to choose — conform to §9.5.

The policy, stated plainly:

When output[N-1] is absent, run the §9.5 bounded recovery:
  1. Search backward from N-1, BOUNDED to the floor max(0, N-B), where B = BACK_RADIUS = 50
     (§10.1). Bound the interval itself; do not find a global nearest anchor and reject it
     afterward (§9.1). Do NOT scan the whole clip.
  2. If a present cached output is found in [floor, N-1] — a grid checkpoint, a
     cut-checkpoint (§6.4), or a recent-cache entry — that is the start point. Walk forward
     filling holes; EXACT. (§9.6.1.)
  3. If the search reaches the floor with nothing present — compute the FLOOR FRAME as a
     FRESH START (no predecessor, copy source chroma, reset semantics — the SAME operation
     as a cut-reset and as frame 0), then walk forward to N. (§9.5, "D29 approximate
     fresh-start.")
  4. Frame 0 is reached ONLY when N < B (floor clamps to 0). For a far seek the reset is at
     the floor — e.g. N=1000 resets at 950, NOT at frame 0.

What it means in practice (this is the part to internalise — both of your two options are
wrong):
  - NEVER "rebuild from frame 0" for a far seek — that ignores the 50-frame bound and
    rebuilds the whole clip instead of <= ~60 frames.
  - NEVER "fail because no anchor" — the floor fresh-start always produces a frame; refusing
    to produce a seekable frame is a worse outcome than a briefly-approximate one.
  - NEVER source[N-1] (R-ARCH-06).
  - Cost is bounded and clip-length-INDEPENDENT: <= (distance to nearest anchor, <=
    CHECKPOINT_INTERVAL=10) + B=50 ~= 60 frames worst case, because checkpoints are laid
    down every 10 frames INCLUDING during recovery, so any region becomes anchored after the
    first seek into it.

The honesty constraint you must carry (§9A.7, the bounded-start honesty rule): the floor
fresh-start is EXACT FROM THE FLOOR FORWARD but is a BOUNDED APPROXIMATION relative to a true
from-frame-0 recursion (the blend began at the floor, not at 0). This is the ONE place
CNR3's output is not bit-exact to a continuous-from-0 run. CR2 (§10.2) guarantees it is
invisible at N (B must exceed the blend's settling length). It MUST be disclosed in
diagnostics and MUST NEVER be described as exact full-history. A scene cut between floor and
N resets exactly at the cut, bounding the approximation further.

--- A2. Q15 (frame-0 bootstrap): REUSE the proven P.11C reset path; no separate helper ---

Your §2.2 open point asks whether the existing P.11C reset-copy machinery suffices to factor
frame 0 as a black-box source-copy/reset, or whether a small K.0/P.11D helper is needed
first. Answer: REUSE P.11C's proven reset-copy path; no separate K.0/P.11D pixel helper, and
therefore no sequencing question.

The reasoning (stated so it is RE-APPLICABLE — this same logic settles Q14 and Q17):
  - The CMS defines "fresh-start" as exactly: copy source chroma, skip the recursive blend
    (§9.2, line ~553).
  - The CMS explicitly says frame 0 IS a fresh-start: §9.5 describes the reset as "no
    predecessor, reset/start semantics — as if frame 0."
  - P.11C already PROVED that operation (its scene-reset path: output Y = source Y, U/V =
    source chroma, no blend).
  - Therefore frame 0 needs no new proof — it is an ALREADY-PROVEN operation triggered by a
    NEW condition (N==0).

The transferable principle: a "new" case that the CMS defines as an INSTANCE of an
already-proven operation needs new TRIGGERING/PLUMBING, not new PROOF. Frame 0, cut-reset,
and the Q14 floor-reset are ONE fresh-start operation triggered three ways — reuse it, do
not re-prove it three times.

The ONE thing to confirm when you write K.1E (the only genuine implementation detail): can
the proven reset path be INVOKED WITH NO PREDECESSOR FRAME PRESENT? P.11C was proven on
caller-supplied frames including a caller-supplied predecessor in its nine-plane triplet, and
P.11A validates that triplet before P.11C decides reset. Frame 0 has no predecessor frame.
  - If the reset path can run with a null/absent predecessor -> clean reuse: frame 0 = "call
    the reset path, pass no predecessor."
  - If P.11A's triplet validation structurally requires nine planes (so a missing predecessor
    fails validation before reset is reached) -> add a MINIMAL LIFECYCLE SHIM: skip
    predecessor-plane assembly/validation, call the proven source-copy directly. That shim is
    SOURCING/LIFECYCLE plumbing (it belongs in the keystone), NOT new pixel maths — the copy
    itself stays the proven P.11C operation.
Either way: no new pixel helper, no K.0/P.11D pixel proof. This also satisfies your own guard
rail — frame 0 using the reset path does not create a "frames may skip the predecessor"
precedent, because the reset path is DEFINED as the no-predecessor case; normal N>0 frames
still blend against the real cached predecessor.

--- A3. Q17 (scene-reset checkpoint): already decided by §6.4 — no special keystone logic ---

Your §9 and question 18 ask whether scene-change reset should mark output[N] as a checkpoint.
It ALREADY does, by the existing §6.4 rule — so the keystone adds nothing for it. §6.4 (and
§9.2, line ~554): a cut makes output[K] a fresh-start, "the IDEAL recovery anchor (exact, not
approximate)," and it is "stored WITH the checkpoint flag set." Scene-reset frames ARE
checkpoints automatically, via the normal compute-and-store path. The keystone lets §6.4
fire; it adds no scene-reset checkpoint handling of its own. (Checkpoint flag is monotonic
under duplicate stores, §6.6: raised, never cleared.)
Same principle as A1/A2: a decided CMS rule triggered by a condition, not new keystone code.

================================================================================
SECTION B — CMS CONFORMANCE: five decisions to CONFORM to, not re-derive
================================================================================

These are the five places (including the two policy questions above) where the proposal
treats as open/novel something §4/§6/§9 already decided. Build to the cited sections. Three
of these are corrections to the proposal's recovery description; two are the policy answers
above, repeated here so the conformance list is complete.

B1. COLD-SEEK POLICY -> §9.5 bounded recovery + floor fresh-start (see A1). The proposal
    re-opened it; conform to §9.5, do not choose "from-0 vs fail."

B2. SCENE-RESET CHECKPOINT -> §6.4 (see A3). Already auto-promoted; no special logic.

B3. RECOVERY REQUEST SET -> HOLES-ONLY, not a blanket span (§9.1 and §9.5.1). Your §2.4 says
    arInitial requests "source[A+1] through source[N-1] for recovery; source[N]" — that
    wording implies a CONTIGUOUS BLANKET from anchor+1 to N-1. The CMS is sharper: request
    "source N plus the sources for the genuine output holes ONLY ... NOT a blanket
    [max(0,N-B), N] source window" (§9.1), with the §9.5.1 example: start point at N-10,
    holes only at N-3 and N-1 -> request {N-3, N-1, N}, THREE sources, not eleven. Request
    HOLES, not SPAN. (If the holes happen to be contiguous, it is the same set — but build it
    as holes-only so it is correct when they are sparse.)

B4. RECOVERY PLANNER SHAPE -> §9.6.1 nearest-present-start-point + contiguous holes; §9.6.2
    AS3 DEFERRED. Your §2.4 "identify a valid anchor and the contiguous sequence of missing
    outputs" is correct but under-specified. The CMS planner (§9.6.1): the start point is the
    FIRST present output found descending from N-1; the holes are CONTIGUOUS from
    start_point+1 to N-1; N is recorded SEPARATELY as the repair target, never as a hole; and
    there is NO third "reused-intermediate" category (a present frame between start and N is
    either the start point or it was a hole). Build exactly this planner. Do NOT implement AS3
    (§9.6.2): it is RESERVED but has no reachable trigger under this planner and must not be
    built against a synthetic plan shape. (Your proposal correctly does not mention AS3 — keep
    it that way, and now you know WHY it is absent rather than forgotten.)

B5. OWNERSHIP / RETURN-TRANSFER / FAILURE CLEANUP -> §4.4-§4.8. Your §5 ownership table and
    Q16 derive the right answer, but it is stated verbatim in the CMS, so cite it and conform
    rather than re-derive:
      - Only the returned output frame N is TRANSFERRED to VapourSynth (§4.8).
      - A consumed predecessor or recovery intermediate is an INPUT -> RELEASED, not
        transferred (§4.8). Balance: lookup_owned_ref_acquired_total == released_total +
        transferred_total (§4.8).
      - Single-ownership / null-on-consume (§4.4): each ref/pin entry is released-or-
        transferred EXACTLY ONCE then nulled; cleanup releases only if still non-null; the
        null field is the idempotency guard (exactly one of {consume, cleanup} releases it).
      - frameData cleanup/destructor MUST unpin any pins still on the pin-list on EVERY exit
        path, including every early error return (§4.5/§4.6). freeFrame NEVER inside the cache
        lock (detach under lock, free after).
      - A cache-store duplicate/adopt is a NORMAL Category-A outcome (§9.3/§9.6.4): the loser
        duplicate is freed (duplicate_store_computed_but_discarded), the winner/adopt is
        correctness-complete and MUST NOT abort or emit. Only a HARD status (e.g. C.13B
        rejecting a non-contiguous plan) maps to setFilterError + bounded one-shot stderr
        OUTSIDE locks (the EMISSION half of what C.13B DETECTS, §9.6.4).

Note on the pattern: all five share one root — the proposal reasons forward from getFrame,
while the decisions live in §4/§6/§9. Connecting each keystone decision back to its owning
section is the fix, and Section C makes that connection durable.

================================================================================
SECTION C — NEW CMS VERSION (CMS07.7 adds a consolidating §9.7)
================================================================================

Because the keystone-relevant predecessor decision was distributed across §4.8, §6.3-6.6,
§9.1-9.3, §9.5-9.5.1, §9.6, and §9A.7, CMS07.7 (now issued) adds a new subsection §9.7 that
states the WHOLE predecessor-sourcing decision in ONE findable place, from the keystone's
point of view, with pointers to each owning section. It is NON-BEHAVIOURAL — it adds no rule
and changes no decision; it consolidates. The owning sections remain authoritative. (It was
added precisely because this proposal re-opened several already-decided points — see Section
B — so build the keystone's predecessor sourcing to §9.7.)

Build the keystone's predecessor sourcing to §9.7. Two parts of it are worth calling out
because they bear directly on the recovery search you will implement in K.1D:

  - §9.7.1/§9.7.2 make explicit that recovery uses ONE bounded backward search to the floor
    max(0,N-B), B = BACK_RADIUS = 50 — NOT a recent-cache search followed by a separate
    checkpoint search. Build a single descending scan over the present-frame index. Do NOT
    build a second checkpoint-specific search.
  - The search does NOT prefer checkpoints: the FIRST present output found wins (close or
    far). Checkpoints matter only because retention (§6.3) makes them the frames likely
    still present in a cold region — they are survivors the one search lands on, not search
    targets. CR3 (§10.2, BACK_RADIUS ~= 5 x CHECKPOINT_INTERVAL, 50 = 5 x 10) sizes the
    bound so ~5 checkpoints fall within reach, which is why the EXACT path is the common
    case and the floor fresh-start is the RARE fallback.

§9.7.6 one-line summary:
  predecessor for output[N] = (a) N==0 fresh-start; (b) cached output[N] returned directly;
  (c) output[N-1] present -> use it; (d) output[N-1] absent -> §9.5 bounded recovery (one
  search to floor N-B; first present output wins; exact anchor if found; else floor
  fresh-start); NEVER source[N-1].

(Two coherence-rule investigations surfaced while preparing §9.7 — CR2 settling-length
measurement and CR4 active_ceiling clamp tension — are recorded in the NON-NORMATIVE
companion CNR3_CMS_Future_Investigations_and_Open_Questions_v7.7.md as FI-02 and FI-03. They
change no CMS rule and are for the behavioural review at first real .vpy runs; they are NOT
implementation tasks for you now.)

================================================================================
SECTION D — DEVELOPMENT DIAGNOSTICS FOR THE KEYSTONE (requested for this phase)
================================================================================

This phase specifically warrants a GENEROUS set of TEMPORARY development diagnostics, because
the keystone's correctness is INVISIBLE in the output on sequential playback — a wrong
predecessor source produces a plausible-but-wrong frame and only diverges on seeks. The
§12.1 out-of-order test catches it as a pass/fail, but during development you need to SEE the
keystone's decision tree to debug it: a failing out-of-order test otherwise says "wrong" but
not "wrong WHERE."

Please add, for the keystone phases (K.1A-K.1G), per-frame trace diagnostics that log:
  - which predecessor branch fired (frame-0 fresh-start / cached-output-direct-return /
    cache-hit-predecessor / recovery / floor-fresh-start);
  - the recovery plan when branch (d) fires: start point, the hole list, and the source
    request set (to confirm holes-only per B3);
  - the pin/release tally for the frame (to confirm §4.4 balance and §4.5 cleanup);
  - a FLAG when the floor-approximation fired (ties to §9A.7 — you want to SEE it fire only
    in cold/no-anchor regions during seek testing, not spuriously).

Constraints on these diagnostics:
  - OUTSIDE the formal diagnostic framework: do NOT use CNR3_DIAG_* names (those are the
    permanent, compile-time-gated, observe-only framework with the R-PROCESS-19 macro-off
    obligation). Use a clearly-temporary marker, e.g. CNR3_KEYSTONE_DEV_TRACE_*, so it is
    visibly NOT part of the durable diagnostics and carries NO macro-off obligation.
  - stderr, never stdout; never inside a lock/atomic scope (consistent with the standing
    rules).
  - REMOVED in a post-proven cleanup patch: once K.1G's aggregate out-of-order proof is green
    and committed, a follow-up patch strips the dev-trace, leaving only whatever permanent
    observation the formal framework genuinely needs. This keeps the durable codebase clean
    while giving development the visibility this phase needs.

================================================================================
SECTION E — STATUS / NEXT STEP
================================================================================

The proposal is approved in substance (the predecessor contract, the two-phase split, the
ownership table, the black-box discipline, and the §12.1 out-of-order proof are all right).
Ratified earlier without change: Q16 ownership story, Q18 K.1A-K.1G decomposition, Q19
Category-B emission scope. The four earlier tightenings still stand (recovery contiguity
validated BY the C.13B guard; the recovery store-then-reacquire discipline not optimised
away; frame 0 always a checkpoint; §6 counters observe-only). Fix the §1 copy-paste stutter.

With Sections A-D folded in, you have all policy answers and all CMS conformance targets.
Proceed to K.1A (request-plan structures + diagnostics only, no functional getFrame) as a
read-first patch. Build K.1E's frame-0/no-anchor handling to A1+A2 (reuse the reset path;
confirm no-predecessor invocation; minimal shim only if needed). Keep it proposal/read-first
per the hard designer gate.
