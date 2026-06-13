# CNR3 Cache Manager Design Specification

**Date:** 2026-06-12
**Version:** CMS07.0
**Status:** Design specification — architectural supersession. COMPLETE: all verify
items V1–V8 resolved (several against authoritative sources: the CNR2 reference code,
the local R76 VapourSynth4.h header); section-level bring-across audit of the
CMS06.11 body done (§9A); final self-review pass done. Controlling design authority
for the CNR3 restart; ready for coder study and the isolated cache-core milestone.
**Supersedes:** CMS07.0 supersedes CMS06.11 and ALL earlier CMS06.x / CMS0x cache
designs. This is an architectural supersession, not a refinement: the pinning model
and the hot-zone model both change fundamentally. Earlier documents are reference
material only.

---

## 0. How to read this document (and what changed)

CMS07.0 replaces the previous cache architecture. The two headline changes:

1. **Pinning is now the mandatory correctness mechanism.** In the old design
   (CMS06.11 and earlier), actively-needed frames were kept findable by a
   combination of held references plus hot-zone heuristics, and non-checkpoint
   pinning was a *deferred emergency escalation*. That is superseded. In CMS07.0,
   any frame a request actively needs is **pinned** by that request (a
   consumer-held pin), and the cache is the **complete liveness index** for the
   active set.

2. **Hot zones are demoted to prune-policy hints.** They no longer guarantee
   findability of active frames (pins do that). They protect the *anticipatory /
   decaying* set — frames likely-to-be-needed-soon or recently-needed-but-not-yet-
   cold — which pins structurally cannot protect (a pin needs a present consumer).

**You must not conflate the old and new concepts.** The three highest-risk traps:
- treating pinning as optional/deferred — it is now baseline;
- reintroducing held-ref-only predecessor reservation — superseded by consumer-pins;
- thinking of a checkpoint as a kind of pin — a checkpoint is a separate
  eviction-protection FLAG with its own retention rule; there is exactly ONE pin
  concept (consumer-claim).

**Design vs code:** CMS07.0 is a design supersession. The existing CODE is at the
prior (CMS06.11 / sequential-fast-path-through-H15.5) state. Development restarts
against this spec; old source is reference material for verifiable salvage only.

---

## 1. Project context (orientation — adapted from handover Document A §A2/§A3/§A4)

CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma
stabiliser, inspired by the CNR2/vscnr2 algorithm, intended for analogue / video-
capture material (chroma shimmer, temporal chroma noise). The final operational
target is **fmParallel** (Section 2).

**The load-bearing fact:** `output[N]` depends on `source[N]` and on the
*already-filtered* `output[N-1]` — not on `source[N-1]`. This recursion is why a
naive stateless filter cannot work.

**Algorithmic core (§A4 — V1 RESOLVED: confirmed causal by Dave against CNR2):**
- For luma Y: copy source luma unchanged.
- For chroma U/V: compare current source chroma vs previous *filtered* chroma, and
  current downsampled luma vs previous downsampled luma; use signed-difference
  response tables; blend
  `output_chroma[N] = blend(output_chroma[N-1], source_chroma[N])` by the combined
  response.
- **Scene-change (cut) detection** runs *during compute* as part of comparing frame
  N to its predecessor. On a true cut at N, output[N] is computed as a **fresh start**
  (copy source chroma, skip the recursive blend), so output[N] does NOT depend on
  output[N-1]. A cut therefore SEVERS the recursive chain at N. (This is a third
  reset point alongside frame 0 and the lower-bound floor; see Section 9.5/9.6.)
- The algorithm is **purely causal**: computing output N uses only the current
  source (N) and the previous filtered output (N-1). There is NO forward-source peek
  / look-ahead / bilateral reach. The source reach to compute N is therefore
  **backward-only**. (V1 confirmed against CNR2 itself; corroborated by §A4, by the
  proven request window in CMS06.11, and by the absence of any forward reference.)

**Why the cache exists (correctness, not performance):** a modern VapourSynth graph
may request output frames out of display order (e.g. 0, 3, 1, 2, 4). To compute
output 3 the filter needs filtered output 2, which may not yet exist. The cache
retains computed outputs so the recursion can find predecessors (or recover them)
rather than rebuilding the chain from frame 0 on every request. **The cache is a
correctness subsystem.** (The reference CNR2 — https://github.com/Asd-g/AviSynth-vsCnr2
— runs serialized and approximates a missing predecessor with the previous *source*
frame; CNR3 abandons serialization for fmParallel and replaces that approximation with
exact output recovery. See §12B for this root rationale and §13 V8.1 for pixel-layer
salvage guidance.)

---

## 2. Operational goal and the fmParallel invariant (stated outright — from §A8)

```text
safe under fmUnordered now
structurally compatible with fmParallelRequests
final design target: fmParallel
```

**Invariant (top-level, binding):** fmParallel is the final operational target.
Interim development may pass through fmUnordered and fmParallelRequests, but design
and coding MUST NOT make choices that block eventual safe fmParallel operation
unless explicitly justified and recorded. This invariant is the justification for
adopting Position B (Section 3) over the simpler held-ref-only model.

`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are NOT final
fmParallel output authority and must be retired/bypassed/redesigned before final
authority (carried forward from decisions D26/D33). The restart is the occasion to
retire the old strict-streaming bridge.

---

## 3. Foundational model — the cache holds references, Position B mandatory

**3.1 The cache holds references, not pixels.** A cache slot holds an
`addFrameRef` reference to a VapourSynth-core-owned `VSFrame`; it does not own
pixel memory. The core reclaims a frame when its atomic reference count reaches
zero. `addFrameRef`/`freeFrame` are declared in the R76 header (VapourSynth4.h lines
377–378). The core's *internal frame refcount* is atomic (V5) — but see the V5
firewall in Section 8.6: that internal atomicity does NOT license shrinking any
designer-mandated cache-lock scope.

**3.2 Two leak surfaces — WE manage allocation and cleanup (no coder discretion).**
Confirmed against the actual R76 header (VapourSynth4.h):
- (a) The per-request `frameData` struct — `VSFilterGetFrame(..., void **frameData,
  ...)` (line 337). It defaults to NULL and is **ours to allocate and free**; the API
  contract requires it be deallocated before the last call for the frame
  (arAllFramesReady or error), and `setFilterError` (line 409) marks the error path.
  The core carries the *pointer* between activations but never owns the struct. The
  API SANCTIONS and REQUIRES manual management — there is no core cleanup hook for it.
  Therefore CMS07.0 mandates our specified allocate-in-arInitial / free-before-last-
  call / free-on-error discipline; the coder IMPLEMENTS it and has NO discretion to
  change whether or how we manage it (V4 RESOLVED).
- (b) `VSFrame` references — core-owned memory; our obligation is ref discipline
  (`freeFrame` every ref we take), never deallocation.
- **Destruction ordering (mandatory):** on `frameData` destruction, first discharge
  every ref the struct tracks (walk the pin-list; unpin / `freeFrame`), THEN free
  the struct. Freeing the struct first loses the owed-ref list → leak.
- **Concurrency guarantee (from the header, strengthens our design):** `getFrame` is
  never called concurrently for the same frame number; for fmParallel-class modes it
  may be called by multiple threads with arInitial but only one thread calls
  arAllFramesReady at a time. Therefore a request's per-invocation `frameData`
  (and its pin-list) is never touched by two threads for the same N — the pin-list's
  "private per request, contention-free" property (Section 4.3) is GUARANTEED by the
  API, not merely our convention.
- **Note — we deliberately do NOT use the core's `cacheFrame`/`setLinearFilter` API
  (VapourSynth4.h lines 362, 408).** That is an opaque core convenience cache for
  linear filters; it offers no consumer-pins, no checkpoint/zone control, and
  constrains the access model via `setLinearFilter`. It cannot provide the
  complete-liveness-index correctness guarantee our recursion requires. Our own cache
  manager is therefore necessary, not a reinvention. (Recorded so the choice is
  explicit, not an apparent oversight.)

**3.3 Position B — mandatory architecture.** Slot and frame operate as a managed
unit; the cache is the complete liveness index for the active set. A frame that is
alive-but-unfindable is, for the cache's purpose, equivalent to one that does not
exist (recompute either way) — so completeness of the index IS searchability for
our goals.
- **Position A (held-ref-only)** is the proven correctness floor and may be used
  ONLY in a specific, named circumstance where B is demonstrated unnecessary or
  unworkable, and ONLY by explicit per-case agreement, documented. The default is
  B; A is the documented exception, never a silent fallback. A B/A mix is rejected.
- **Justification:** (i) goal-driven — under fmParallel, predecessors are the most-
  shared frames, so alive-but-unfindable becomes a frequent recompute tax; (ii)
  conjectural on the performance margin (unmeasurable until testing — instrument and
  confirm); (iii) KISS / maintainability — B is ONE mental model (cache = liveness
  truth; slot+frame are one unit), whereas A forces every contributor to carry the
  decoupling forever, a comprehension tax that erodes and reintroduces bugs. B holds
  even if the performance margin proves small.
- **This supersedes decision D13** ("non-checkpoint pinning deferred"). See §12.

**3.4 Direction of causation.** The output side drives: downstream requests output
N; the filter then requests the source frame(s) it needs. Demand flows
output → filter → source. The input clip is passive. VS-LIFECYCLE-01 (only retrieve
in arAllFramesReady what was requested in arInitial) is a contract with the engine's
source handling. The filter NEVER requests its own output frames from VapourSynth —
outputs exist only when computed; the cache is their sole retainer.

**3.5 Per-instance, per-invocation (D03/D16).** The cache is per filter instance,
never global. Per-request planning/ownership state (source plan, pin-list) is
carried in per-invocation `frameData`, never in shared `Cnr3Data` state.

---

## 4. Pinning scheme (the correctness mechanism)

**4.1 Consumer-pins principle.** A pin represents a consumer's active, in-flight
need. The consumer takes the pin; the consumer releases it. **Production never
pins** (producing output N is not consuming it; nobody consumes N yet — its future
consumer N+1 will pin it when it arrives). No pin without a present consumer. Pin
ownership is unambiguous and self-balancing per request: the thread that pinned
unpins, within its own arInitial → arAllFramesReady lifetime.

**4.2 No pre-anticipation pinning.** A speculative pin by a producer on behalf of a
future, possibly-never-arriving consumer creates ambiguous ownership. Rejected. The
anticipatory case is the hot zone's job (Section 5), not a pin's.

**4.3 Pin-and-record inside the atomic.** Every pin taken is appended to the
request's private per-invocation `frameData` pin-list, INSIDE the same atomic as the
pin (pin and record are indivisible). The pin-list is private per request → no
cross-thread contention on it.

**4.4 Single-ownership / null-on-consume.** A reserved/owned ref or pin entry is
owned by `frameData` until consumed:
- successful consumption releases (or transfers) the ref exactly once, then sets the
  field/entry to nullptr/consumed;
- cleanup releases an entry only if it is still non-null;
- the null field is the idempotency guard; exactly one of {consume, cleanup}
  releases any given entry;
- consume and cleanup attribute counters correctly (cleanup increments the cleanup
  counter; consume increments the consume counter — never both for one entry).

**4.5 Final unpin.** At end of arAllFramesReady, unpin the entire recorded pin-list
in ONE short atomic (count decrements, marshalled inside the lock). Hold-to-end,
uniform, once. Mid-walk unpinning is a known-available future optimisation,
deliberately NOT used (avoids variable intra-request pin lifetimes).

**4.6 Leak-safety.** `frameData` cleanup/destructor must unpin any pins still on the
list on EVERY exit path including errors. Makes leaked pins structurally hard.

**4.7 One pin concept.** "Pin" = consumer-claim only. There is no "policy-pin." (See
Section 6 for checkpoints, which are a separate flag, not a pin.)

**4.8 Ownership accounting (carried forward from D05/D06/D55).** Pin/owned refs
participate in the existing lookup-ref accounting:
`lookup_owned_ref_acquired_total == released_total + transferred_total`. A consumed
predecessor/intermediate is an INPUT → **released, not transferred**; only the
returned output frame N is transferred to VapourSynth.

---

## 5. Hot-zone scheme — decay-hints over the pinned liveness floor

**5.1 Role (demoted from the old design).** Hot zones do NOT guarantee findability
of active frames — pins do. Zones protect the **anticipatory / decaying** set:
frames no current request has claimed but which are likely-needed-soon (the
store→next-claim handoff) or recently-needed-but-not-yet-cold (the dwindling-zone
graceful-decay window). This is orthogonal to pinning, not a substitute for it.

**5.2 B de-risks the zone scheme.** Because pins guarantee the active set
independently, the zone's failures (premature prune, merge churn, zone-limit churn)
cost EFFICIENCY (a recompute), never CORRECTNESS. The old scheme's soft spots become
harmless inefficiencies.

**5.3 Zone definition.** A zone is a `[low, high]` window tracking where recent
activity has been, carrying `last_observed_frame`. Frames inside a live zone are
prune-DEFERRED (a preference); frames outside all zones are prune-ELIGIBLE.

**5.4 Slide / spawn / merge (machinery carried forward from D08, revalidated).**
- Slide rule: for an arriving frame F, find the nearest active zone within
  `JUMP_THRESHOLD`; if found, slide it so `low = max(0, F - BACK_RADIUS)`,
  `high = F + FORWARD_RADIUS`, update `last_observed_frame`. Else it is a jump →
  allocate a new zone (or, if no free slot and none retire-eligible, merge the two
  closest zones conservatively: `min(lows), max(highs)`).
- Sliding is safe because correctness rests on pins, not on the zone covering the
  predecessor.

**5.5 Decay sequence (makes dwindling-zone pruning safe; prune after criteria):**
active (pins in range OR recently observed) → dwindling (no new observation, pins
clearing) → retire-eligible (NO pins in range AND `decay_margin` frames elapsed
since `last_observed_frame`) → retired (anticipatory protection withdrawn) → frames
prune-eligible → pruned by capacity pressure, furthest-from-zone first. Safe at every
step because pins underwrite correctness.

**5.6 Retirement test is EXACT and cheap.** "No pins in range" is answerable in the
same locked prune pass from the pin-count state already maintained — NO parallel
`active_request_count` counter needed. Checkpoints do NOT keep a zone alive (they
have their own separate retention rule). This eliminates the old conservative
"no pinned checkpoint" proxy and its merge-ratchet tendency.

**5.7 Hot-zone update at arInitial (carried forward from D15).** Zone activity is
registered at arInitial, not deferred to arAllFramesReady — required for safety once
multiple requests are in flight.

---

## 6. Checkpoints — a flag with its own retention rule (NOT a pin)

**6.1 A checkpoint is a FLAG, eviction-protected by a separate retention rule.** A
checkpoint with no active consumer is NOT pinned. The three independent eviction
protections, each meaning one thing: `pin_count > 0` (active consumer need),
checkpoint flag (recovery-anchor retention), hot-zone membership (anticipated need).
Only the first is a pin.

**6.2 Hard floors apply to both ordinary and checkpoint frames.** A pinned frame is
NEVER evicted; a hot-zone frame is spared. Checkpoint-ness never overrides a pin; a
pin never overrides checkpoint protection. ANDed.

**6.3 Establishment and retention (DISTINCT rules; V2/V3 confirmed):**
- Establishment: promote every `CHECKPOINT_INTERVAL`-th frame (and frame 0) to the
  checkpoint pool — the regular grid. **In addition, promote every detected
  scene-change (cut) frame to the checkpoint pool** (Section 6.4). The grid promotion
  is RETAINED alongside cut promotion (regular anchors for long static scenes + exact
  anchors at cuts).
- **Supplementary rule — checkpoints may be IRREGULARLY spaced** because of scene-cut
  promotion. This is safe: the checkpoint search is position-agnostic (it finds the
  greatest checkpoint frame ≤ the search bound via the ordered frame-number map, D04),
  making no assumption of regular spacing. The only place INTERVAL appears beyond
  establishment is the capacity sizing estimate (CR5), which becomes a density FLOOR,
  not a ceiling (cuts add density on top of the grid).
- Retention: count-based soft trigger — checkpoint prune RUNS when the pool exceeds
  `CHECKPOINT_MAX_RETAIN`, prunes toward `CHECKPOINT_MIN_RETAIN`. A checkpoint is a
  prune candidate iff frame ≠ 0 AND `pin_count == 0` AND outside every hot zone.
  Evict greatest-hot-zone-distance first. Retain limits are SOFT triggers (a
  hot-zone or pinned checkpoint is retained past MAX_RETAIN). Frame 0 never pruned.
- "Kept longer but bounded": pool lives between MIN_RETAIN and MAX_RETAIN; does not
  grow with clip length.
- Under fmParallel scatter, count-trigger + hot-zone-distance ordering (not a
  distance-from-a-single-front metric, which is ill-defined under scatter).

**6.4 Scene-change frames as checkpoints.** A cut detected during compute (Section
9.2) makes output[K] a fresh-start, dependency-free reset — the IDEAL recovery anchor
(exact, not approximate, and nothing before it is needed). Therefore output[K] is
stored with the checkpoint flag set. No special search or recovery handling is needed:
a cut-checkpoint is just a present output the Phase-1 descending search (9.5) finds
like any other, and its longer retention makes it more likely to survive in a cold
region. Cuts-as-checkpoints absorbs the previously-considered "remember cut positions"
optimisation. **Sizing caveat:** checkpoint density is now "grid + cuts," so in
cut-heavy content the pool runs nearer MAX_RETAIN; the soft-trigger retention sheds the
unprotected (cold, out-of-zone) cut-checkpoints first, which are exactly the ones safe
to drop. A cut-checkpoint inside a hot zone or pinned is retained regardless (it should
never be evicted while protected — instrument for that as an invariant).

**6.5 Cut-near-grid checkpoints — accept slight waste (V2/V3 decision).** When a cut
lands close to a grid checkpoint (e.g. cut at 47, grid at 50), both are kept — two
near-duplicate anchors. This is ACCEPTED as harmless: retention prunes the cold one;
the cost is minor pool churn. NO suppression logic and NO new "minimum spacing"
threshold (rejected as over-engineering for a marginal saving). V2/V3 confirmed:
CHECKPOINT_INTERVAL=10, MIN_RETAIN=10, MAX_RETAIN=48 are intended for the new
architecture.

---

## 7. Eviction predicate and bounded prune

**7.1 Composite eviction predicate (read atomically under the single lock):**
```text
evict(slot) iff
    slot.pin_count == 0
    AND slot.frame_number is outside every live hot zone [low, high]
    AND ( slot.is_checkpoint
            ? checkpoint-retention-permits (Section 6.3)
            : capacity-permits (Section 7.2) )
ordering: evict greatest-distance-from-any-hot-zone-boundary first.
```
Zone retirement (5.5/5.6) flips a zone inactive so its range stops contributing to
"outside every live hot zone."

**7.2 Capacity trigger (carried forward from D10/D11, self-debouncing).**
Non-checkpoint prune fires when the pool strictly exceeds
`active_ceiling × OVERFLOW_FACTOR`; prunes back toward `active_ceiling`. The
capacity→overflow gap IS the hysteresis (won't re-fire until the pool climbs back
over threshold). No percentage marks, no periodic timers. Store prunes before
hard-ceiling rejection (D11).

**7.3 Bounded prune — three acts (composes the decide/detach/free separation).**
- (a) DECIDE: under the lock, evaluate the eviction predicate, select up to **K**
  victims (greatest-distance-first).
- (b) DETACH: under the SAME lock, detach each selected slot from the cache index
  via the single central remove helper (D09), collecting its `VSFrame*` ref into a
  local list. (a)+(b) are one critical section — a pin cannot creep in between select
  and detach (the slot leaves the index the moment selected; a pinned frame is never
  selected).
- (c) FREE: release the lock; then `freeFrame` the collected refs in one post-lock
  BATCH (freeFrame may trigger core deallocation — kept outside the lock).
- **K** caps the lock-hold for BURST prunes (post-seek, cold-start); rarely binds in
  steady trickle. Instrumentation: count "prune stopped at K with pool still over
  threshold"; raise K if it fires regularly.

---

## 8. Locking — one cache-wide lock, held minimally

**8.1 Decision.** A single mutex guards all cache state (slot index, both pools,
zones, checkpoint flags, pin-counts). Correct-by-construction; the single consistent
snapshot that region-scoped search-and-pin and the composite eviction predicate both
require is FREE under one lock; no lock-ordering, no deadlock; one mental model.

**8.2 Discipline.** Do ALL the right in-memory cache operations inside the lock and
no more — without being stingy. Slow work goes OUTSIDE the lock.
- INSIDE (indivisible commits): search; pin; hole-catalogue; store; pin-record;
  final unpin; eviction-predicate read; prune decide+detach.
- OUTSIDE (never serialises threads through the cache mutex): pixel computation;
  VapourSynth source requests; `freeFrame` of evicted/duplicate refs.

**8.3 Combined principle:** pin-and-record the plan INSIDE the lock; execute the slow
parts OUTSIDE it. The inside-lock pinning is what makes outside-lock execution safe.

**8.4 Fine-grained / cross-region locks: REJECTED as baseline.** They reintroduce
the "separated things humans fail at" problem at the locking layer, threaten the
snapshot property the pin scheme depends on, and add deadlock risk. Deferred to a
MEASURED optimisation applied only if instrumentation proves the single lock is the
actual bottleneck. Fine-graining attacks a load-bearing assumption → requires strong
evidence.

**8.5 Mutex-scoping audit (coder task).** Enumerate every cache operation that
checks-then-acts and confirm none drops the lock between check and act:
search-identify-and-pin; store-on-compute-complete (+ first-in-best-dressed); bounded
prune (decide+detach / free); hot-zone slide; zone retirement; checkpoint establish;
final unpin. PLUS a broad open ask: identify any scenario not enumerated here where
cache state is checked-then-acted across a lock boundary, given this caching goal.

**8.6 V5 firewall — core refcount atomicity does NOT shrink any cache-lock scope.**
There are two unrelated "atomicity" concepts; they must never be conflated:
1. The core's *internal frame reference count* is updated atomically (V5). This
   protects a SINGLE refcount operation (`addFrameRef` / `freeFrame`).
2. The designer-mandated *cache-lock critical sections* (Section 8.7) protect
   MULTI-STEP cache-state decisions (e.g. find-then-pin, decide-then-detach).

These are different layers. The core's atomic refcount does **NOT** make any
cache-lock scope optional, smaller, or splittable. In particular: the fact that a
pin's `addFrameRef` is internally atomic gives NO licence to take that pin OUTSIDE
the cache lock — because the protected operation is the *find-and-pin sequence*, not
the bare refcount bump. Taking the refcount bump alone, outside the lock, would
reintroduce exactly the TOCTOU gaps this architecture eliminates (another thread can
prune the slot between the find and the bump). **The atomic-scope register (8.7) is
the authority on critical-section boundaries; V5 confirms only the refcount's own
atomicity and confers nothing over those boundaries.**

**8.7 Atomic-scope register — DESIGNER-OWNED, MANDATORY, boundaries inviolable.**
Each entry is one indivisible cache-lock critical section. The coder IMPLEMENTS these
exactly; the coder may NOT shrink, split, merge, or reorder the contents of a scope
without explicit designer agreement. All slow work (pixel compute, VS source
requests, the batch `freeFrame`) is OUTSIDE these scopes by mandate.

```text
AS1  arInitial plan-and-pin (Section 9.1):
       { Phase-1 descending bounded search [max(0,N-B), N];
         pin the start point and every present reused frame;
         catalogue the output holes;
         append every pin to frameData pin-list;
         update/slide hot zone(s) for N }
       — one lock acquisition, indivisible.

AS2  arAllFramesReady per-hole store-and-pin (Section 9.2), repeated per hole:
       { first-in-best-dressed check;
         store computed output (or adopt existing winner);
         pin it; append to pin-list }
       — one lock acquisition PER hole; compute happens OUTSIDE before this.

AS3  reused-frame pin during ascending fill (Section 9.2/9.5), as encountered:
       { confirm output[K] present; addFrameRef under lock; append to pin-list }
       — find-and-pin is one indivisible unit (the find-and-add-ref primitive, D06).

AS4  final unpin (Section 4.5):
       { for every entry on the pin-list: unpin (decrement) }
       — one lock acquisition for the whole list at end of arAllFramesReady.

AS5  bounded prune decide+detach (Section 7.3 a+b):
       { evaluate composite eviction predicate;
         select up to K victims (greatest-distance-first);
         detach each victim slot from the index (central remove helper, D09);
         collect freed VSFrame* refs into a local list }
       — one lock acquisition; the batch freeFrame (c) is OUTSIDE this scope.

AS6  checkpoint establish (Sections 6.3/6.4):
       { on store of a grid frame or a detected-cut frame, set is_checkpoint;
         insert into checkpoint pool / ordered index }
       — folded into the relevant AS2 store scope (same lock), not a separate lock.

AS7  zone retirement / merge (Sections 5.5/5.6):
       { test no-pins-in-range + decay-margin; mark zone inactive / merge }
       — performed under the same lock during AS1 or the prune pass; never split.
```
The register is the single authority on critical-section boundaries. If an operation
needed in practice is not covered here, it is raised to the designer — NOT improvised
with an ad-hoc smaller lock.

---

## 9. Request lifecycle with atomic scope

**9.1 arInitial (one atomic + slow work outside).**
- INSIDE one atomic: determine the recovery plan for N (Section 9.5) — i.e.
  Phase-1 descending search for the start point, pinning the start point and any
  present cached outputs that will be reused, and cataloguing the OUTPUT HOLES that
  must be computed; record all pins to the `frameData` pin-list; update hot zone(s)
  for N (D15). One indivisible operation. The backward search is BOUNDED — it must
  bound the search interval `[max(0, N-B), N]` itself, not find a global nearest
  prior anchor and reject it afterward (carried forward from CMS06.11 §4.5.3 /
  `prepare_bounded_recovery_plan()`).
- OUTSIDE the atomic: request from VapourSynth the SOURCE frames needed — **source N
  plus the sources for the genuine output holes only** (Section 9.5.1 — the dissolved
  source window). NOT a blanket `[max(0,N-B), N]` source window. Slow (may trigger
  upstream decode) — never inside the lock.
- Safe because present/reused frames (and the start point) are pinned (protected) and
  holes are absent (cannot be pruned). The plan snapshot stays valid after lock
  release.

**9.2 arAllFramesReady (compute outside, per-frame store-and-pin atomic, end-unpin).**
- The WHOLE activation is the single overarching consumer for the hole-filling pass.
- For each output hole, in ASCENDING frame-number order from the start point forward:
  compute the output OUTSIDE the lock; then a brief per-frame atomic
  { first-in-best-dressed check; store; pin; record }; unlock; proceed. Store-and-pin
  MUST be the same atomic (else a gap lets another thread prune the just-stored frame).
- **Scene-change detection runs during each compute** (Section 1, §A4): if a cut is
  detected at hole K, output[K] is computed as a fresh start (copy source chroma,
  skip the recursive blend) and stored WITH the checkpoint flag set (Section 6.4);
  otherwise output[K] = blend(output[K-1], source[K]). Either way it is store-and-
  pinned and the ascending walk continues. A cut severs the chain at K (frames below
  K do not influence frames ≥ K).
- Compute output N from the now-present, pinned predecessor chain via the existing
  explicit-predecessor processing boundary (D27/D37 — no new pixel maths); return N
  to VapourSynth (the returned frame's ref is TRANSFERRED, not released).
- Final unpin of the whole pin-list in one short atomic at end (4.5).

**9.3 First-in-best-dressed — both branches release (D07).**
- Winner: store-and-pin-and-record (inside atomic), no extra addref beyond the store.
- Loser: `freeFrame` its computed-but-unstored duplicate before discarding — never
  drop the pointer (else leak). Wasted compute acceptable
  (`duplicate_store_computed_but_discarded` counter); the frame ref must still be
  freed.

**9.4 Terminology (kept distinct to stop one word hiding two things):**
*unlock* (release the cache lock) ≠ *freeFrame* (release a frame ref); *source hole*
(missing source → request from VS) ≠ *output hole* (missing cached output →
recompute); *pin* (consumer need, in lock) ≠ *hot zone* (anticipated, heuristic) ≠
*checkpoint flag* (recovery-anchor retention).

**9.5 Recovery model — two-phase: descending search, then ascending fill-holes-only.**
The old conservative `[max(0,N-B), N]` source-request window is GONE (it was an
artifact of held-ref-only defensiveness). Because pins guarantee a present
predecessor cannot be pruned, recovery only computes genuine holes and only requests
their sources. This makes the old "H17 sparse-hole" efficiency the BASELINE, because
the pin scheme makes it safe.

**Phase 1 — find the start point (DESCENDING search from N-1).**
```text
scan K = N-1, N-2, ... down to the lower-bound floor max(0, N-B):
    first PRESENT cached output found  -> start_point = K, STOP (pin it)
        (checkpoint flag IRRELEVANT here: the first present frame wins, close or
         far. A checkpoint matters only because its longer retention makes it MORE
         LIKELY to still be present in a cold region — not because it is preferred
         when a closer ordinary output exists.)
    none present before the floor       -> floor fallback (below)
```
The search is bounded to `[max(0,N-B), N]` (do not scan the whole clip).

**Phase 2 — fill (ASCENDING from start_point to N), fill-holes-only (CMS06.11 §2.1).**
```text
for K ascending from start_point+1 .. N:
    if output[K] already present -> reuse it (find-and-pin); do NOT recompute
    else (hole)                  -> compute (scene-change-aware, Section 9.2);
                                    store-and-pin (first-in-best-dressed)
then return output[N].
```
(There may be OTHER present frames above the start point; fill-holes-only reuses each
of them too — only genuine holes are computed.)

**Floor fallback — D29 approximate fresh-start (confirmed).** If Phase 1 reaches the
floor `max(0, N-B)` without finding any present output, compute the floor frame as a
FRESH START (no predecessor, reset/start semantics — as if frame 0), then fill
ascending to N. Justification: the recursive chroma blend smears toward its
predecessor, so the influence of the approximate start frame DECAYS over the B-frame
walk and the visible result at N-1/N converges to near-continuous-from-0 blending.
**Coherence constraint:** B (BACK_RADIUS) MUST exceed the effective settling length of
the recursive blend, so the approximation is invisible at N (relates CR2/CR3).

**Scene cuts bound the approximation further.** A cut between the start point (or
floor) and N severs the chain and resets EXACTLY at the cut, wiping any approximate-
start error below it. So the floor approximation only matters for a long static span
with NO cut between floor and N — precisely where recursive blending is most stable
anyway. Detected cuts are promoted to checkpoints (Section 6.4), so they become exact,
longer-retained start points found naturally by the Phase-1 search — this absorbs the
previously-considered "remember cut positions" optimisation for free.

**9.5.1 Dissolved source window (the request set).** arInitial requests **source N +
the sources for the genuine output holes only** (the holes found in Phase 1 between
start_point and N). NOT a blanket backward window. Example: start point a checkpoint
at N-10, holes only at N-3 and N-1 → request sources {N-3, N-1, N}, three frames, not
eleven. Requesting a source for a position whose output is already present is
unnecessary and avoided (it would be harmless per CMS06.11 first-in-best-dressed
layering, but the point of the dissolved window is to not do it).

---

## 9A. Carried-forward hard rules (section-level audit of CMS06.11 — gap-fill)

The CMS06.11 body was audited section-by-section as an INDICATIVE gap-finder (not a
definitive authority — where it conflicts with this architecture, CMS07.0 wins). The
following still-valid mechanism detail was not yet stated in CMS07.0 and is carried
forward here, each rule re-validated against the new architecture.

**9A.1 VS-LIFECYCLE-01 — hard VapourSynth API constraint. Non-negotiable.**
```text
Any source frame that will be retrieved with getFrameFilter() in
arAllFramesReady must have been requested with requestFrameFilter()
in arInitial of the SAME callback activation.
```
This is not a CNR3 design choice; it is a hard API rule. Violating it produces silent
failure or undefined behaviour. It is NOT possible to discover in arAllFramesReady
that more sources are needed and request-then-retrieve them there. Consequence under
the new model: the request decision must be made using only arInitial-time
information — which is exactly why AS1 (Phase-1 descending search + hole catalogue +
pin) runs in arInitial, and why the dissolved source request set (§9.5.1) is computed
there. If the Phase-2 walk could ever need a source not in the AS1-computed set, that
is a design bug, not a runtime recoverable.

**9A.2 Reference-count discipline RC1–RC8 (carried; helper NAMES indicative — the
restart coder names them; the OBLIGATIONS are mandatory).**
- **RC1 — Single store helper.** All cache-owned `addFrameRef` calls occur only in
  the one store helper.
- **RC2 — Single remove helper.** All cache-owned `freeFrame` calls occur only in the
  one remove helper. No direct pool/index erase outside it (this is the D09 central
  remove helper; AS5 detach goes through it).
- **RC3 — Store error paths rebalance.** Store takes `addFrameRef` then fails to
  insert → `freeFrame` before returning.
- **RC4 — Lookup error paths rebalance.** find-and-add-ref takes the ref then fails →
  `freeFrame` before returning null.
- **RC5 — Caller exit paths free.** Every path holding a caller-owned ref frees or
  transfers it exactly once on every exit: success, error, early return, abort.
  (Generalised by the pin-list discharge, §3.2/§4.6.)
- **RC6 — Shutdown clears.** Manager destruction iterates both pools, `freeFrame`s
  every slot, clears the index, resets zones, and **logs a warning for any slot with
  pin_count > 0** — under the new model a non-zero pin at shutdown can only mean a
  leaked consumer pin (the API guarantees the free function runs after all getFrame
  activity), so it is a leak indicator, never normal.
- **RC7 — Validation enforces balance.**
  `cache_addframeref_total − cache_freeframe_total == total_live_slots` at quiescent
  points.
- **RC8 — First-in-best-dressed store idempotency.** If output[N] already exists at
  store time: return success WITHOUT taking `addFrameRef`, without modifying either
  pool, without disturbing the existing slot's classification (including its
  checkpoint flag). The cache has NOT taken ownership of the caller's frame — the
  caller (the losing walk) still owns and must free it (§9.3).

**9A.3 RAII owned-ref wrapper — UPGRADED from "recommended" to BASELINE.** The old
design specified a move-only `Cnr3OwnedFrameRef` RAII wrapper (frame + vsapi;
`freeFrame` in destructor; `release()` for transfer) as recommended-but-deferred
because legacy code used explicit handling. The restart removes that constraint:
**new code uses the RAII wrapper from the start** for caller-owned refs (and the
frameData pin-list discharge composes with it). Explicit manual handling is no longer
the accepted interim. The caller-side diagnostic counters and invariant carry
unchanged: `lookup_owned_ref_acquired_total == released_total + transferred_total`
at quiescent points (development mode).

**9A.4 Ceiling derivation (concrete formula, carried — this is V8's
"derives from actual frame bytes" made exact).** At create time: per-plane
bytes = ceil-subsampled width × height × bytes_per_sample summed over planes;
`candidate_ceiling = CACHE_BYTE_BUDGET / estimated_frame_bytes`;
`active_ceiling = clamp(candidate, MIN_HARD_CEILING=150, MAX_HARD_CEILING=1000)`.
The ceil-division on subsampled dimensions is deliberate (never underestimates for
odd dimensions). Disambiguation retained: PAL VHS is 720×576 (ceiling clamps to
1000); a genuine 1440×576 8-bit clip derives ≈863 unclamped — a log showing
ceiling=1000 for a claimed 1440-wide clip indicates a width misreport.

**9A.5 Hard-ceiling abort policy (carried, adapted to pin-list).** A store is allowed
iff live refs after store ≤ active_ceiling. Rejected iff it would exceed the ceiling
AND prune cannot free any frame (everything is pinned / protected). On rejection:
1. Store returns failure WITHOUT taking `addFrameRef` (RC3 preserved).
2. Increment `cache_ceiling_hard_aborts`.
3. The getFrame path executes cleanup (9A.6), then returns a VS filter error:
   *"CNR3: cache ceiling reached ([N] frames). CNR3 is designed for near-linear
   access. Large random seeks in rapid succession may exceed cache capacity."*
Note under the new model: a hard abort implies the pinned+protected set has filled
the ceiling — CR4's headroom rule exists precisely so this never happens in
legitimate operation; an abort is therefore also a CR4-violation signal.

**9A.6 Failure-path cleanup discipline (carried, simplified by the pin-list).** Every
failure path returning a VS error must, before returning:
1. Discharge the frameData pin-list (unpin every recorded pin — this single step
   replaces the old per-kind "unpin checkpoint pins" item).
2. Free every caller-owned VSFrame ref not on the pin-list (RAII makes automatic).
3. Free any source frames obtained from VapourSynth.
4. Free any destination frame allocated but not returned.
5. Hot-zone state: NO rollback — a zone touched by a failed request retires
   naturally via the decay rule (§5.5).
6. Diagnostic counters still increment.
A ceiling abort may leave already-stored frames in the cache — those are valid
outputs and stay. After cleanup, all ref balances hold.

**9A.7 Bounded-start honesty (durable rule, carried).** When recovery starts at the
floor (S > 0 fresh-start, §9.5), the start is a bounded reset/start APPROXIMATION.
It must never be described — in docs, comments, or diagnostics — as exact
full-history recursion. Diagnostics must disclose floor-approximation use (the §10.5
summary's floor count satisfies this). Override only by explicit documented
agreement.

**9A.8 Design Compliance Review (process rule, carried; checklist adapted).** After
each implementation phase (or coherent block), review all changed code paths AND all
unchanged helpers invoked by them, verifying execution follows CMS07.0 — not old
assumptions. Adapted checklist: (1) mutex ownership correct at every mutable-state
access; (2) `_externally_locked`-style helpers called only under the lock; (3)
public locking helpers never call other public locking helpers (deadlock); (4) no
old-architecture logic in any active path; (5) no pool/index erase bypassing the
single remove helper (RC2); (6) no cache-owned addFrameRef outside the store helper
(RC1); (7) store collisions follow RC8; (8) store-failure rebalances (RC3); (9)
find-and-pin is atomic under the lock (AS3); (10) every caller-owned ref freed or
transferred exactly once (RC5); (11) every pin discharged on every exit (pin-list);
(12) hot-zone state never rolled back on failure; (13) cache-side balance holds
(RC7); (14) caller-side lookup-ref diagnostics balance (dev mode); (15) shutdown
clear() releases everything (RC6); (16) no diagnostics write to stdout; (17) NEW —
every critical section matches its atomic-scope register entry (AS1–AS7) exactly.

**9A.9 FORWARD_RADIUS empirical basis (citation carried).** FORWARD_RADIUS=10 rests
on measured linear-encode request jitter from the prior design's simulation (p99
forward jitter ≈ 8 under concurrent linear encoding) — it is measured-and-margined,
not arbitrary.

**9A.10 Audit verdict on everything else in the CMS06.11 body.** Superseded by this
architecture and intentionally NOT carried: §4.2 zone-as-findability-guarantee
lifecycle and the lazy-retirement proxy (replaced §5.5/5.6); §4.4 deferred pinning
(replaced §3.3/D13 supersession); §4.5 bounded-warmup conservative-window recovery
(replaced §9.5; the §4.5.3 bounded-search contract survives as the bounded Phase-1
search in §9.1/AS1); §4.10/4.10.1 sequential fast path + reserved-predecessor model
(subsumed: under pins, EVERY request with a present pinned predecessor IS the fast
path); §8 phase plans and §14 implementation state (old code state — design
superseded, code state unchanged until restart coding begins); §13.18
reserved-predecessor rules (mechanism superseded, sub-rules retained per §12.3).
Old §6 counter catalogue: the accounting INVARIANTS carry (RC7, 9A.3, reservation/
pin balances); the specific counter set is re-derived by the restart coder around the
new mechanisms (§10.4/§10.5 state the required ones).

---

## 10. Constants and parameter coherence

**10.1 Sizing constants (V2/V3/V8 confirmed; carried forward from CMS06.11 with
MAX_RETAIN raised).**
```text
active_ceiling            : frame-count, derived at creation from a 1 GiB nominal
                            byte budget / estimated frame bytes, clamped
                            [MIN_HARD_CEILING=150, MAX_HARD_CEILING=1000].
                            8-bit 4:2:0 PAL 720x576 (~0.59 MiB/frame) → byte budget
                            non-binding → ceiling = 1000 (~590 MiB). Format-agnostic:
                            derives from the ACTUAL frame bytes at creation (V8).
OVERFLOW_FACTOR           = 1.1
CHECKPOINT_INTERVAL       = 10        (+ frame 0; grid promotion)
CHECKPOINT_MAX_RETAIN     = 48  (soft; raised from 32 for cut-checkpoint density)
CHECKPOINT_MIN_RETAIN     = 10
HOT_ZONE_FORWARD_RADIUS   = 10
HOT_ZONE_BACK_RADIUS      = 50
MAX_HOT_ZONES             = 5
JUMP_THRESHOLD            = FORWARD_RADIUS + BACK_RADIUS + 1 = 61   (DERIVED)
decay_margin              = 20        (NEW — Section 5.5)
K (bounded-prune victims) = 8         (NEW — Section 7.3; instrumentation-tunable)
```

**10.2 Coherence rules (CR1–CR5) — coder MUST codify each as a comment directly
above the constant's definition (per Rule 1 / Document A §A12):**
- **CR1** JUMP_THRESHOLD is DERIVED = FORWARD_RADIUS + BACK_RADIUS + 1; never set
  independently.
- **CR2** BACK_RADIUS ≥ bounded recovery search window B; ideally = B (currently
  both 50). **AND** B MUST exceed the effective settling length of the recursive
  chroma blend, so the floor-approximation fresh-start (Section 9.5) is invisible at
  N. (If B were shrunk for memory, confirm it still exceeds the blend settling
  length, or the approximate start would show at the output.)
- **CR3** BACK_RADIUS ≈ 5 × CHECKPOINT_INTERVAL (a zone covers ~5 grid anchors);
  50=5×10.
- **CR4** active_ceiling ≥ ~2 × max-protected set, where max-protected ≈
  MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool ≈ 5×60 + 48 =
  ~348; 1000 ≫ 348. (If pruning can never reach target, this rule is violated.)
- **CR5** CHECKPOINT_MAX_RETAIN ≥ MAX_HOT_ZONES × (BACK_RADIUS / INTERVAL) = 25 grid
  checkpoints, treated as a density FLOOR (cuts add irregular checkpoints on top —
  Section 6.4). MAX_RETAIN raised 32 → 48: 25 grid floor + ~23 headroom for scene-cut
  checkpoints within the ~300-frame protected span. Content-dependent starting value;
  if cut-heavy material keeps the pool pinned at 48 with prune unable to reduce, raise
  it. MIN_RETAIN unchanged at 10.
- **decay_margin bound:** FORWARD_RADIUS ≤ decay_margin ≤ BACK_RADIUS (10 ≤ 20 ≤ 50);
  far below active_ceiling.

**10.3 MAX_HOT_ZONES = 5** scales with concurrent DISTINCT access regions, NOT thread
count. Linear pipe-to-ffmpeg uses ~1 zone regardless of threads → 5 ample. Raise only
if a scatter workload appears (raising it pushes CR4 → active_ceiling may follow).

**10.4 Instrumentation discipline.** A counter BUMP (single increment) may occur
inside a lock; FORMATTING and EMISSION (sprintf, string building, file/stderr writes)
MUST be outside the lock unless specifically justified. Emitting inside a critical
section extends it and pollutes contention measurement. Non-zero error counters are a
hard gate (D14).

**10.5 Recovery-search summary (required end-of-run diagnostic).** To measure actual
search cost (rather than rely on the design's conjectures), record per recovery
(cheap counter bumps; format/emit once at end-of-run, outside any lock):
- **Search-depth distribution** — frames walked back from N-1 before Phase 1 stopped
  (0 = predecessor present immediately = fast path). Histogram, **omit zero-count
  lines**.
- **Terminated-on** — present ordinary output / present grid-checkpoint / present
  cut-checkpoint / floor (approximate start). (Splitting grid vs cut quantifies how
  much cuts-as-checkpoints actually helps.)
- **Holes-filled distribution** — genuine holes computed by Phase 2 per recovery
  (the real recompute cost; distinct from search depth — a deep search may find most
  frames present). Omit zero-count lines.
- **Summary line** — average and max search depth; average holes filled; count and %
  of floor-approximation use.
Reading guide: depth-0 % should dominate under linear workload (low → predecessors
being pruned too eagerly → revisit hot-zone/decay sizing); floor-approximation % more
than tiny → B / lower-bound too short or cache too small. This summary, with the
K-bound prune counter (7.3) and the cut-checkpoint-evicted-while-protected invariant
(6.4), validates the tuning guesses (K=8, MAX_RETAIN=48, decay_margin=20, B) after one
real run.

---

## 11. First implementation milestone (prove ownership before behaviour — D30)

Build, against this spec and **in isolation (no VapourSynth wiring yet)**, the
cache-manager core, mirroring the staged "prove the mechanism before wiring
behaviour" discipline:
- cache-manager data structures (slot = `VSFrame*` ref + frame number + pin_count +
  is_checkpoint; ordered frame-number index per D04; non-checkpoint + checkpoint
  pools per D05; hot-zone state; per-invocation pin-list);
- single cache-wide lock skeleton + the inside/outside-lock discipline;
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume +
  discharge-before-free ordering;
- composite eviction predicate + bounded prune (decide+detach in lock, batch
  freeFrame outside, K-bound).

Prove in isolation: pin/unpin balance = 0, lookup-ref balance = 0, no leaks, no
double-free, eviction never selects a pinned/checkpoint/in-zone slot — BEFORE any
getFrame integration. Header/structure design and exact function naming are the
coder's, aligned with separation-of-responsibilities (pixel processing must not own
cache/scheduling policy; the cache manager must not contain pixel logic).

**Second step — salvage:** identify old code verifiably safe to reuse (response-table
creation, memory diagnostics, the pixel/frame-processing layer incl. the explicit-
predecessor boundary) and copy/modify from the `.txt` reference files into the new
locations, preserving separation of responsibilities.

---

## 12. Decision cross-check vs prior settled decisions (Document B)

**12.1 Carried forward UNCHANGED:** D01 (API4-only), D03 (per-instance), D04 (ordered
maps), D05 (pools own refs, index non-owning), D06 (addref under mutex — this is the
find-and-pin primitive), D07 (first-in-best-dressed), D08 (sliding zones), D09
(central remove helper), D10 (frame-count ceiling), D11 (prune before reject), D14
(diagnostics gate), D15 (zone update at arInitial), D16 (per-invocation frameData),
D26/D33 (old_strict not final authority; retire before parallel), D27/D28 (reuse
processing boundaries; no parallel pixel algorithms — override discipline), D29
(bounded-start reset semantics), D30 (compute/store/return/transfer/authority
separately provable), D31/D32 (CMS02-J0 mandatory before parallel wiring; diagnostics
consolidation), D36/D37 (sequential predecessor reuse; fast path = source N + cached
output N-1 via explicit-predecessor boundary — generalised: predecessor is PINNED).

**12.2 SUPERSEDED:** **D13** ("non-checkpoint pinning deferred") — superseded by
Position B (Section 3.3): consumer-pinning is the mandatory baseline, not a deferred
escalation. Reason: fmParallel goal + KISS/maintainability.

**12.3 Superseded IN MECHANISM, sub-rules RETAINED** (the old held-ref-only
predecessor-reservation cluster, D38–D58): the held-ref reservation MECHANISM is
replaced by consumer-pins, BUT these sub-rules survive (generalised from "the
reserved predecessor ref" to "any consumer pin"): single-ownership/null-on-consume
(D45/D53/D54 → Section 4.4); lookup-ref accounting `acquired==released+transferred`
(D55 → 4.8); ordinary refs are lookup-addref not a distinct pin-kind, don't conflate
with checkpoints (D56 → Section 6); H17 deferred (D48/D57 → 9.5); Option A (rely on
serial callback ordering) rejected (D49 → Section 3.3). The fail-closed-only H15.6B
draft stays retired (D46/D58).

---

## 12A. Failure-mode register vs the new architecture (V7 RESOLVED — bring-across proof)

CMS06.11 §1 named the failure modes the old design existed to prevent. V7 asked: does
the NEW architecture provably cover each? Mapping (this is the concrete
bring-across-and-revalidate evidence that nothing the old design guarded is lost):

- **FM1 — Prune destroys in-flight predecessor.** OLD: hot-zone heuristic + deferred
  pinning. NEW: **consumer-pins make it structurally impossible** — an in-flight
  predecessor is pinned; the eviction predicate (§7.1) never selects a pinned slot.
  This is THE reason hot zones could be demoted to decay-hints: the one failure mode
  they were load-bearing for is now covered by something STRONGER (structural, not
  heuristic). ✓ improved.
- **FM2 — Prune destroys a checkpoint needed by a recovery chain.** NEW: checkpoint
  retention flag (§6) protects future-needed anchors; an ACTIVELY-walked checkpoint is
  pinned during the walk (AS3). ✓
- **FM3 — Jump-recovery burst exceeds pool capacity.** NEW: bounded prune + K-bound
  (§7.3) + CR4 (ceiling ≥ ~2× max-protected); and the dissolved window (§9.5.1) +
  fill-holes-only shrink the burst footprint to genuine holes, not a blanket window. ✓
- **FM4 — Prune eviction key wrong (lowest-first).** NEW: evict greatest-distance-
  from-zone first (§7.1), never by frame-number magnitude. ✓ carried forward.
- **FM5 — VSFrame reference leak.** NEW: RC invariant (one addFrameRef per slot, one
  freeFrame on removal) + pin-list discharge on every exit path (§3.2/§4.6) +
  first-in-best-dressed both-branches-release (§9.3). ✓ strengthened.
- **FM6 — VSFrame use-after-free.** NEW: find-and-pin under the lock before use (AS3);
  pins keep frames alive for the consumer's whole lifetime. ✓
- **FM7 — Duplicate store overwrites / double-references.** NEW: first-in-best-dressed
  idempotent store, both branches release (§9.3). ✓ carried forward.

**V7 conclusion:** hot zones were the heuristic mitigation for FM1 (and partly
FM2/FM3). The new architecture replaces that heuristic with a structural guarantee
(pins), which is precisely why demoting hot zones to decay-hints is provably safe.
FM4–FM7 were never hot-zone responsibilities and carry forward unchanged. Every old
failure mode is covered, most of them more strongly. Also retained: the RC invariant
(CMS06.11 §2 goal 5) — exactly one addFrameRef per slot while held, one freeFrame on
removal.

---

## 12B. Root rationale — CNR3's cache replaces CNR2's predecessor approximation

The reference CNR2 source (https://github.com/Asd-g/AviSynth-vsCnr2 ;
https://raw.githubusercontent.com/Asd-g/AviSynth-vsCnr2/refs/heads/main/src/vsCnr2.cpp)
is the algorithmic ancestor of CNR3 and is prime salvage for the pixel layer. It also
makes the motivation for CNR3's entire cache+recovery architecture concrete:

CNR2 runs `MT_SERIALIZED` (single-threaded, effectively sequential) and keeps the
previous filtered output in a single `prev` member. Its `GetFrame(n)` does:
```cpp
if (last_frame != n - 1) {           // non-sequential request
    prev = child->GetFrame(n - 1);   // fetch SOURCE n-1 as an APPROXIMATE predecessor
    downSampleLuma(prevp_y, prev);
}
... blend cur with prev; prev = dst;  // sequential case uses the true previous OUTPUT
```
So when a request is NOT sequential, CNR2 cannot supply the true previous OUTPUT and
substitutes the previous SOURCE frame — an approximation it gets away with only because
it is serialized and the recursive blend forgives a one-frame stand-in.

**CNR3 deliberately abandons serialization (the final goal is fmParallel) and therefore
cannot rely on that approximation.** CNR3's cache + recovery model (Sections 9.5, 5, 6,
7) is the principled replacement: on any request, supply the EXACT previous output —
present-and-pinned, or rebuilt by the descending-search / fill-holes walk, or (only when
nothing is recoverable within the bounded floor) an explicit fresh-start that the blend
smear forgives far more comfortably than CNR2's source-substitution ever did.

**Coder consequence (stressed):** salvage CNR2's pixel maths (response tables, blend,
downSampleLuma, in-compute scene-detect — V8.1) but NOT its predecessor/recovery logic.
The `last_frame != n-1 → use source[n-1]` shortcut is exactly what CNR3 exists to
eliminate. Importing it would defeat the architecture.

---

## 13. Verify items (confirm before treating dependent sections as final)

- **V1 RESOLVED** algorithmic temporal reach backward-only / purely causal —
  confirmed by Dave against CNR2 itself (authoritative); corroborated by §A4, the
  proven CMS06.11 request window, and absence of any forward reference. Closed.
- **V2 RESOLVED** checkpoint retention params (MAX_RETAIN raised to 48, MIN_RETAIN=10)
  confirmed intended for the new architecture; cut-near-grid waste accepted (§6.5).
- **V3 RESOLVED** checkpoint establishment (every-10 grid + frame 0 + cut frames)
  confirmed intended (§6.3/§6.4).
- **V4 RESOLVED** frameData carry + cleanup confirmed against the local R76
  VapourSynth4.h: `void **frameData` (line 337) is the sanctioned carry, defaults
  NULL, ours to free before the last call / on error (`setFilterError` line 409); no
  core cleanup hook. WE manage allocation/cleanup per our discipline (Section 3.2);
  coder implements, no discretion. Per-frame-number single-threading guarantees the
  pin-list is contention-free. Cut/storage residual: none material — the header uses
  the `void **frameData` pointer form (no `void *[4]` inline variant in this header).
- **V5 RESOLVED (with firewall)** `addFrameRef`/`freeFrame` declared (lines 377–378);
  core refcount atomicity relied upon for the refcount itself ONLY. Section 8.6
  firewall: this confers NO licence to shrink any cache-lock scope (8.7 register is
  the authority). Coder may confirm atomicity but gains nothing over lock boundaries.
- **V6 RESOLVED (design position; residual is runtime-tuning only)** R76 caches
  upstream node output by default (`VSCacheMode cmAuto`; controls `setCacheMode` line
  363, `setCacheOptions` line 364, `setMaxCacheSize` line 482). So our SOURCE requests
  (including dissolved-window holes) are often served warm by the core without
  re-decode — a COST benefit, never a correctness factor. Design position: leave our
  OWN node at cmAuto and do NOT call `setCacheMode`/`cacheFrame` on it — our cache
  manager is the sole authority on OUTPUT caching; layering the core cache over our
  outputs would be a duplicative second cache. The exact upstream-cache aggressiveness
  under load is pure runtime tuning, not a design dependency.
- **V7 RESOLVED** the original failure modes are the named FM1–FM7 (CMS06.11 §1).
  Hot zones were the heuristic mitigation for FM1 (partly FM2/FM3); the new
  architecture replaces that with a structural guarantee (consumer-pins), which is why
  demoting hot zones to decay-hints is provably safe. Full mapping in §12A. Every old
  failure mode is covered, most more strongly.
- **V8.1 — PIXEL-LAYER design item (NOT a cache-manager concern; recorded here for the
  pixel-layer/salvage step, not CMS07.0 scope).** CNR3 computes in NATIVE subsampling
  at NATIVE pixel bit depth, using a WIDE (int64) arithmetic ACCUMULATOR for the
  weighted blend with the shift scaled by depth — NOT pixel promote/demote to a working
  format. This is the proven overflow-safe method in the reference CNR2 source (its
  changelog "Fixed high bit depth chroma overflow" corresponds to the int64 accumulator
  + `shift2 = depth<<1`). Restrict to YUV planar 420/422/440/444, 8..16-bit, as CNR2
  does. Reading subSamplingW/H parameterises one compute loop across subsamplings; no
  chroma-siting/resampling.
  **Reference (pixel layer salvage):**
  - repo: https://github.com/Asd-g/AviSynth-vsCnr2
  - source: https://raw.githubusercontent.com/Asd-g/AviSynth-vsCnr2/refs/heads/main/src/vsCnr2.cpp
  **ADOPT from CNR2:** the response-table construction (raised-cosine weighting from
  mode/ln,lm,un,um,vn,vm), the weighted blend formula
  `dst = (weight*prev + (shift-weight)*cur + shift1) >> shift2` with int64 weight and
  `shift2 = 2*depth`, `downSampleLuma` (2×2 box average of luma to chroma grid), and
  the in-compute scene-change accumulation/threshold. These are clean of any caching
  concern (CNR2 has no real cache).
  **DO NOT ADOPT from CNR2:** its recovery/predecessor logic. CNR2 uses
  `MT_SERIALIZED` and, on a non-sequential request (`last_frame != n-1`), fetches
  SOURCE[n-1] as an APPROXIMATE predecessor. CNR3 MUST NOT do this — CNR3 replaces that
  approximation with EXACT output[n-1] recovery via the cache (Sections 9.5 / 12B).
  Copying CNR2's `GetFrame` wholesale would import the very approximation this
  architecture exists to eliminate.
- **V8 RESOLVED for the cache (format-agnostic).** The cache stores `VSFrame`
  references and derives `active_ceiling` at filter creation from the ACTUAL frame
  byte-size, whatever the format — so the cache manager needs no knowledge of bit
  depth or subsampling. Working footprint note: 8-bit 4:2:0 PAL 720×576 ≈ 0.59
  MiB/frame → byte budget non-binding → ceiling clamps at 1000 (~590 MiB); higher bit
  depths self-adjust the ceiling downward (16-bit 4:2:0 ≈ 860 frames; 16-bit 4:2:2 ≈
  648). No `[VERIFY]` remains for the cache.

---

## 14. Changelog

### CMS07.0 FINAL — 2026-06-13 (promotion from CMS07.0a)
- **Section-level bring-across audit of the CMS06.11 body completed (§9A)**, treating
  CMS06.11 as INDICATIVE (gap-finder), not definitive. Carried forward and
  re-validated: VS-LIFECYCLE-01 full statement (9A.1); RC1–RC8 reference-count
  discipline, obligations mandatory / helper names indicative (9A.2); ceiling
  derivation formula with the 720-vs-1440 disambiguation (9A.4); hard-ceiling abort
  policy + error message, now also a CR4-violation signal (9A.5); failure-path
  cleanup discipline, simplified by the pin-list (9A.6); bounded-start honesty
  durable rule (9A.7); Design Compliance Review process + 17-item adapted checklist,
  item 17 new: critical sections must match the AS register (9A.8); FORWARD_RADIUS
  empirical basis citation (9A.9). Superseded-not-carried material listed (9A.10).
- **RAII owned-ref wrapper UPGRADED from recommended to BASELINE (9A.3)** — the
  restart removes the legacy-code constraint that justified deferral.
- Stale `[VERIFY]` markers cleared from §6.3 and §10.1 (the underlying items were
  resolved earlier in §13 but the body annotations lagged — caught by self-review).
- §15 rewritten from open-items to completion status. Header promoted to final.
- Deliberately out of scope: handover pack bump (deferred by explicit decision);
  pixel-layer spec (V8.1 guidance in §13 feeds that separate workstream).

### CMS07.0a — 2026-06-12 (working draft increment)
- **V1 RESOLVED:** algorithm confirmed purely causal against CNR2 (current source +
  previous output; no forward peek). `[VERIFY]` dropped from Section 1.
- **Recovery model rewritten (Section 9.5):** two-phase — Phase 1 DESCENDING search
  from N-1 for the nearest present cached output (start point; checkpoint flag
  irrelevant at search time), bounded to `[max(0,N-B), N]`; Phase 2 ASCENDING
  fill-holes-only (reuse every present frame, compute only genuine holes).
- **Dissolved source window (Section 9.5.1):** the old blanket `[max(0,N-B), N]`
  source-request window is GONE. arInitial requests source N + sources for genuine
  holes only. The old "H17 sparse-hole" efficiency becomes the baseline (pins make it
  safe). H17-as-deferred-item retired.
- **Floor fallback (Section 9.5):** D29 approximate fresh-start confirmed; justified
  by recursive-blend smear/decay; CR2 extended — B must exceed blend settling length.
- **Scene cuts (Sections 1, 9.2, 6.4):** detection runs during compute; a cut is an
  in-walk fresh-start that severs the chain; cut frames are promoted to checkpoints
  (irregular spacing — search is position-agnostic, D04); grid promotion retained
  alongside; absorbs the deferred "remember cut positions" idea.
- **CHECKPOINT_MAX_RETAIN 32 → 48** for cut-checkpoint density; CR5 reframed as a
  density floor; CR4 max-protected updated to ~348.
- **V2/V3 RESOLVED:** INTERVAL=10, MIN_RETAIN=10, MAX_RETAIN=48 confirmed for the new
  architecture; cut-near-grid near-duplicate checkpoints accepted as harmless (§6.5,
  no suppression logic).
- **Recovery-search summary instrumentation added (§10.5):** depth distribution,
  terminated-on (ordinary / grid-cp / cut-cp / floor), holes-filled distribution,
  averages — end-of-run, zero-count lines omitted — to validate tuning guesses on real
  runs.
- **V4 RESOLVED** against the local R76 VapourSynth4.h: frameData is the sanctioned
  carry, manually managed by us, no core cleanup hook; coder implements our discipline
  with no discretion (§3.2). Per-frame-number single-threading guarantees pin-list
  contention-freedom. Noted we deliberately do NOT use the core `cacheFrame` API.
- **V5 RESOLVED with a firewall (§8.6):** core refcount atomicity is relied upon for
  the refcount only and confers NO licence to shrink/split any cache-lock scope —
  framed explicitly to prevent the coder mistaking refcount atomicity for permission
  to take pins outside the lock (which would reintroduce TOCTOU).
- **Atomic-scope register added (§8.7):** AS1–AS7, designer-owned, mandatory,
  boundaries inviolable — the single authority on critical-section contents; the coder
  implements exactly and may not shrink/split/merge a scope without designer agreement.
- **V6 RESOLVED:** engine caches upstream sources by default (cmAuto); cost not
  correctness; leave our node at cmAuto, do not layer the core cache over our outputs.
- **V7 RESOLVED:** FM1–FM7 failure-mode register added (§12A) mapping each old failure
  mode to the new architecture; hot-zone demotion proven safe (FM1 now structural via
  pins, not heuristic).
- **V8 RESOLVED for the cache (format-agnostic; ceiling self-derives).** V8.1 recorded
  as a PIXEL-LAYER item (compute native-subsampling at widened bit depth, parameterised
  by format descriptor; compare to CNR2 source) — out of CMS07.0 cache scope.
- **ALL VERIFY ITEMS V1–V8 NOW RESOLVED.** Remaining for CMS07.0-final: section-level
  bring-across audit against the CMS06.11 body (no still-valid mechanism detail lost).
- **CNR2 reference source checked** (Asd-g/AviSynth-vsCnr2). Confirms: purely causal
  (`prev` is the previous OUTPUT); native-subsampling native-bit-depth compute with an
  int64 blend accumulator and `shift2 = 2*depth` (the "high bit depth chroma overflow"
  fix) — adopted as the V8.1 pixel-layer method; response tables / blend / downSampleLuma
  / in-compute scene-detect are salvage material. §12B added: CNR3's cache+recovery is
  the principled replacement for CNR2's serialized `last_frame!=n-1 → source[n-1]`
  predecessor approximation. STRESSED: salvage CNR2 pixel maths, NOT its recovery logic.
  Source links recorded in §1, §12B, §13 V8.1.

### CMS07.0 — 2026-06-12
Architectural supersession of CMS06.11. Pinning becomes the mandatory correctness
mechanism (consumer-pins); hot zones demoted to decay-hints over a pinned liveness
index; checkpoints become a flag + retention rule, not a pin; single cache-wide lock
held minimally; bounded prune (decide+detach in lock, freeFrame outside, K=8); new
constants decay_margin=20 and K=8; five parameter-coherence rules (CR1–CR5) to be
codified as code comments; fmParallel final-goal invariant stated outright. D13
superseded; held-ref-only predecessor-reservation cluster superseded in mechanism
with sub-rules retained; foundational decisions D01–D11/D14–D16/D26–D33/D36–D37
carried forward. DRAFT: sections dependent on V1–V8 marked `[VERIFY]`.

---

## 15. Completion status

Originally a DRAFT open-items list; now the closure record.
- V1–V8: ALL RESOLVED (§13) — V1/V8.1 against the CNR2 reference source, V4/V5/V6
  against the local R76 VapourSynth4.h header, V2/V3/V8 by design confirmation, V7 by
  the FM1–FM7 mapping (§12A).
- Section-level bring-across audit against the CMS06.11 body: DONE (§9A) —
  CMS06.11 treated as INDICATIVE gap-finder; carried rules re-validated against the
  new architecture (RC1–RC8, VS-LIFECYCLE-01, abort/cleanup/shutdown, bounded-start
  honesty, DCR process, ceiling formula); RAII wrapper upgraded to baseline;
  superseded material listed (§9A.10).
- Final self-review pass: DONE (stale `[VERIFY]` markers cleared; version strings,
  cross-references, and old-concept references checked).
- NOT in this document's scope (separate deliverables): the matched handover pack
  bump (A/B/C → "CMS07.0-or-later", per the Handover Pack Production Spec) — deferred
  by explicit decision; the pixel-layer specification (V8.1 guidance recorded in §13
  for that workstream).
