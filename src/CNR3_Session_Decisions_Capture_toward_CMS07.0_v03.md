# CNR3 Cache Manager — Session Decisions Capture (Staging toward CMS07.0)

**Status:** Working ledger of settled decisions from design-coordination session.
NOT a spec. This is the staging ground from which CMS07.0 will be assembled.
Extendable — append new settled items as later questions close.

**Version:** v03 — adds Items 3+7 CLOSED (parameter coherence rules, decay_margin,
instrumentation-discipline restatement). v02 added Item 6 + bounded prune. v01
covered foundation, Position B, pinning, arInitial/arAllFramesReady atomics, the
new hot-zone scheme, checkpoints, Q1/Q2.

**Context:** This session re-examined the cache manager architecture via a
"dummy question" / devil's-advocate walkthrough. The outcome is an
architectural supersession (CMS06.11 → CMS07.0), not a refinement. The pinning
model and the hot-zone model both change substantially. Decisions below are
settled unless marked OPEN or VERIFY.

---

## 0. Headline architectural change

The session moved the design from:

- **OLD (CMS06.11 and earlier):** held-ref-only for predecessors; hot zones as
  the *findability guarantee* for active frames; non-checkpoint pinning held as
  a *deferred emergency escalation* (Section 4.4) triggered by
  `predecessor_missing_when_expected`.

to:

- **NEW (toward CMS07.0):** **consumer-pins as the correctness floor** for all
  actively-needed frames; the cache is the **complete liveness index** for the
  active set; **hot zones demoted** to anticipatory/decay prune-policy *hints*
  with no correctness stake.

This is justified by the stated final goal (fmParallel) and, independently, by
KISS / conceptual-self-consistency / long-horizon-maintainability.

---

## 1. Foundational model (settled)

**1.1 The cache holds references, not pixels.** A cache slot holds an
`addFrameRef` reference to a VapourSynth-core-owned `VSFrame`; it does NOT own
or store pixel data. Pixel memory is core-owned and reclaimed by the core when
the frame's atomic refcount reaches zero. Single most clarifying frame of the
session: *"the cache holds references, not pixels."*

**1.2 Two leak surfaces, not one.**
- (a) Our per-request `frameData` struct — plain heap memory, **we** allocate
  and **we** free (new/delete or malloc/free). The core carries the *pointer*
  between activations (arInitial → arAllFramesReady) but never owns the struct.
- (b) `VSFrame` references — core-owned memory; our obligation is **ref
  discipline** (`freeFrame` every ref we take), never deallocation.
- **Ordering rule:** on frameData destruction, first discharge all refs the
  struct tracks (walk the pin-list, freeFrame/unpin), THEN free the struct.
  Free the struct first → lose the list of owed refs → leak. Order is mandatory.

**1.3 Direction of causation.** Output side is the driver. Downstream requests
output N; *we* then request the source frame(s) we need. Demand flows
output → us → source. The input clip is passive; we pull from it. (This is why
VS-LIFECYCLE-01 — only retrieve in arAllFramesReady what was requested in
arInitial — is a contract with the engine's *source* handling.)

**1.4 We never request output frames from VapourSynth.** Outputs exist only
when we compute them. The only source of output N-1 is our own cache or our own
recomputation. This is the entire reason the output cache must exist.

**1.5 Two caches.** Our cache holds references to *output* frames; VapourSynth's
own upstream cache holds *source* frames (if at all). Different mechanisms,
different owners. VS-LIFECYCLE-01 lives on the seam between them.

**1.6 Frame-count / index alignment.** CNR3 is 1:1 (one source in, one output
out, no rate change), so output N's primary source is source N. This is
filter-specific, NOT a VapourSynth guarantee. Settled for CNR3; labelled as a
filter-specific assumption.

---

## 2. Position B mandatory (settled — the central decision)

**2.1 Decision.** Position B (slot/frame pairing → complete liveness index) is
the **mandatory adopted architecture**. Position A (held-ref-only) is the proven
correctness floor and may be used in a *specific, named* circumstance ONLY where
B is demonstrated unnecessary or unworkable there, and ONLY by explicit per-case
agreement, documented with reason and scope. **The default is B; A is the
documented exception, never the silent fallback. A B/A mix is explicitly
rejected as worst-of-both.**

**2.2 Justification (recorded as goal-driven + conjectural + KISS).**
- Goal-driven: fmParallel is the final operational target; nothing may preclude
  it. Under fmParallel, predecessors are the most-shared frames (every request N
  wants N-1; N+1 wants N), so "alive-but-unfindable" becomes a frequent recompute
  tax, not a rare edge.
- Conjectural on the *performance margin*: the size of B's benefit is unmeasurable
  until testing (linear pipe-to-ffmpeg workload may keep the margin modest;
  B's case rests on the tail — stalls, scheduling skew). Adoption is therefore
  goal-driven-and-conjectural, NOT measured. If instrumentation later shows the
  tail negligible, revisiting B→A is a legitimate future move, not heresy.
- KISS / maintainability (decisive, and holds even if the perf margin proves
  small): B is ONE mental model (cache = source of truth about liveness; slot
  and frame are one managed unit). A forces every contributor to carry the
  decoupling (frame can be alive without a slot) forever — a standing
  comprehension tax that erodes and reintroduces bugs. B makes the careful thing
  the natural thing; A's correctness depends on distributed discipline that
  humans and AIs reliably fail at.

**2.3 Completeness = searchability (for our goals).** A frame alive-but-unfindable
is, for the cache's purpose, identical to a frame that doesn't exist (recompute
either way). So an incomplete liveness index IS a search failure. Held-ref-only
violates this; B restores it.

---

## 3. The pinning scheme (settled)

**3.1 Consumer-pins principle.** A pin represents a consumer's *active, in-flight
need*. The consumer takes the pin; the consumer releases it. **Production never
pins** (producing output N is not consuming it; nobody is consuming N yet — its
future consumer N+1 will pin it when it arrives). No pin without a present
consumer. Pin ownership is therefore unambiguous and self-balancing per request:
the thread that pinned is the thread that unpins, within its own
arInitial → arAllFramesReady lifetime.

**3.2 No pre-anticipation pinning.** A speculative pin by a producer on behalf of
a future, possibly-never-arriving consumer creates ambiguous ownership (who
unpins if the consumer never comes?). Rejected. The anticipatory case is the
hot zone's job (Section 5), not a pin's.

**3.3 Pin-and-record inside the atomic.** Every pin taken is appended to the
request's private per-invocation `frameData` pin-list, *inside the same atomic
as the pin itself* (pin and record are indivisible — cannot pin without
recording, cannot record without pinning). The pin-list is private per-request →
no cross-thread contention on it.

**3.4 Final unpin.** At end of arAllFramesReady, unpin the entire recorded
pin-list in ONE short atomic (count decrements, marshalled inside the lock —
fast in-memory work). Hold-to-end, uniform, once. Mid-walk unpinning is a
known-available future optimisation, deliberately NOT used (KISS/safety: avoids
variable intra-request pin lifetimes in a multithreaded environment).

**3.5 Leak-safety.** frameData cleanup/destructor must unpin any pins still on
the list on EVERY exit path including errors — extends the CMS06.11
single-ownership/null-on-consume RC discipline from a single ref to the
pin-list. Makes leaked pins structurally hard.

**3.6 Pinning is single-meaning.** "Pin" = consumer-claim only. There is NO
"policy-pin." (Earlier in-session framing of checkpoints as long-lived
"policy-pins" was CORRECTED — see Section 6. Checkpoints are eviction-protected
by a separate flag + rule, not by a pin.)

---

## 4. arInitial / arAllFramesReady walkthrough with atomic scope (settled)

**4.1 Combined principle:** *pin-and-record the plan INSIDE the lock; execute the
slow parts (compute, source-requests) OUTSIDE it. The inside-lock pinning is what
makes outside-lock execution safe.* The pin is the commit step and MUST share the
lock with the search — non-negotiable.

**4.2 arInitial (one atomic + slow work outside):**
- INSIDE one atomic (cache lock): search the window [anchor … N-1]; pin each
  *present cached output* (including the anchor frame itself); catalogue the
  *output holes* (missing cached outputs); record all pins to frameData pin-list.
  These are one indivisible operation.
- OUTSIDE the atomic: request from VapourSynth the *source frames* needed to fill
  the output holes (and source N). Slow (may trigger upstream decode) — must NOT
  be inside the cache lock.
- Safe because: the present frames in the plan are pinned (protected); the holes
  are absent (cannot be pruned — nothing there to prune). The plan snapshot stays
  valid after lock release.

**4.3 arAllFramesReady (compute outside, per-frame store-pin atomic, end-unpin):**
- Requested sources have arrived; callback fires. The WHOLE arAllFramesReady
  activation is the single overarching consumer for the hole-filling pass (NOT a
  nested sequence of sub-consumers).
- For each output hole: compute the output OUTSIDE the lock (slow pixel work),
  then a brief per-frame atomic { first-in-best-dressed check; store; pin;
  record }, then unlock; proceed to next hole.
- Store-and-pin must be in the SAME atomic (if pinned only on next-consume, a gap
  exists where another thread could prune the just-stored frame).
- Compute output N from the now-present, pinned predecessor chain; return N to VS.
- Final unpin of the whole pin-list in one short atomic at end.

**4.4 First-in-best-dressed — BOTH branches have a mandatory ref action:**
- Winner: store-and-pin-and-record (inside atomic).
- Loser: must `freeFrame()` its computed-but-unstored duplicate before discarding
  — NO dropping the pointer (else leak of a never-pinned frame). Wasted compute is
  acceptable (`duplicate_store_computed_but_discarded` counter); the frame ref
  must still be freed.

**4.5 Terminology nailed (to stop one word hiding two things):**
- *unlock* (release the cache lock) ≠ *freeFrame* (release a frame ref).
- *source hole* (missing source → request from VS) ≠ *output hole* (missing cached
  output → recompute).
- *pin* (consumer-need, inside lock) ≠ *hot zone* (anticipated-imminent residency,
  heuristic) ≠ *checkpoint flag* (recovery-anchor retention).

---

## 5. The new hot-zone scheme: decay-hints over a pinned liveness floor (settled)

**5.1 OLD scheme superseded and marked dangerous under the B goal.** The old
hot-zone scheme made the zone the *findability guarantee for active frames* (via
held-refs + rolling-predecessor pattern), with pinning as deferred emergency. This
lets active frames be pruned-then-recovered rather than pinned — the completeness
violation B rejects. Retire/supersede its ROLE. (Bring across the slide/spawn/
merge *machinery* for reuse; revalidate.)

**5.2 Hot zones have a job pins CANNOT cover.** Pins protect the *active* set
(present consumer). The zone protects the *anticipatory / decaying* set
(soon-needed, or recently-needed-but-not-yet-cold) — frames NO current request has
claimed, so a pin-only scheme would prune them the instant the last consumer
releases, even though a new request is about to want them. Includes the
store→next-claim handoff AND the dwindling-zone graceful-decay window. This is
orthogonal to pinning, not a substitute for it. (CORRECTS an in-session
mis-statement that called the zone "a substitute for pinning.")

**5.3 B de-risks the zone scheme rather than replacing it.** Under B, the active
set is guaranteed by pins independently of zones. So the zone is RELIEVED of
correctness duty: its failures (premature prune, merge-ratchet, zone-limit
churn) cost EFFICIENCY (a recompute), never CORRECTNESS. This removes the old
scheme's soft spots as *dangers* (they become harmless inefficiencies).

**5.4 New scheme — zones as pure prune-policy hints:**
- A zone is a `[low, high]` window tracking where recent activity has been
  (carries `last_observed_frame`).
- Frames inside a live zone are prune-DEFERRED (anticipatory). Frames outside all
  zones are prune-ELIGIBLE. "Prune-deferred" is now a *preference*, not a
  guarantee (correctness is the pin floor).
- Slide / spawn / merge machinery retained from old scheme (revalidate on
  bring-across).

**5.5 Decay sequence (answers "make dwindling-zones safe, prune after criteria"):**
active (pins in range OR recently observed) → dwindling (no new observation, pins
clearing) → retire-eligible (NO pins in range AND decay-margin frames elapsed
since `last_observed_frame`) → retired (anticipatory protection withdrawn) →
frames become prune-eligible → pruned by capacity pressure, furthest-from-zone
first. Safe at every step because pins underwrite correctness throughout.

**5.6 Retirement test is now EXACT and cheap (kills the merge-ratchet soft spot).**
Old scheme used a conservative proxy ("no pinned checkpoint in range") that
couldn't retire a zone holding a long-lived checkpoint → forced merges → wider
zones → protected more → feedback ratchet. New test: "no pins in range" is exact,
cheap, and rides the pin state already maintained — answerable in the same locked
prune pass. Checkpoints do NOT keep a zone alive (they have their own separate
retention rule, Section 6). PIGGYBACK CONFIRMED: zone-retirement needs no
parallel `active_request_count` counter — the pin-count IS the activity record.

**5.7 Decay margin = the one new tuning knob.** Meaning: how long after activity
leaves a region we keep anticipatorily protecting it. Pure efficiency dial, no
correctness role → tunable by measurement. Small = aggressive prune, more
straggler recomputes; large = more memory, fewer recomputes. Sizing belongs to
parameter-coherence (Section 9, OPEN).

**5.8 Composite eviction predicate (read atomically under one lock):**
`evict iff pin_count == 0 AND outside-all-live-zones AND (checkpoint ?
checkpoint-retention-permits : capacity-permits)`, with furthest-from-zone-boundary
evicted first. Zone retirement just flips a zone inactive so its range stops
contributing to "outside-all-live-zones."

---

## 6. Checkpoints (settled model; numbers VERIFY)

**6.1 Checkpoint = a FLAG with its own eviction rule, NOT a pin.** (CORRECTS the
in-session "policy-pin" framing.) "Pin" keeps exactly one meaning (consumer-claim).
A checkpoint with no active consumer is NOT pinned — it is eviction-protected by
a separate flag + retention rule. Three independent eviction protections, each
meaning one thing: pin-count>0 (active need), checkpoint-flag (recovery policy),
hot-zone (anticipated). None of the latter two is a pin.

**6.2 Hard floors apply to both classes.** A pinned frame is NEVER evicted
(checkpoint or not); a hot-zone frame is spared. Checkpoint-ness never overrides a
pin; a pin never overrides checkpoint protection. ANDed.

**6.3 Current spec rules (READ FROM CMS06.11 — sourced, not reconstructed):**
- Establishment: `CHECKPOINT_INTERVAL = 10` — promote every 10th frame + frame 0.
- Retention: count-based soft trigger — prune *runs* when pool exceeds
  `MAX_RETAIN = 32`, prunes back toward `MIN_RETAIN = 10`. A checkpoint is a prune
  candidate iff frame ≠ 0 AND pin_count == 0 AND outside every hot zone. Evict
  greatest-hot-zone-distance first. Retain limits are SOFT triggers (a hot-zone or
  pinned checkpoint is retained past MAX_RETAIN). Frame 0 never pruned.
- "Kept longer but not excessively": pool normally lives 10–32 checkpoints; at
  INTERVAL=10 that's ~100–320 frames of backward checkpoint history. Bounded by
  count, does NOT grow with clip length.
- Count vs distance: equivalent under steady linear advance; DIVERGENT under
  fmParallel scatter (distance needs a well-defined "front"; under scatter the
  front is ill-defined). The spec's choice is the count-trigger + hot-zone-distance
  ordering above. Must not lean on the equivalence under the fmParallel goal.

**6.4 The current spec's eviction predicate ALREADY matches the model re-derived
this session** (pin + hot-zone hard floors; class only changes the count trigger).
Reassuring: the existing design is self-consistent under the new pin reasoning.

**6.5 Note on bring-across:** CMS07.0 must keep checkpoint establishment and
retention as DISTINCT rules (how often laid down ≠ how long kept).

---

## 7. Q1 / Q2 (closed)

- **Q1 (pin-eligible always vs only-when-claimed): CLOSED →** pinned iff a consumer
  has claimed it. NOT pinned by mere residency (residency gives only hot-zone
  heuristic protection). Holes are pin-INELIGIBLE (nothing to pin). The "always"
  option (resident = pinned) is REJECTED — protection is by need (consumer) or
  recovery role (checkpoint flag), never by mere residence.
- **Q2 (does checkpoint-pin generalise / is it a distinct lifetime): CLOSED →**
  there is no checkpoint-PIN. Checkpoints unify with ordinary frames at the
  EVICTION-PREDICATE level (one predicate, three independent protection inputs),
  NOT at the pin level. One pin concept; one eviction predicate; a separate
  checkpoint retention rule. (This SUPERSEDES the earlier in-session "unify as
  long-lived pin-policy" answer, which was the wrong level of unification.)

---

## 8. Locking / atomicity — Item 6 CLOSED

**8.1 Cost relocation, not elimination.** The hard problem did not vanish; it
RELOCATED from "distributed per-ref/per-pin discipline" to "centralised,
well-scoped critical sections." That relocation is the KISS win (difficulty in
named places, not scattered invariants). The difficulty concentrates in the
locking design — now resolved below.

**8.2 The TOCTOU edge case that started the session is STRUCTURALLY CLOSED** by
pin-at-arInitial: the request's need is physically present as a pin from before
eviction could see the request until after the request is done. Check and claim
are the same atomic act. No interval of "needs-but-hasn't-claimed."

**8.3 DECISION — one cache-wide lock, held minimally.** A single mutex guards all
cache state (slot index, zones, checkpoint pool, pin-counts).
- Rationale: correct-by-construction; the "single consistent snapshot" property
  (which region-scoped search-and-pin and the composite eviction predicate both
  depend on) is FREE under one lock; no lock-ordering, no deadlock; ONE mental
  model — matches every prior decision this session.
- Discipline: "do ALL the right process things inside the lock and no more —
  without being stingy." Expensive work goes OUTSIDE the lock when not needed
  inside it.
- Fine-grained / per-region locks: REJECTED as baseline. Cross-region locks
  reintroduce the "separated things humans fail at" problem at the locking layer,
  threaten the snapshot property the pinning scheme depends on, and add deadlock
  risk. Deferred to a MEASURED optimisation applied ONLY if instrumentation proves
  the single lock is the actual bottleneck. Fine-graining attacks a load-bearing
  assumption → requires strong evidence to adopt.

**8.4 Inside vs outside the lock.**
- INSIDE (in-memory, indivisible commits): search; pin; hole-catalogue; store;
  pin-record; final unpin; eviction-predicate read; prune decision + slot detach.
- OUTSIDE (slow work, never serialises threads through the cache mutex): pixel
  computation; VapourSynth source requests; `freeFrame` of evicted/duplicate
  frame refs.

**8.5 DECISION — bounded prune scheme (refines the spec's existing trigger;
composes Dave's 1a/1b/1c).** Three conceptual acts:
- (a) DECIDE: under the lock, evaluate the composite eviction predicate (5.8),
  select up to K victims (greatest-distance-from-zone first).
- (b) DETACH: under the SAME lock, detach each selected slot from the cache index,
  collecting its `VSFrame*` ref into a local list. (a)+(b) are one critical
  section — so a pin CANNOT creep in between select and detach (the slot is gone
  from the index the moment selected; and a pinned frame is never selected).
- (c) FREE: release the lock; then `freeFrame` the collected refs in one
  post-lock BATCH (outside the lock — freeFrame may trigger core deallocation).

**8.6 DECISION — prune trigger: frame-count overflow, self-debouncing (from spec).**
- Non-checkpoint prune fires when pool strictly exceeds capacity ×
  `OVERFLOW_FACTOR (1.1)`; prunes back toward capacity. The capacity→overflow gap
  IS the hysteresis (won't re-fire until the pool climbs back over threshold) — no
  percentage marks, no periodic timers, no "did I already fire?" state.
- REJECTED alternatives (debated): percentage marks (50/75/90%) — need manual
  hysteresis state, awkward; periodic every-X-frames — decouples pruning from
  actual pressure (prunes when not needed / misses bursts). Pressure-triggered
  count-overflow is simpler and self-debouncing.
- Steady linear flow ⇒ cache sits at capacity, each new frame nudges ~1 over ⇒ a
  steady TRICKLE of tiny ~1-frame prunes ⇒ tiny critical sections. Ideal for the
  single-lock model.
- Checkpoint prune: fires when checkpoint pool > MAX_RETAIN (32), prunes toward
  MIN_RETAIN (10) — same bounded discipline applies.

**8.7 DECISION — K bound (max victims per bounded-prune acquisition).** Caps the
lock-hold for BURST prunes (post-seek, cold-start catch-up); rarely binds in
steady trickle. **Starting point K = 8** (> per-frame trickle of 1 by a healthy
margin so normal operation never feels it; relates to FORWARD_RADIUS=10 / p99
jitter ~8; 8 in-memory detaches is a short critical section). If a burst needs
more, successive requests' prunes mop up. INSTRUMENTATION SIGNAL: add a counter
for "prune stopped at K with pool still over threshold"; if it fires regularly,
raise K; if prunes are always 1–2, K is irrelevant. Starting guess, refine by
measurement.

**8.8 Mutex-scoping audit (for the coder).** Enumerate every cache operation that
checks-then-acts and confirm none drops the lock between check and act under
fmParallel. Designer to FRAME for the coder as "what level of atomicity does each
scenario need," covering at least: search-identify-and-pin; store-on-compute-
complete (+ first-in-best-dressed); bounded prune (decide+detach / free);
hot-zone slide; zone retirement; checkpoint establish; final unpin — PLUS a broad
open ask for the coder to identify scenarios the designer has NOT enumerated,
given the redefined caching goal. (Under one cache-wide lock most check-then-act
races vanish by construction; the audit confirms no operation accidentally splits
its check and act across two lock acquisitions.)

---

## 9. Still-parked / OPEN items

1. **Item 6 — locking granularity: CLOSED** (Section 8). One cache-wide lock held
   minimally; bounded prune (K=8 start); fine-grain deferred to measured. Done.
2. **Item 3 — hot-zone sizing: CLOSED** (Section 9B). decay_margin=20; zones=5;
   sizing relationships in 9B.
3. **Item 7 — parameter coherence: CLOSED** (Section 9B). Five coherence rules
   recorded; existing constants kept (already coherent).

**Remaining = no open DESIGN items.** Only VERIFY items (Section 10) and the two
doc-updates (Section 11) remain — confirm-and-write, not design decisions.

### 9B. Parameter coherence (Items 3+7 CLOSED)

**Decision: keep all existing CMS06.11 sizing constants unchanged** — they already
satisfy every coherence rule below with healthy margins. The deliverable is making
the coherence EXPLICIT (rules + the new decay_margin), not re-tuning.

**The five coherence rules (CMS07.0 requirement: coder codifies each as a comment
directly above where the constant is defined in code, so the constraint can't be
missed on edit):**

- **CR1 — JUMP_THRESHOLD is DERIVED, never set independently:**
  `JUMP_THRESHOLD = FORWARD_RADIUS + BACK_RADIUS + 1` (=61). Ensures a slide
  reaches exactly to where a zone already protects.
- **CR2 — BACK_RADIUS ≥ bounded recovery search window B; ideally =B.** A recovery
  walk searches `[N-B, N]` for an anchor; the zone must protect that whole reach.
  Currently BACK_RADIUS=50 and B=50 coincide. ✓
- **CR3 — BACK_RADIUS ≈ 5 × CHECKPOINT_INTERVAL** so a zone always covers ~5
  recovery anchors. 50 = 5×10. ✓
- **CR4 — active_ceiling ≥ ~2× max-protected set.** Max protected ≈
  MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool
  = 5×60 + ~32 = ~332. active_ceiling=1000 ≫ 332 (≈3× headroom). ✓ If pruning can
  never reach target, this rule is violated (cache jams).
- **CR5 — CHECKPOINT_MAX_RETAIN ≥ MAX_HOT_ZONES × (BACK_RADIUS / INTERVAL).**
  = 5×5 = 25; MAX_RETAIN=32 > 25. ✓ (matches spec's own arithmetic note).

**decay_margin (new constant, Item 3) = 20 frames** (≈2×FORWARD_RADIUS,
≈2×CHECKPOINT_INTERVAL). After activity leaves a zone's region, frames before the
zone becomes retire-eligible (Section 5.5). Pure efficiency dial, NO correctness
role (pins protect anything actually needed) → instrumentation-tunable. Coherence:
**FORWARD_RADIUS ≤ decay_margin ≤ BACK_RADIUS** (must outlast p99 jitter ~8 so a
momentarily-quiet zone isn't retired; must not exceed the back-region that would
be pruned anyway). 10 ≤ 20 ≤ 50 ✓. Comfortably far below active_ceiling=1000.

**MAX_HOT_ZONES = 5: keep, with coherence note** — this scales with the number of
concurrent DISTINCT access regions, NOT with thread count. Linear pipe-to-ffmpeg
(dominant workload) uses ~1 zone regardless of thread count → 5 ample. Revisit ONLY
if a scatter workload appears; raising it pushes the CR4 max-protected set, so
active_ceiling may need to follow.

**Instrumentation discipline (restated, applies everywhere):** a counter BUMP
(single int increment) may occur inside a lock; FORMATTING and EMISSION (sprintf,
string building, file/stderr writes) MUST be outside the lock unless there is a
specific justified reason. Emitting inside a critical section extends it and
pollutes contention measurement. Snapshot under lock if needed; format/print
outside.

### 9A. Sizing starting-points settled this session (refine by instrumentation)

- **K (bounded-prune victims/acquisition) = 8.** Starting guess (Section 8.7).
- **active_ceiling = 1000 frames** starting point for 8-bit 4:2:0 PAL 720×576
  (≈0.59 MiB/frame ⇒ ~590 MiB at ceiling). At 8-bit the 1 GiB byte budget is
  NON-BINDING (derives ~1725 frames → clamped to MAX_HARD_CEILING=1000); the
  frame-count hard ceiling dominates. RECOMPUTE for higher bit-depth: 16-bit 4:2:0
  ≈1.19 MiB/frame → ~860 frames (byte-budget binds, below ceiling); 16-bit 4:2:2
  ≈1.58 MiB → ~648 frames. (Dave's earlier "~1.5 GB" recollection was high; 8-bit
  PAL at 1000 frames ≈ 0.6 GB.) **Format-dependent → see V8.**
- Spec sizing constants carried forward (CMS06.11): OVERFLOW_FACTOR=1.1;
  MIN_HARD_CEILING=150; MAX_HARD_CEILING=1000; CHECKPOINT_INTERVAL=10,
  MAX_RETAIN=32, MIN_RETAIN=10; HOT_ZONE_FORWARD_RADIUS=10, BACK_RADIUS=50,
  MAX_HOT_ZONES=5, JUMP_THRESHOLD=61. All subject to parameter-coherence (item 7).

---

## 10. VERIFY items (read from record / confirm with coder — do NOT reconstruct)

- **V1 — CNR2 temporal reach:** is the source window genuinely backward-only
  `[N-B, N]`, or does CNR2's noise estimate peek forward? (Working assumption:
  backward-only. Confirm against the actual algorithm.)
- **V2 — checkpoint retention metric/params:** confirmed from CMS06.11 as
  INTERVAL=10, MAX_RETAIN=32, MIN_RETAIN=10 (Section 6.3). Re-confirm these are
  still the intended values for the new architecture.
- **V3 — checkpoint establishment rule:** every-10th + frame 0 (confirmed CMS06.11);
  confirm still intended.
- **V4 — R76 API4 frameData carry mechanism:** exact spelling/mechanism by which
  per-request state is carried between arInitial and arAllFramesReady; whether the
  core offers a cleanup hook or it's pure manual free; that destruction discharges
  pin-list refs BEFORE freeing the struct on every path.
- **V5 — addFrameRef/freeFrame atomicity on R76 API4:** CONFIRMED thread-safe and
  atomic (std::atomic refcount) via external check. Re-confirm against actual R76
  headers before it goes into the spec as settled (load-bearing).
- **V6 — VapourSynth upstream source-cache policy on R76:** how much/under what
  policy the engine caches our upstream sources (affects cost of conservative
  over-fetching). Lower priority.
- **V7 — the original "unfortunate results" without hot zones:** read the specific
  failure mode from the prior record/decision log that originally motivated zones,
  so the new scheme provably addresses THAT failure, not a reconstruction.
- **V8 — CNR3 operating pixel format (bit depth + subsampling):** determines
  frame byte-size and therefore whether active_ceiling is byte-budget-bound or
  hits the 1000-frame hard ceiling (Section 9A). Working assumption 8-bit 4:2:0
  PAL 720×576 → ceiling binds at 1000 (~590 MiB). Confirm the actual format; if
  10/16-bit, recompute the derived ceiling.

---

## 11. CMS07.0 production plan (for "when the time comes" — NOT YET)

**Trigger:** all of Section 9 settled (esp. item 6 locking).

**REMINDER TO DAVE at CMS07.0 time:** upload the handover pack specification `.md`
and the latest handover documents (A/B/C) `.md`. They are superseded/obsolete but
may contain material usable for the CMS07.0 introduction and the completeness
audit.

**Structure:**
1. Strong introduction (state the architecture cleanly on its own terms).
2. New pinning scheme in full detail — with EXPLICIT atomic scope (what's inside
   the lock, what's outside, why) for arInitial and arAllFramesReady.
3. New zone scheme in full detail — decay-hints; slide/spawn/merge; decay sequence;
   exact pin-absence retirement; composite eviction predicate; atomic scope explicit.
4. Checkpoints — flag + separate retention rule (distinct establishment vs
   retention), with verified constants.
5. Bring-across-and-revalidate (item 4): carry over every still-relevant section
   from the superseded design (VS-LIFECYCLE-01, API4 lifecycle constraints,
   recovery/fallback H16.3/H16.4, bounded-window source logic, RC discipline,
   counters, etc.) and DOUBLE-CHECK each against the new design for completeness —
   supersession WITH a completeness audit, never a rewrite-from-blank.
6. Body = new design, self-contained, codeable, crystal-clear for coder/human.
   Appendix / decision-log = supersession rationale + rejected old approaches
   (so not silently lost, not accidentally resurrected). Body should NOT require
   the old scheme as context.
7. END: completeness double-check — nothing missing, everything precise and
   complete. Changelog at top; self-review with grep checks.

**Also outstanding (independent of CMS07.0 mechanics):**
- CMS must state the fmParallel final-goal invariant OUTRIGHT (was at risk of
  living only in handover, not the controlling spec). Put near the top of CMS07.0.
- Handover pack (Docs A/B/C) bump to the new controlling spec — open since before
  this session.
- CMS07.0 must require the coder to codify the five coherence rules (CR1–CR5) and
  the decay_margin bounds as comments directly above each constant's definition in
  code (Section 9B).
- CMS07.0 must state the instrumentation discipline (Section 9B): counter bumps may
  be inside locks; formatting/emission outside.
