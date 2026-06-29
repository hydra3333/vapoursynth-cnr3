# CNR3 W.3 — Review Register (through Round 4: W3OD appended; register CLOSED)

**Subject:** the §7.5 COMBINED LIVE STORE-AND-PRUNE HELPER — wiring live cache eviction + lazy hot-zone
retirement into the arAllFramesReady store path. Pre-code design + scope review, three-way (W3D/W3C/W3OD,
W3X authority), per `CNR3_W3_Three_Way_Review_Process_v1_0.md`.

**Build state:** committed `CMS07-W.2-hot-zone-observation-arInitial`, four-way 54/54. CMS07.14 controlling.
Source under review = the W.2-committed tree (`cnr3_arAllFramesReady.cpp`, `cnr3_cache_core.cpp/.h`).

**Append-only.** One global comment sequence (W3X keeps the authoritative next-free number). LF line endings.

*(Round-0: §0 added — W3OD's recommended opening gate, with W3D's source-verification of it.)*
*(Round-1: W3OD appended verdicts on all seven SR-D, raised SR-OD-01/02/03, recommended SR-D-06 → RESOLVED. #0001–#0010.)*
*(Round-2: W3C appended verdicts on all SR-D + SR-OD, raised SR-C-01..05, recommended SR-D-06 RESOLVED + SR-OD-02 per-store. #0011–#0025.)*
*(Round-3: W3D appended verdicts on the W3C round + SR-OD; ratified SR-D-06 on own read; raised the SR-C-01 formula refinement. #0026–#0034. §F + §G added.)*
*(Interim W3D↔W3X: SR-D-08 raised (#0035); W3X ruled #0036 SR-D-06 RESOLVED + #0037 SR-OD-02 per-store → SR-D-02 closed. §H added.)*
*(Round-4: W3OD appended verdicts on SR-C-01..05 and SR-D-08 (#0038–#0044); register CLOSED. §I added; STATE SUMMARY refreshed. Comment numbers #0038–#0044 are a W3OD-proposed block; W3X reconciles canonical numbering.)*
*(Round-5: W3C final trip — verdicted SR-D-08 (#0045 agree) + last-gasp scan (#0046, no new finding). §J added. SR-D-08 now full three-way; register CLOSED across all three reviewers. STATE SUMMARY refreshed by W3D.)*

---

## STATE SUMMARY (as of Round 5 — register CLOSED, full three-way) — derived from the findings below; never gets ahead of the recorded verdicts

**HEADLINE:** **Register CLOSED — full three-way.** All ten SR-D/SR-OD and all five SR-C findings are AGREED/RESOLVED across W3D, W3OD, and W3C, with W3X's two rulings recorded (#0036 SR-D-06; #0037 SR-OD-02 per-store). SR-D-08 — the last open item — is now agreed by all three (W3C #0045), and W3C's last-gasp pass (#0046) raised no new finding. **0 in dispute · 0 awaiting W3X · 0 pending · 0 deferred.** W3D proceeds to write the W.3 coder scope + designer-owned eviction-proof harness from the closed register.

**RESOLVED / AGREED — full three-way (D+OD+C), or W3X-ruled**
| ID | Sev | Status | Agreed decision · settled by |
|----|-----|--------|------------------------------|
| SR-D-01 | BLOCKER | AGREED | One-critical-section combined helper from proven `_locked` primitives; "no old two-lock call site" → SR-C-04 checklist. · D+OD+C |
| SR-D-02 | BLOCKER | RESOLVED | Safety agreed three-way (walk deps pinned / hot-zone-protected); timing ruled per-store (#0037). |
| SR-D-03 | MAJOR | AGREED | `Cnr3OutputCacheCore` public method; count 54→55+; ordering selftest per SR-OD-03. · D+OD+C |
| SR-D-04 | MAJOR | AGREED | Returned-frame correctness + observable eviction + target-not-evicted + four-way green; harness drives BOTH triggers incl. cut-dense segment. · D+OD+C |
| SR-D-05 | MAJOR | AGREED | Retire before select, same lock; `current_frame` = activation target N (SR-OD-01). · D+OD+C |
| SR-D-06 | MAJOR | RESOLVED · W3X | Composite selector's shared pin/hot-zone guard (`:2430`, before `is_checkpoint`) protects cut frames; no change (#0036). · D+OD+C read |
| SR-D-07 | MAJOR | AGREED | Single-activation = validation scope; satisfied once SR-OD-01 (target-N) + SR-OD-02 (per-store) pinned. · D+OD+C |
| SR-D-08 | MAJOR | AGREED | Six-point concurrency checklist (one-hold/no-gap, `_locked`-only, call-local vectors, immutable outside-lock work, free-after-unlock, no new sync) → scope patch-review gate. · D raised (#0035) + OD (#0043) + C (#0045) |
| SR-OD-01 | MAJOR | AGREED | Retirement `current_frame` = activation target N; helper takes `stored_frame` + `target_frame`, passes target to retire. · OD+C+D |
| SR-OD-02 | MAJOR | RESOLVED · W3X | Per-store (#0037): each store+prune a self-contained bounded critical section; concurrency-stable. |
| SR-OD-03 | MINOR | AGREED | Discriminating ordering selftest: decay-eligible zone shielding an otherwise-prunable frame; ordering proven by victim identity. · OD+C+D |
| SR-C-01 | MAJOR | AGREED (refined) | Once/outside-lock/conservative-high/overflow-checked byte estimate; **summed actual plane bytes**, NOT ×3 (2× over on 4:2:0). · C+D+OD |
| SR-C-02 | MAJOR | AGREED | Pre-reserve pin-list (if AS2) + all four prune vectors before the one lock; reservation failure ⇒ no mutation. · C+D+OD |
| SR-C-03 | MAJOR | AGREED | `Cnr3CacheStoreKind` enum (is_checkpoint × should_pin, four kinds); cache core stays VSAPI-free; plugin keeps authoritative-return layer. · C+D+OD |
| SR-C-04 | MAJOR | AGREED | Call-site replacement checklist + grep no old store-only path remains; cache-hit routes through none of store/prune/retire. · C+D+OD |
| SR-C-05 | MINOR | AGREED | Fixed KDT line + field set (incl. both trigger flags, selected/detached counts), after helper returns, outside lock, `cnr3_status_name`. · C+D+OD |

**OPEN / NEEDS W3X:** none. **PENDING:** none. **DEFERRED:** none (concurrency stress is SR-D-07's validation scope).

**NEXT:** the review is CLOSED across all three reviewers. W3D writes (1) the W.3 coder build scope — combined helper as a `Cnr3OutputCacheCore` public method; `Cnr3CacheStoreKind` enum + per-site mapping; summed-plane byte estimate; combined pre-reserve contract; §7.5 six-step order with retire(target-N) before per-store prune; SR-D-08 six-point concurrency gate; call-site checklist + cache-hit exemption; fixed KDT line — and (2) the designer-owned eviction-proof harness (golden 576p50 + `SetVideoCache(mode=0)`, driving past BOTH capacity and checkpoint triggers incl. a cut-dense segment; proving returned-frame correctness + observable eviction + target-N-not-evicted + the discriminating retire-before-select selftest). Per W3OD's request, the drafted scope is cross-checked against source by W3OD before it reaches the coder. No wiring code until the scope is approved.

*Note: append-only body header status fields were not bumped each round (no-edit-headers convention); this index computes current status from the recorded verdict comments. W3X owns canonical reconciliation. (W3D refresh at Round 5: SR-D-08 is now full three-way after W3C's #0045; the register is closed across all three reviewers with W3X's two rulings recorded.)*

---

## 0. ITEM 0 — live-site confidence check of the §7.5 contract (W3OD gate; verify BEFORE scope draft)

**Origin:** opening gate recommended by W3OD, relayed via W3X. Framing: Step 0 CLOSED the DESIGN questions
(§7.4/§7.5/§7.6 are ratified — do NOT re-litigate them). W.3 is the FIRST phase where those components compose
live in `cnr3_arAllFramesReady`, and the first with a live READER of the W.2 hot-zone observation. This is a
CONFIDENCE CHECK against the live source — "the design is closed; confirm the implementation site matches it"
— not a re-opened Step 0. It earns its keep even when every check passes; the point is that it was checked.

**The four checks (W3OD):**
1. Does §7.5's six-step order map onto the real `arAllFramesReady` store path without restructuring — a single
   store site or several; does live lock scoping match "one `cache_mutex_` critical section, free-after-unlock"?
2. Is the §7.4 checkpoint-retention trigger callable from the live helper AS BUILT (reuse, not modify the W.1
   primitive — the `noncheckpoint_capacity_permits` flag, the retain/byte-estimate inputs fit the live site)?
3. **(LOAD-BEARING)** Does W.2's observation actually GATE W.3's prune candidate selection? SR-C-02's argument
   is that observing N centres a hot zone on N so the prune excludes output[N]. W.2 proved the observation
   FIRES; W.3 is the first time anything READS that zone state to make an eviction decision. Confirm from
   source that the live prune's candidate selection genuinely consults the hot-zone membership the W.2 call
   recorded — confirmed, not assumed.
4. **(Step 0 SR-D-04, sharpened by W3X — tracked as W.3 SR-D-07)** Single-activation scope holds AND all
   pruning functionality is VALIDLY ENABLED NOW. By design, introducing concurrency later must NOT require new
   or changed code — only added validation/contention stress. If ANY part of the W.3 design would need new or
   changed code under concurrency, that is a CURRENT design flaw to flag NOW, not an fmParallel deferral.

**W3D analysis — VERIFIED against the committed W.2 source (per the gate's own "verify, don't defer"):**
- **Point 1 — MAPS, but multi-site (folds into SR-D-01/SR-D-02).** Live store path is NOT a single site:
  production stores (no pin) at ~:519/:525/:1540 and AS2-consumer stores (with pin) at ~:1013 (floor) / ~:1221
  (holes). The six-step order composes via existing `_locked` primitives; `execute_bounded_prune_pass_locked`
  detaches victims into an out `detached_slots` vector for a POST-LOCK free — matching "one critical section,
  free-after-unlock". Order maps; the multi-site wiring is the work (= SR-D-01, SR-D-02).
- **Point 2 — callable AS BUILT, reuse (one cpp-body verify left).** `cnr3_calculate_cache_prune_trigger_decision`
  already evaluates BOTH §7.2 capacity and §7.4 checkpoint triggers; `execute_bounded_prune_pass_locked` takes
  `retain_checkpoint_count` + a separate `checkpoint_candidate_order` — the §7.4 trigger is consumed INSIDE the
  existing prune pass (W.1). Live helper REUSES with NO cache-core change. RESIDUAL: confirm the cpp body
  composes both triggers' candidates into the one bounded pass.
- **Point 3 — CONFIRMED for non-checkpoint output[N]; residual for cut-output[N] (→ SR-D-06).** Chain holds in
  source: observe(N) creates/refreshes active zone covering N; the non-checkpoint selector skips
  `pin||is_checkpoint||frame_is_inside_hot_zone`; step-4 retire spares the just-observed zone (margin not
  elapsed same-activation). RESIDUAL: a cut output[N] is stored as a checkpoint, so confirm the checkpoint side
  of `select_composite_prune_candidates_bounded_locked` applies the same hot-zone exclusion. [W3OD CLOSED this
  in r1 — see SR-D-06.]
- **Point 4 — holds BY CONSTRUCTION under the SR-D-01 shape; W3X sharpened the bar (SR-D-07).** All step-1..5
  primitives are `_locked`; combined helper takes `cache_mutex_` once across steps 1-5 and frees post-lock.
  Sharpened test: reviewers must check nothing is correct ONLY because one activation holds the lock
  uninterrupted in a way that needs rework under concurrency. Any such spot → SR-D-07 now.

**Disposition.** Three of four checks map cleanly; point 3 confirms for the common case with one residual
(SR-D-06). No mismatch forces a re-opened gaps review — proceed to verdict and, on closure, to scope.

---

## A. THE TASK (W3D framing — verify against the source)

W.3 is where the live cache first EVICTS. Today it never does: in committed W.2 source, the live
arAllFramesReady path has ZERO calls to `execute_bounded_prune_pass` or `retire_decay_eligible_hot_zones`;
`store_*_owned_frame*` just appends. W.1 added the §7.4 trigger primitive; W.2 wired hot-zone OBSERVATION at
arInitial. W.3 wires the CONSUMER: the §7.5 combined helper that, on a live store, also retires stale zones
and prunes.

**Read-first finding (W3D): the combined helper is mostly a COMPOSITION of existing `_locked` primitives, not
new policy.** The CMS §7.5 six-step order maps onto primitives that already exist:
```
§7.5 step                                  existing _locked primitive (cache_core.h)
1 store / adopt                            store_noncheckpoint_owned_frame_locked / store_checkpoint_owned_frame_locked
                                           / store_owned_frame_and_record_pin_locked (AS2)
2 set is_checkpoint flag                   folded into store_checkpoint_owned_frame_locked (monotonic, §6.6)
3 pin IF AS2 consumer (NOT production)     store_owned_frame_and_record_pin_locked pins; production store does not (§4.1)
4 retire decay-eligible hot zones          retire_decay_eligible_hot_zones_locked(current_frame)
5 evaluate trigger(s) + decide/detach      execute_bounded_prune_pass_locked(...)  -- honours BOTH §7.2 capacity
                                           AND §7.4 checkpoint triggers (W.1); detaches into out detached_slots
6 unlock, then FREE                        free detached_slots (+ duplicate loser) AFTER the lock
```
Genuinely NEW W.3 code: (i) the orchestrating combined helper (steps 1-5 under ONE `cache_mutex_`, step 6
post-lock free); (ii) WIRING at the live store sites (production ~:519/:525/:1540 no-pin; AS2 ~:1013 floor /
~:1221 holes pin); (iii) the once-computed conservative frame-byte estimate; (iv) a temporary prune/retire KDT.
PRIMITIVES are proven; WIRING, ORDERING, LOCK DISCIPLINE are the work. W.3 RELIES on W.2: produced output[N]
is unpinned (§4.1), surviving its own activation's prune only because W.2 observed N into its own hot zone
(§7.6) and §7.1 excludes hot-zone-covered slots.

---

## B. FINDINGS (Round 0 SR-D by W3D; Round 1 verdicts + SR-OD by W3OD)

### SR-D-01   [severity: BLOCKER]   [status: ACKNOWLEDGED]
Finding (W3D): the combined helper MUST perform steps 1-5 in ONE `cache_mutex_` critical section by composing
the `_locked` primitives, and MUST NOT be "existing public store (lock/unlock) THEN separate public prune
(lock/unlock)". §7.5 forbids a prune running while the just-stored slot is still unpinned in a separate scope.
W3D lean: a new combined helper (public entry pre-reserves working vectors outside the lock; one lock; steps
1-5; post-lock free); arAllFramesReady call sites switch to it. CONFIRM all needed `_locked` store variants are
callable in that composition and no call site is left on the old two-lock path.

[#0001 | OD | r1 | agree]  Verified against source. The `_locked` store variants
(`store_owned_frame_and_record_pin_locked`, `store_noncheckpoint_owned_frame_locked`,
`store_checkpoint_owned_frame_locked`, `store_recovery_plan_hole_owned_frame_and_record_pin`) all exist as
class members callable within one composition, and the header documents the detach-then-free-after-unlock
contract (detached frames "released after cache_mutex_ is unlocked") with pin-list capacity pre-reserved
before the lock. The one-critical-section / no-two-lock shape is both required by §7.5 and buildable from
proven primitives. Agreed as stated; the "no call site left on the old path" check is a scope/patch-review
obligation (every production + AS2 store site must route through the new helper) — flag it as an explicit
read-first item in the W.3 coder scope.

### SR-D-02   [severity: BLOCKER]   [status: RESOLVED — safety agreed three-way; timing ruled per-store (#0037)]
Finding (W3D): the RECOVERY multi-store is the highest-risk interaction. One recovery activation stores floor
+ many holes + target. If the combined-helper prune fires per-store during the walk, it must NOT evict any
frame the same walk still depends on. W3D lean (verify): floor + anchor + holes are AS2 consumers PINNED as
stored, and the target is hot-zone-protected (W.2), so a mid-walk prune cannot select them — but show it
explicitly, per store, including the moment AFTER a hole is stored-and-pinned but BEFORE the next. Also decide:
prune after EACH store, or once at end of walk?

[#0002 | OD | r1 | partial]  Agree this is the highest-risk interaction, and verified it is LIVE: the recovery
walk stores floor and holes inside `cnr3_arAllFramesReady` (`cnr3_complete_live_recovery` :855; floor store
:1027; hole store :1221 via `store_recovery_plan_hole_owned_frame_and_record_pin`, which PINS the hole as
stored). So the floor/holes are pin-protected mid-walk exactly as W3D claims, and the target rests on hot-zone
protection (now confirmed for BOTH checkpoint and non-checkpoint cut targets — see SR-D-06). PARTIAL, not full
agree, because the finding leaves "per-store vs end-of-walk" as an open option and I do not think it is a free
choice under the SR-D-07 bar — I have raised that as SR-OD-02 with a recommended ruling. The SAFETY argument
(nothing the walk still needs can be selected) holds; the TIMING decision must be ruled before scope.

### SR-D-03   [severity: MAJOR]   [status: ACKNOWLEDGED]
Finding (W3D): WHERE does the combined helper live, and what does that do to the selftest count? (a) a
`cnr3_cache_core` public method — six-step atomicity/ordering provable by NEW cache-core selftest cases, count
RISES (54 -> 55+), live wiring is a thin call. (b) an arAllFramesReady-local orchestration — DLL-only, count
STAYS 54, proven only by the live harness. W3D lean: (a). CONFIRM feasibility (needs store/retire/prune
`_locked` members + pin_list, all on the class).

[#0003 | OD | r1 | agree]  Agree, lean (a). Verified the helper's needs are all on the class: the store/retire/
prune `_locked` members and the pin_list are all `Cnr3OutputCacheCore` members, so a public combined method
can compose them inside the class that owns `cache_mutex_`. (a) is right for two reasons: the six-step
atomicity and the SR-D-05 retire-before-select ordering deserve a selftest (a DLL-only orchestration could
only be proven by the live harness, which is a weaker bar for ordering); and keeping the critical section
inside the owning class is the cleaner lock discipline. The count rising (54→55+) is correct and expected for
new cache-core logic — same pattern as W.1. (See SR-OD-03 for what that selftest must specifically prove.)

### SR-D-04   [severity: MAJOR]   [status: ACKNOWLEDGED]
Finding (W3D): the PROOF SHAPE changes from W.2 (observe-only → byte-identity). W.3 changes cache STATE by
design, so byte-identity is NOT the test. W.3 proof must be: (1) every RETURNED frame byte-correct; (2)
eviction OBSERVABLY fires (KDT + victims detached, and/or selftest); (3) the just-produced target is NOT
evicted by its own activation's prune (W.2 interlock); (4) four-way still green (count per SR-D-03). Confirm
this is the right + sufficient proof contract.

[#0004 | OD | r1 | agree]  Agree — the four-part contract is right and sufficient, and the shift off
byte-identity is correct (eviction changes state by design, so "bytes identical to pre-W.3" would be a false
expectation). ONE ADDITION to the harness coverage, not a disagreement: the eviction-proof scenario must drive
the cache past BOTH triggers — capacity (§7.2) AND checkpoint-count (§7.4) — so both eviction paths fire live,
not just the capacity one. On cut-heavy content the checkpoint trigger can fire while total slots stay under
the capacity ceiling (the exact case W.1's §7.4 trigger exists for), so a capacity-only scenario would leave
the checkpoint-prune path live-unproven. W3D's harness should include a cut-dense segment for that.

### SR-D-05   [severity: MAJOR]   [status: ACKNOWLEDGED]
Finding (W3D): step 4 (retire) ordering + argument. Retirement must run BEFORE candidate selection within the
same lock, so a stale zone cannot wrongly shield a candidate; `execute_bounded_prune_pass_locked` does NOT
retire, so the combined helper must call `retire_decay_eligible_hot_zones_locked(current_frame)` first. DECIDE
the `current_frame` argument per call site in a multi-store activation.

[#0005 | OD | r1 | agree]  Agree, verified: `execute_bounded_prune_pass_locked` calls only
`cnr3_calculate_cache_prune_trigger_decision` then `select_composite_prune_candidates_bounded_locked` — there
is NO retire call inside it, and the selector reads current zone state. So the combined helper MUST call
`retire_decay_eligible_hot_zones_locked` explicitly before the prune pass, or a decay-eligible stale zone
wrongly shields a candidate. Confirmed as stated. The `current_frame` argument question W3D raises is real and
load-bearing enough that I have sharpened it into its own finding (SR-OD-01) — the rule must be pinned, not
left per-call-site-undecided.

### SR-D-06   [severity: MAJOR]   [status: RESOLVED — W3X confirmed (#0036)]
Finding (W3D): the hot-zone protection of the just-produced output[N] is CONFIRMED for a NON-checkpoint output,
but a DETECTED-CUT output[N] is stored as a CHECKPOINT (§6.4), so the non-checkpoint selector skips it as a
non-candidate and its protection then rests on the §7.4 CHECKPOINT-retention candidate path. CONFIRM the
checkpoint side of `select_composite_prune_candidates_bounded_locked` applies the SAME `frame_is_inside_hot_zone`
exclusion — else a cut-output[N] is a real SR-C-02 gap for cut frames and must be resolved before scope.

[#0006 | OD | r1 | agree]  CLOSED FAVOURABLY — I read the link W3D had not. In
`select_composite_prune_candidates_bounded_locked` the guard
`if (slot.pin_count != 0 || frame_is_inside_hot_zone_locked(slot.frame_number)) { continue; }` is applied
BEFORE the `if (slot.is_checkpoint)` branch — i.e. the hot-zone (and pin) exclusion is SHARED across both the
checkpoint and non-checkpoint candidate paths, not confined to the non-checkpoint selector. So a detected-cut
output[N], stored as a checkpoint and sitting inside its own W.2-observed hot zone, is skipped by that shared
guard regardless of its checkpoint flag, and is therefore NOT selectable by the §7.4 checkpoint-retention
prune during its own activation. SR-C-02's safety argument holds for cut frames as well as non-cut. This was
the one unread link in the Item-0 point-3 chain; it is now read and confirmed. Recommend W3X move SR-D-06 to
RESOLVED (the agreed decision: the composite selector's shared pin/hot-zone guard protects cut-output[N]
equally; no design change needed).

### SR-D-07   [severity: MAJOR]   [status: ACKNOWLEDGED]
Finding (W3X via intro; recorded by W3D): single-activation is the IMPLEMENTATION + VALIDATION scope, NOT a
licence to under-build. All pruning functionality must be VALIDLY ENABLED and correct NOW. Concurrency
(fmParallel) must add only validation + contention stress — NEVER new or changed code. `DEFERRED-to-fmParallel`
is legitimate only for concurrency validation/stress, never for prune functionality or a design choice that
would need reworking under concurrency. Reviewers must actively check for any spot where concurrency WOULD
force new/changed code.

[#0007 | OD | r1 | agree]  Agree with the sharpened bar. The all-under-one-`cache_mutex_` one-critical-section
shape (SR-D-01) satisfies it for the prune STEP itself — that mutation is already fully enabled, not stubbed.
The live exposure is the WIRING, and I found one spot that meets W3X's "would concurrency force a different
choice?" test: the multi-store prune TIMING (per-store vs end-of-walk) and the retirement `current_frame` rule
are choices that could be correct only because a single activation holds the lock uninterrupted across the
whole walk. I have raised those as SR-OD-02 (timing) and SR-OD-01 (current_frame) precisely so they are
settled NOW under this bar, not deferred. With those two pinned, I believe W.3 satisfies SR-D-07.

---

### SR-OD-01   [severity: MAJOR]   [status: OPEN]
Finding (W3OD): the `current_frame` argument passed to `retire_decay_eligible_hot_zones_locked` during a
recovery multi-store walk is the spot most likely to be subtly wrong, and it is an SR-D-07 spot (its
correctness must not depend on single-activation timing). Retirement retires a zone when
`(current_frame − last_observed_frame) >= DECAY_MARGIN (=20)` and no pinned frame is in range. In a recovery
activation that stores floor + many holes (e.g. floor 1950, holes 1951..1999, target 2000), if the combined
helper passes the JUST-STORED frame number as `current_frame` per store, the margin is computed against the
wrong reference and retirement decisions can diverge depending on store order. RULE I recommend: the
`current_frame` passed to retirement must be the **activation target N** (the frame whose getFrame is being
serviced), not the individual just-stored hole — so retirement reasons about decay relative to the actual
request wavefront, consistently across every store in the walk, and identically whether the prune fires
per-store or end-of-walk. This also makes the choice concurrency-stable (the reference is the activation's own
target, not a walk-position artefact). Verify from source what `last_observed_frame` holds for each candidate
zone at that point and pin the rule in the scope. Interacts with SR-D-05 (which raises the question) and
SR-OD-02 (timing).

[#0008 | OD | r1 | (finding raised)]  Raised in Round 1; awaiting W3D + W3C verdicts. My reasoning: SR-D-05
correctly requires retire-before-select but leaves "which frame" open; under SR-D-07 an open choice that could
give different retirement outcomes per store order is a current design flaw, so it must be pinned now. Target-N
is the rule that is both correct (decay relative to the real wavefront) and concurrency-stable.

### SR-OD-02   [severity: MAJOR]   [status: RESOLVED — W3X ruled per-store (#0037)]
Finding (W3OD): prune-per-store vs prune-at-end-of-walk (left open by SR-D-02) needs an explicit W3X ruling,
not an open option, because under SR-D-07 the two are not equivalent. Per-store: runs the selector N times in
one activation, each prune a small self-contained bounded critical section, pressure relieved incrementally,
lock-holds individually bounded, and the shape generalises cleanly to concurrency (each store's prune stands
alone). End-of-walk: runs the selector once (cheaper) but accumulates the whole walk's pressure before any
relief, requires the K-bound to absorb a larger detach set, and creates a single large prune whose timing
relative to other activations would be harder to reason about under fmParallel — i.e. it risks being an
SR-D-07 violation. W3OD lean: **per-store**, unless W3C finds a feasibility reason against it. Recommend W3X
rule on this before scope, since the scope's call-site wiring depends on it.

[#0009 | OD | r1 | (finding raised)]  Raised in Round 1; awaiting W3C feasibility view + W3X ruling. Reasoning
above; the deciding factor for me is SR-D-07 concurrency-stability, which per-store satisfies and end-of-walk
strains.

### SR-OD-03   [severity: MINOR]   [status: OPEN]
Finding (W3OD): the combined-helper selftest (SR-D-03 option a) must explicitly prove the retire-BEFORE-select
ordering in an OBSERVABLE outcome, not merely that eviction occurred. A selftest that only asserts "victims
detached" can pass even if retire ran after select, in any case where no zone happened to be decay-eligible.
Construct a scenario where a zone IS decay-eligible AND covers a frame that would otherwise be a prune
candidate, such that the returned victim set differs depending on retire/select order — so the ordering is
proven by the outcome (mirroring how W.1's scenario E proved the K-bound semantics rather than just "it
trimmed"). MINOR: a selftest-construction obligation for the scope, not a design gap.

[#0010 | OD | r1 | (finding raised)]  Raised in Round 1. Reasoning: ordering correctness (SR-D-05) is
load-bearing, so its selftest must be discriminating, not incidental — otherwise a future regression that
reorders retire/select could pass green.

---

## C. NON-EXHAUSTIVE WATCH-LIST (W3D Round 0 — probe; raise as SR-C-*/SR-OD-* if they harden)

- **frame_byte_count (SR-C-03 candidate):** computed ONCE, OUTSIDE the lock, conservative-HIGH, threaded to
  every combined-helper call in the activation. Confirm compute site (VS format/dims/subsampling) + reuse.
- **Pre-reserved working vectors:** `execute_bounded_prune_pass_locked` requires `candidate_order`,
  `checkpoint_candidate_order`, `selected_frame_numbers`, `detached_slots` pre-reserved to `max_remove_count`
  BEFORE the lock (contract: must not allocate under the lock). Confirm the public entry reserves all four.
- **AS2-consumer-vs-production mapping (step 3):** production stores must NOT pin (§4.1); AS2 consumers MUST
  pin. Confirm the helper's pin-or-not parameter maps 1:1 and nothing crosses over.
- **Cache-hit is exempt:** adds no slot → no pressure → does NOT route through the combined store-prune.
  Confirm a prune is not wrongly attached to cache-hit.
- **max_remove_count / K-bound:** what K per call site? Lock-hold must stay bounded.
- **Temporary KDT (SR-C-06 candidate):** prune/retire KDT (which triggers fired? victims detached? zones
  retired?) so the harness SEES eviction. Define fixed text now so the harness greps exactly what is emitted.
- **fmParallel:** single-activation is the validation scope, not a build shortcut (now SR-D-07 + SR-OD-01/02).
- **Harness shape:** W3D builds the eviction-proof harness (designer-owned), adapted from the golden 576p50
  catalogue, driving the cache PAST a trigger, `SetVideoCache(mode=0)`. [W3OD addition #0004: drive past BOTH
  capacity and checkpoint triggers — include a cut-dense segment.]

---

## D. STATE AFTER ROUND 1

- BLOCKERs: SR-D-01 (ACKNOWLEDGED, agreed by W3OD), SR-D-02 (PARTIAL — safety agreed, timing → SR-OD-02).
- MAJORs: SR-D-03/04/05/07 ACKNOWLEDGED (W3OD agreed, with additions noted); SR-D-06 RESOLVED pending W3X
  confirm; SR-OD-01/02 OPEN (need W3D + W3C verdicts and a W3X timing ruling); SR-OD-03 OPEN (MINOR).
- Still needed before scope: W3C round (feasibility / SR-C-* findings, esp. on SR-OD-02 timing and the
  watch-list reserve/byte-estimate items); W3X ruling on SR-OD-02 (per-store vs end-of-walk) and confirm of
  SR-D-06 RESOLVED; W3D verdicts on SR-OD-01/02/03.
- No wiring code this phase. On closure, W3D writes the W.3 coder scope + eviction-proof harness.

— End of register through Round 1 (W3OD appended).


---

## E. ROUND 2 — W3C APPEND

### HAND-OFF SUMMARY — W3C -> W3X, Round 2

W3C completed the implementation-feasibility / live-path / lifecycle / atomicity review against the committed W.2 source (`src(8).zip`, marker `CMS07-W.2-hot-zone-observation-arInitial`). I added W3C verdict comments on all SR-D findings, W3C verdict comments on SR-OD-01/02/03, and five SR-C findings from the watch-list.

Decision-relevant result: no W3C objection to W3OD's target-N retirement rule (SR-OD-01) or per-store prune timing (SR-OD-02). I agree those should be pinned before scope. I recommend W3X rule per-store for W.3 and confirm SR-D-06 RESOLVED. W3D most needs to look next at SR-C-01/02/03 because they shape the public helper signature: conservative frame-byte estimate, pre-reserved storage/no allocation under lock, and explicit production-vs-AS2 semantics without pulling VSAPI into the cache core.

Source references below are from the extracted committed W.2 source ZIP (`src(8).zip`) and use `file:line` form.

### W3C comments on existing SR-D findings

#### SR-D-01 comment

[#0011 | C | r2 | agree]  Agree. Source review confirms W.3 is buildable in the one-cache-lock shape and that a two-public-call shape would be the wrong implementation. The current live production helper `cnr3_store_live_output_frame_for_return` calls public `store_checkpoint_owned_frame` / `store_noncheckpoint_owned_frame` (`cnr3_arAllFramesReady.cpp:487-528`), and the recovery floor/hole sites call public AS2 store/pin helpers (`cnr3_arAllFramesReady.cpp:1011-1019`, `1219-1228`). Those public helpers each take their own lock (`cnr3_cache_core.cpp:642-693`, `697-743`, `746-791`), while the required W.3 shape needs store/adopt, optional AS2 pin, retire, and prune under one lock. The needed `_locked` primitives exist on `Cnr3OutputCacheCore`: store variants (`cnr3_cache_core.h:1216-1246`), retirement (`1175-1177`), and bounded prune execution (`1363-1372`). Therefore W.3 should add a cache-core public combined helper that reserves outside the lock, enters one `cache_mutex_` scope, calls the `_locked` primitives in §7.5 order, exits, then lets rejected/detached owned refs release outside the lock. The W.3 scope must also require a source grep / patch-review check that no old live store site remains on the old store-only public path.

#### SR-D-02 comment

[#0012 | C | r2 | partial]  Partial. I agree with the safety premise once SR-OD-01 and SR-OD-02 are pinned: recovery floor and holes are AS2 consumers and are pinned when stored (`cnr3_arAllFramesReady.cpp:1011-1019`, `1219-1228`; `store_owned_frame_and_record_pin_locked` pins even duplicate winners at `cnr3_cache_core.cpp:2028-2044`), while the recovery target N is protected by W.2's hot zone and the selector's shared pin/hot-zone guard (`cnr3_cache_core.cpp:2467-2497`). I do not agree that prune-per-store vs prune-at-end can stay open. From the live-path and future-fmParallel lens, per-store is the safer and more implementable shape: each store is followed by its own bounded retire/select/detach pass, each pass is capped by `CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS` (`cnr3_cache_core.h:145-148`), and the lock-hold remains bounded per store. End-of-walk would defer pressure relief across a potentially long recovery repair and would make a larger single prune timing decision depend on the activation's whole walk. W3C therefore agrees with W3OD's SR-OD-02 lean: rule per-store before scope.

#### SR-D-03 comment

[#0013 | C | r2 | agree]  Agree with option (a), with API-surface constraints raised as SR-C-02 and SR-C-03. The combined helper belongs in `Cnr3OutputCacheCore` because only the cache core can legally compose `_locked` store/retire/prune helpers while owning `cache_mutex_`; plugin-local orchestration would either call public lock-owning helpers sequentially or require exposing lock internals. The helper should be public and selftestable, and the selftest count should rise (54 -> 55+) because W.3 adds a new cache-core orchestration contract. The cache core must remain VSAPI-free: the live plugin may still create/adopt `Cnr3OwnedFrameRef` and handle authoritative return/duplicate-winner lookup, but the six-step store/retire/prune atomicity belongs in the cache core.

#### SR-D-04 comment

[#0014 | C | r2 | agree]  Agree. W.3 intentionally changes cache state, so W.2-style byte-identity against pre-W.3 is not an appropriate proof. The right proof is returned-frame byte correctness plus visible eviction and safety: target N not evicted by its own activation, recovery floor/holes not evicted while needed, cache-core selftests green, and live KDT/harness evidence that eviction actually fired. I also agree with W3OD's addition that the live harness must drive both capacity pressure and checkpoint-retention pressure. Source confirms W.1's trigger decision separately reports capacity and checkpoint triggers (`cnr3_cache_core.h:380-399`) and the locked prune pass proceeds when either trigger is active (`cnr3_cache_core.cpp:2582-2588`), so a capacity-only live scenario would leave the checkpoint-trigger path live-unproven.

#### SR-D-05 comment

[#0015 | C | r2 | partial]  Partial until SR-OD-01 is ruled, then agree. Source confirms `execute_bounded_prune_pass_locked` does not retire zones: it computes the trigger, checks early-return, counts hot-zone rejections if gated, selects candidates, and detaches (`cnr3_cache_core.cpp:2570-2643`), with no call to `retire_decay_eligible_hot_zones_locked`. Source also confirms retirement depends directly on the caller's `current_frame` argument: `current_frame - hot_zone.last_observed_frame >= CNR3_CACHE_HOT_ZONE_DECAY_MARGIN` (`cnr3_cache_core.cpp:1766-1800`). Therefore the combined helper must call `retire_decay_eligible_hot_zones_locked(current_frame)` before select/detach, inside the same lock. W3C agrees with SR-OD-01 that the `current_frame` value for every store in an activation must be the activation target N, not the just-stored frame. After that rule is pinned, SR-D-05 is implementable and should be treated as agreed.

#### SR-D-06 comment

[#0016 | C | r2 | agree]  Agree with moving SR-D-06 to RESOLVED. I independently checked the selector body. The shared guard `if (slot.pin_count != 0 || frame_is_inside_hot_zone_locked(slot.frame_number)) { continue; }` appears before the `if (slot.is_checkpoint)` branch (`cnr3_cache_core.cpp:2462-2472`). Therefore both checkpoint candidates and non-checkpoint candidates are excluded if they are pinned or inside any active hot zone. A cut output[N] stored as a checkpoint is still protected by its W.2-observed hot zone during the same activation. No design or code change is needed for this issue.

#### SR-D-07 comment

[#0017 | C | r2 | agree]  Agree with the sharpened bar. The implementation must not rely on "only one activation today" for correctness. From the source, the cache core already centralises cache mutation under one non-recursive `cache_mutex_`, and the `_locked` helpers are documented not to acquire it themselves (`cnr3_cache_core.h:1120-1127`). If W.3 uses a public combined helper that reserves outside the lock, uses one lock around steps 1-5, uses activation target N for retirement, and prunes per store with bounded K, then the same code path remains valid under fmParallel; later work becomes validation/contention stress, not a redesign. If W.3 instead chose just-stored-frame retirement or end-of-walk pruning, I would treat that as a current SR-D-07 design flaw rather than a deferrable fmParallel concern.

### W3C comments on SR-OD findings

#### SR-OD-01 comment

[#0018 | C | r2 | agree]  Agree. The source makes the bug shape concrete. W.2 observation writes `last_observed_frame = frame_number` when a zone slides or is created (`cnr3_cache_core.cpp:1721-1728`, `1743-1748`). Retirement then compares the caller's `current_frame` against that stored `last_observed_frame` (`cnr3_cache_core.cpp:1785-1798`). In a recovery activation for target N, passing a just-stored floor/hole frame would make retirement depend on repair-walk position rather than the real request wavefront. Passing activation target N is also straightforward to implement: the public combined helper can accept both `stored_frame_number` and `activation_target_frame` and pass only the latter to `retire_decay_eligible_hot_zones_locked`. That keeps per-store and end-of-walk outcomes consistent for retirement and is concurrency-stable.

#### SR-OD-02 comment

[#0019 | C | r2 | agree]  Agree with per-store, no W3C feasibility objection. The live store sites already occur as discrete operations: production target store via `cnr3_store_live_output_frame_for_authoritative_return` (`cnr3_arAllFramesReady.cpp:804-812`, `1361-1369`), recovery floor store (`1011-1019`), recovery hole store (`1219-1228`), and frame-0 store (`1540-1543`). Replacing each store with a combined store/retire/prune call is a direct mapping. Each prune pass is bounded by K (`cnr3_cache_core.h:145-148`) and the locked prune pass accepts pre-reserved vectors (`cnr3_cache_core.h:1355-1372`). Per-store therefore keeps lock-hold and eviction decisions local and bounded. I see no source-based feasibility reason to prefer end-of-walk; end-of-walk would require extra activation-level accumulation and would be harder to reason about under fmParallel. Recommend W3X rule per-store.

#### SR-OD-03 comment

[#0020 | C | r2 | agree]  Agree. The selftest must distinguish retire-before-select from select-before-retire. Source confirms the selector reads current hot-zone state (`cnr3_cache_core.cpp:2467-2497`) and retirement is separate (`1766-1810`), so a non-discriminating test could pass even if the helper accidentally called prune before retire. The W.3 cache-core selftest should construct a decay-eligible hot zone shielding a frame that would otherwise be the selected victim; after the combined helper, the expected victim must be the formerly shielded frame. If retire/select were reversed, that frame would remain shielded and the test would choose a different victim or no victim, making the ordering observable.

### New W3C findings

### SR-C-01   [severity: MAJOR]   [status: OPEN]
Finding (W3C): W.3 scope must define a single conservative `frame_byte_count` computation and require it to be computed once per activation outside the cache lock, then threaded unchanged into every combined-helper call in that activation. Source basis: `Cnr3FilterData` already stores immutable `video_info`, `bits_per_sample`, `sub_sampling_w`, and `sub_sampling_h` for the instance (`cnr3_plugin_internal.h:28-39`), and these are initialised from the source format during create (`vapoursynth-Cnr3.cpp:190-237`, `350-351`). The watch-list asks for conservative-HIGH and once-only. W3C recommendation: compute a checked `uint64_t` high estimate from the instance dimensions and storage bytes before the first store/prune call; using full-frame three-plane storage (`width * height * storage_bytes * 3`) is conservative for 4:2:0/4:2:2/4:4:4 and avoids underestimating cache pressure. The scope must specify overflow checks and failure handling. Do not compute this under `cache_mutex_`, and do not recompute per store.

[#0021 | C | r2 | (finding raised)]  This is raised because an underestimated byte count would delay capacity pruning, while a per-store recomputation risks inconsistency during one activation. Conservative-high once per activation is feasible from existing instance state and satisfies the watch-list without touching cache policy.

### SR-C-02   [severity: MAJOR]   [status: OPEN]
Finding (W3C): The public combined helper must pre-reserve every potentially-allocating structure before entering `cache_mutex_`, including AS2 pin-list capacity and all prune working vectors. Source basis: existing public `store_owned_frame_and_record_pin` reserves one pin before locking (`cnr3_cache_core.cpp:714-740`), and existing public `execute_bounded_prune_pass` reserves `candidate_order`, `checkpoint_candidate_order`, `selected_frame_numbers`, and `detached_slots` before locking (`cnr3_cache_core.cpp:1033-1075`). The new helper must combine those contracts: if it may record an AS2 pin, reserve pin-list capacity before the lock; reserve all four prune vectors to `max_remove_count` before the lock; then call only `_locked` helpers inside. If reservation fails, no cache mutation should have occurred.

[#0022 | C | r2 | (finding raised)]  This is raised because the one-lock design is only safe if it does not introduce allocation while holding the cache mutex. The existing primitives already encode the no-allocation discipline; W.3 must preserve it when composing them.

### SR-C-03   [severity: MAJOR]   [status: OPEN]
Finding (W3C): The combined-helper API must keep production-store and AS2-consumer-store semantics explicit, and must not pull VSAPI or authoritative-return logic into the cache core. Source basis: production target/frame-0 stores are no-pin stores (`cnr3_arAllFramesReady.cpp:487-528`, `1540-1543`), while recovery floor/hole stores are AS2 store-and-pin calls (`1011-1019`, `1219-1228`). The current authoritative-return helper handles VSFrame return ownership and duplicate-winner lookup in plugin code (`532-594`). W.3 should keep that layer boundary: the plugin prepares/adopts `Cnr3OwnedFrameRef` and handles return-frame decisions; the cache-core combined helper performs store/adopt/pin-if-AS2, retire, prune, and reports enough summary for the plugin to preserve duplicate and return behaviour. A single ambiguous bool is risky; use a clear store-kind/operation enum or distinct public helper overloads/wrappers so production never pins and AS2 always pins.

[#0023 | C | r2 | (finding raised)]  This is raised because crossing production and AS2 semantics would be a lifecycle bug: production output[N] must remain unpinned and rely on W.2 hot-zone protection, while AS2 floor/holes must be pinned for the recovery walk. It also protects the architectural boundary: cache core should not learn VSAPI return mechanics.

### SR-C-04   [severity: MAJOR]   [status: OPEN]
Finding (W3C): W.3 scope must include an explicit live call-site replacement checklist and a cache-hit exemption check. Source basis: existing live store paths are not centralised into one cache-core call site: production helper store selection at `cnr3_arAllFramesReady.cpp:487-528`, predecessor-present target call at `804-812`, recovery target call at `1361-1369`, recovery floor call at `1011-1019`, recovery hole call at `1219-1228`, and frame-0 store at `1540-1543`. Cache-hit (`cnr3_get_frame_live_cache_hit_return`, `597+`) returns an existing cached frame and does not add a slot, so it must not route through combined store-prune. Patch review must grep that all old live store-only public calls that should change are gone or wrapped, and that cache-hit remains store/prune-free.

[#0024 | C | r2 | (finding raised)]  This is raised because SR-D-01's atomicity can be correct in the helper but still fail in production if one legacy store path remains on the old append-only call. Conversely, attaching prune to cache-hit would add eviction pressure without a store and would be outside §7.5.

### SR-C-05   [severity: MINOR]   [status: OPEN]
Finding (W3C): The W.3 temporary KDT line must be fixed in the scope before patch and include enough fields for the designer harness to prove that eviction actually fired and which trigger(s) fired. Minimum W3C-recommended fields: instance, activation target N, stored frame number, store kind, store status, retire status, prune status, capacity trigger flag, checkpoint trigger flag, selected count, detached count. The KDT should be emitted after the combined helper returns, outside the cache lock, and should use `cnr3_status_name(...)` for statuses. Source basis: existing KDT helpers are plugin-side stderr traces guarded by `CNR3_KEYSTONE_DEV_TRACE` (`cnr3_arAllFramesReady.cpp:25-150`), while `Cnr3CachePruneExecutionSummary` already exposes trigger and detach counts (`cnr3_cache_core.h:412-419`).

[#0025 | C | r2 | (finding raised)]  This is minor because it is a scope/harness visibility requirement rather than a design gap, but it should be fixed before patch so the harness greps stable text and does not infer eviction indirectly.

### State after W3C Round 2

- W3C recommends SR-D-06 be confirmed RESOLVED by W3X.
- W3C recommends W3X rule SR-OD-02 as per-store pruning.
- W3C agrees SR-OD-01 target-N retirement and recommends the W.3 scope require both `stored_frame_number` and `activation_target_frame` as distinct inputs.
- New W3C findings before scope: SR-C-01/02/03/04 are MAJOR and should be answered in the W.3 coder scope; SR-C-05 is MINOR and should be answered in the harness/KDT section.
- No W3C production-code patch is proposed in this round.

— End of W3C Round 2 append.


---

## F. ROUND 3 — W3D APPEND (verdicts on the W3C round + the SR-OD findings)

### HAND-OFF SUMMARY — W3D -> W3X, Round 3

I verdicted every open finding against the committed W.2 source and ratified the one load-bearing link I had
owed. Decision-relevant result: the register is CONVERGED pending two W3X rulings and one formula refinement
I have fully specified — after those, all BLOCKER/MAJOR are AGREED/RESOLVED and I can write the W.3 coder
scope + eviction-proof harness.

- **SR-D-06 — RATIFIED RESOLVED on my own read** (#0026). I read `select_composite_prune_candidates_bounded_locked`
  (`cnr3_cache_core.cpp:2430`): the `pin || frame_is_inside_hot_zone` guard precedes the `is_checkpoint`
  branch, so cut-output[N] is protected equally. All three reviewers now confirm the point-3 chain. W3X: please
  formally confirm RESOLVED.
- **SR-OD-01 (current_frame = activation target N) — AGREE** (#0027); this also closes my own SR-D-05, which I
  move to AGREED (the open "which frame" is answered = target N).
- **SR-OD-02 (per-store vs end-of-walk) — AGREE per-store; my designer position is per-store** (#0028). W3X:
  please rule. (W3OD, W3C, W3D all lean per-store; no feasibility objection.)
- **SR-OD-03 (discriminating selftest) — AGREE, MINOR** (#0029); folded into the SR-D-03 selftest scope.
- **SR-C-02/03/04/05 — AGREE** (#0031-#0034). SR-C-03 (store-kind enum + VSAPI-free cache core) is the
  signature-shaping one; I have given the enum design intent.
- **SR-C-01 — PARTIAL** (#0030): agree the once/outside-lock/conservative-high/overflow-checked PRINCIPLE,
  but the `width*height*storage_bytes*3` formula over-estimates 2x on 4:2:0 (CNR3's primary PAL workload),
  which would roughly halve effective cache. Refinement specified: summed actual plane bytes. Not a redesign.

What W3X needs to do: (1) confirm SR-D-06 RESOLVED; (2) rule SR-OD-02 = per-store. What W3D needs from no one
else: the SR-C-01 formula is mine to set in the scope. Nothing else is open. On your two rulings I write the
scope.

(Round/comment numbering: I have labelled this Round 3 / comments #0026-#0034 following W3C's "Round 2"
internal label; the relay filename is r4. Canonical numbering is yours to reconcile.)

### W3D comments

#### SR-D-06 (ratify)

[#0026 | D | r3 | agree]  RATIFIED on my own read — I had flagged this as the one link I would not take on
others' reads. In `select_composite_prune_candidates_bounded_locked` (`cnr3_cache_core.cpp:2430`) the guard
`if (slot.pin_count != 0 || frame_is_inside_hot_zone_locked(slot.frame_number)) { continue; }` is applied to
every slot BEFORE the `if (slot.is_checkpoint)` branch, so a cut-output[N] sitting inside its own W.2-observed
active hot zone is skipped regardless of its checkpoint flag and is never offered to the §7.4
checkpoint-retention selection. Confirms exactly as W3OD (#0006) and W3C (#0016) found. The Item-0 point-3
chain is now end-to-end source-verified by all three reviewers. W3X may move SR-D-06 to RESOLVED.

#### SR-OD-01 (retirement current_frame = activation target N)

[#0027 | D | r3 | agree]  AGREE — and this answers the open question in my own SR-D-05. Retirement's margin
test `(current_frame − last_observed_frame) >= DECAY_MARGIN` needs a stable reference. The just-stored frame
would make decay a function of walk position/order — exactly the single-activation-only correctness SR-D-07
forbids. Activation target N is correct (decay relative to the real request wavefront) and concurrency-stable.
It also REINFORCES the W.2 interlock: arInitial observed N, so the zone covering N has `last_observed = N`;
`retire(current = N)` gives margin 0, so the just-produced output's protecting zone is never retired in its own
activation — which is precisely what keeps output[N] out of the step-5 candidate set. W3C's API shape (#0018:
helper takes both `stored_frame_number` and `activation_target_frame`, passes only the latter to retire) is the
right separation. I move my SR-D-05 to AGREED on this basis: the "current_frame argument" is settled = target N.

#### SR-OD-02 (per-store vs end-of-walk prune timing)

[#0028 | D | r3 | agree]  AGREE per-store; my designer position for the W3X ruling is per-store. Beyond
W3OD's/W3C's reasoning (which I concur with), the safety case for per-store is concrete: at the instant any
per-store prune runs inside a recovery walk, every dependency the walk still needs is protected — the anchor
is pinned (`plan_bounded_recovery_search_and_record_anchor_pin_locked`), the floor is pinned as stored, each
hole is pinned as it is stored (`store_recovery_plan_hole_owned_frame_and_record_pin`), holes not yet stored
are not in the cache so cannot be selected, and the target rests on its hot zone (SR-D-06). Those pins persist
until the activation's pin_list unwinds at end of getFrame, so no per-store prune within the walk can evict a
walk dependency. Per-store also satisfies SR-D-07 (each prune is a self-standing bounded critical section that
generalises to concurrency) and keeps each lock-hold K-bounded. On the K watch-list item: per-store growth is
+1 slot per store, so any K >= 1 keeps pace; `CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS` gives ample headroom, so a
single bounded pass per store self-keeps-up with per-store pressure — no multi-pass debounce needed within an
activation. End-of-walk accumulates the whole walk's pressure and makes one large prune whose cross-activation
timing is harder to reason about under fmParallel — it strains SR-D-07. Recommend W3X rule per-store.

#### SR-OD-03 (discriminating retire-before-select selftest)

[#0029 | D | r3 | agree]  AGREE, MINOR — correct bar, folded into the SR-D-03 selftest scope. The new
cache-core selftest must construct a decay-eligible zone shielding a frame that would otherwise be the selected
victim, so the correct retire-before-select order makes that frame the victim while the wrong order leaves it
shielded (a different victim, or none) — the ordering is proven by the OUTCOME, not incidentally passed when no
zone happens to be decay-eligible. Same discipline as W.1 scenario E. As selftest/harness owner via the scope,
I will specify this scenario explicitly (decay-eligible zone + otherwise-prunable shielded frame + asserted
victim identity).

#### SR-C-01 (once-computed conservative frame-byte estimate)

[#0030 | D | r3 | partial]  PARTIAL (honest-agree). I AGREE with the principle — compute once per activation,
OUTSIDE the lock, conservative-high, overflow-checked (`uint64_t`), threaded unchanged into every combined-helper
call. I DO NOT agree with the specific `width * height * storage_bytes * 3` formula. Full-three-plane
over-estimates by 2x on 4:2:0 (Y full + two QUARTER-size chroma planes = 1.5 planes of actual storage, not 3)
and by 1.5x on 4:2:2. CNR3's primary workload is PAL 4:2:0 720x576, so the 2x over-estimate would roughly HALVE
`active_ceiling` and therefore halve the effective cache before the §7.2 capacity prune fires — safe for memory
(over-estimate never under-protects) but a real cache-effectiveness cost (more recompute/recovery) on exactly
the main workload. Refinement (scope will specify): compute the actual summed plane bytes — sum over planes of
`plane_width * plane_height * bytes_per_sample`, derived from the instance's stored `sub_sampling_w/h` and
`bits_per_sample` (`cnr3_plugin_internal.h`) — still exact-or-conservative, without the 2x penalty. This is a
formula refinement, not a redesign; everything else in SR-C-01 stands.

#### SR-C-02 (pre-reserve pin-list + all four prune vectors before the lock)

[#0031 | D | r3 | agree]  AGREE — and it is load-bearing for SR-D-07: any allocation under `cache_mutex_` is
exactly the single-activation-only shortcut SR-D-07 forbids. The combined helper's public entry must COMBINE
both existing reservation contracts before the one lock: reserve one pin-list entry IF the store is an AS2
consumer (as public `store_owned_frame_and_record_pin` does), AND reserve
`candidate_order`/`checkpoint_candidate_order`/`selected_frame_numbers`/`detached_slots` to `max_remove_count`
(as public `execute_bounded_prune_pass` does); then call only `_locked` helpers inside. The scope states this as
a hard requirement with the failure rule: if any reservation fails, no cache mutation has occurred.

#### SR-C-03 (explicit production/AS2 store-kind; VSAPI-free cache core)

[#0032 | D | r3 | agree]  AGREE — the most valuable new finding; it shapes the helper's public signature. I
confirmed the two-layer structure from source: `cnr3_store_live_output_frame_for_authoritative_return` (:532,
the plugin-side wrapper owning authoritative-return + duplicate-winner) calls
`cnr3_store_live_output_frame_for_return` (:487, the inner cache store); live sites at :804/:1361 call the
wrapper. The combined cache-core helper replaces the CACHE operation only; the authoritative-return /
duplicate-winner / VSFrame-ownership layer stays in plugin code and the cache core stays VSAPI-free. Design
intent for store-kind: NOT a single ambiguous bool. Use an explicit `Cnr3CacheStoreKind` enum naming the valid
combinations over the two orthogonal axes (is_checkpoint × should_pin): production-noncheckpoint (no pin),
production-checkpoint (no pin), AS2-consumer-noncheckpoint (pin), AS2-consumer-checkpoint (pin). Each call site
names its kind; the helper cannot be mis-called into pinning a production frame (§4.1 violation) or failing to
pin an AS2 consumer; the mapping is self-documenting. The scope defines the enum and the per-site mapping.

#### SR-C-04 (call-site replacement checklist + cache-hit exemption)

[#0033 | D | r3 | agree]  AGREE. The scope carries an explicit checklist of every live store site that must
route through the combined helper — production target via the authoritative-return wrapper (:804, :1361),
recovery floor (:1011-1019), recovery hole (:1219-1228), frame-0 (:1540-1543) — plus a patch-review grep that
no old store-only public call remains (this discharges W3OD's #0001 read-first obligation), and an explicit
statement that cache-hit (`cnr3_get_frame_live_cache_hit_return`, :597+) adds no slot, so it routes through
NEITHER store nor prune NOR retire (retirement is lazy and rides the store-prune; a pure cache-hit activation
correctly performs no retirement, and zones are retired on the next store activation). Attaching a prune to
cache-hit would be outside §7.5 and is forbidden.

#### SR-C-05 (fix KDT text + fields in scope)

[#0034 | D | r3 | agree]  AGREE, MINOR. As harness owner I will fix the EXACT KDT line in the scope (stable
grep tokens — the W.2 grep-token lesson), with W3C's field set: instance, activation target N, stored frame
number, store kind, store status, retire status, prune status, capacity-trigger flag, checkpoint-trigger flag,
selected count, detached count — emitted AFTER the combined helper returns and OUTSIDE the lock, statuses via
`cnr3_status_name`, guarded by `CNR3_KEYSTONE_DEV_TRACE`. The harness greps these tokens to prove eviction
fired and which trigger(s) fired. `Cnr3CachePruneExecutionSummary` (h:412-419) supplies the counts/flags.

---

## G. STATE AFTER ROUND 3

- **BLOCKERs:** SR-D-01 AGREED (all three). SR-D-02 — safety AGREED (all three); timing now resolvable via the
  SR-OD-02 ruling.
- **MAJORs:** SR-D-03 AGREED (option a, count rises, selftest per SR-OD-03); SR-D-04 AGREED (proof contract +
  both-trigger harness coverage); SR-D-05 AGREED (current_frame = target N, per SR-OD-01); SR-D-06 RESOLVED
  pending W3X formal confirm (now ratified by W3D's own read); SR-D-07 AGREED (satisfied once SR-OD-01/02 are
  pinned); SR-OD-01 AGREED (target N); SR-OD-02 AGREED-per-store pending W3X ruling; SR-C-02/03/04 AGREED;
  SR-C-01 PARTIAL (principle agreed; formula refined to summed plane bytes — W3D resolves in scope).
- **MINORs:** SR-OD-03 AGREED (discriminating selftest); SR-C-05 AGREED (KDT text — W3D fixes in scope).
- **Open and owned by W3X (the only blockers to scope):** (1) confirm SR-D-06 RESOLVED; (2) rule SR-OD-02 =
  per-store. No other finding is open.
- **On those two rulings, W3D writes:** the W.3 coder build scope (the combined helper as a `Cnr3OutputCacheCore`
  public method; the `Cnr3CacheStoreKind` enum + per-site mapping; the summed-plane byte estimate; pre-reserve
  contract; the §7.5 six-step order with retire(target-N) before per-store prune; the call-site checklist +
  cache-hit exemption; the fixed KDT line) and the designer-owned eviction-proof harness (golden 576p50 +
  `SetVideoCache(mode=0)`, driving past BOTH the capacity and checkpoint triggers incl. a cut-dense segment,
  proving returned-frame correctness + observable eviction + target-N-not-evicted + the discriminating
  retire-before-select selftest).
- No wiring code this phase.

— End of register through Round 3 (W3D appended).


---

## H. INTERIM (W3D <-> W3X exploration, post-Round-3) — finding raised before the next reviewer trip

*(This section holds findings raised during W3X's pre-trip review of the Round-3 register, before the next
W3OD/W3C round-trip. Comment #0035 continues the global sequence; W3X reconciles canonical numbering.)*

### SR-D-08   [severity: MAJOR — concurrency-safety; makes SR-D-07 concrete for the combined helper]   [status: OPEN]
Finding (W3D, raised at W3X's request from a concurrency review of SR-D-01): the combined helper's correctness
rests on the ONE-CRITICAL-SECTION property — the entire `store -> set-flag -> pin-if-AS2 -> retire ->
evaluate-trigger -> decide/detach` sequence executes under a SINGLE, UNINTERRUPTED hold of `cache_mutex_`, so
no parallel activation can read or mutate the cache between any two dependent steps. This is the exact
protection against the check-then-act / gap-between-two-separate-locks bug class (a later step assuming an
earlier step's result still holds, when a parallel op changed it in the gap) — and it is precisely the shape
SR-D-01 mandates and the two-public-call shape SR-D-01 forbids. The design gives this property ONLY IF all of
the following hold; each is a PATCH-REVIEW check, not an assumption:

1. **One hold, no gap.** The helper acquires `cache_mutex_` ONCE at its inner boundary and holds it across
   steps 1-5; no inner step releases or re-acquires it. The varying quantity the trigger tests (live slot /
   checkpoint count) is read INSIDE this hold, immediately before the prune acts on it — check and act under
   the same hold (no TOCTOU on the trigger).
2. **`_locked`-only inside the hold.** Every operation inside the scope is a `_locked` primitive
   (caller-holds-lock, does not itself lock); the helper calls NO public lock-owning method inside the scope.
   `cache_mutex_` is non-recursive (h:616-627), so an accidental public re-entry DEADLOCKS (caught in test),
   not silently nests — the documented discipline at h:1118-1127. Patch-review grep: only `_locked` names
   appear between lock and unlock.
3. **Call-local working state.** The four prune vectors and any scratch are CALL-LOCAL (per-activation stack),
   reserved BEFORE the lock; never shared cache members — so concurrent activations cannot race on them
   (matches existing `execute_bounded_prune_pass`, whose vectors are function locals). Extends SR-C-02 with the
   local-not-shared requirement.
4. **Outside-lock work touches only immutable/local state.** The conservative byte-estimate (immutable
   instance format/dims, fixed at create-time) and the pre-reservation (local vectors) are the only
   outside-lock work; neither reads or writes shared cache state, so nothing the helper depends on is computed
   in a way that leaves a gap on shared state.
5. **Free after unlock.** `freeFrame` on detached victims happens ONLY after the lock is released (step 6;
   h:45 / §8.2). Detached slots are moved into the call-local vector under the lock and freed post-lock. This
   also prevents a lock-ordering deadlock between `cache_mutex_` and VapourSynth's internal locks (no VSAPI
   call is ever made while `cache_mutex_` is held).
6. **No new synchronization machinery.** No `std::atomic` / lock-free construct is introduced; the sole
   synchronization point remains the single non-recursive `cache_mutex_`.

This finding does NOT change SR-D-01's decision; it makes the concurrency guarantee SR-D-01 relies on explicit
and verifiable, and consolidates checks that were previously scattered across SR-D-01 / SR-C-02 / SR-D-07.
Items 1-2 are the load-bearing safety core (the no-gap, single-hold property); 3-6 are the conditions that
keep it true.

[#0035 | D | r3x | (finding raised)]  Raised at W3X's request following a concurrency review of SR-D-01.
Reasoning: SR-D-01 mandates the one-critical-section shape and SR-D-07 sets the concurrency bar, but the
specific checks GUARANTEEING that no parallel activation can interleave between dependent steps were
implicit/scattered. This consolidates them into a verifiable patch-review checklist that directly answers the
gap-between-two-locks (TOCTOU) bug class. Awaiting W3C + W3OD verdicts on the next trip; W3D expects all six to
verify cleanly against source (the cache-core's existing discipline already encodes them), but they must be
ticked, not assumed.

### W3X interim rulings

[#0036 | X | r3x | ruling]  SR-D-06 — CONFIRMED RESOLVED by W3X. All three reviewers independently read the
shared pin/hot-zone guard in `select_composite_prune_candidates_bounded_locked` (`:2430`, applied before the
`is_checkpoint` branch) and confirmed a cut-output[N] inside its own W.2 hot zone is protected equally with a
non-checkpoint output; no design or code change needed. Status: RESOLVED. (SR-OD-02 still awaits a W3X ruling;
not ruled here.)

[#0037 | X | r3x | ruling]  SR-OD-02 — RULED **PER-STORE** by W3X. The combined store-and-prune helper runs its
retire + trigger + decide/detach (steps 4-5) after EACH store, not once at end-of-walk. Rationale: per-store is
the concurrency-stable shape — each store+prune is a self-contained bounded critical section, satisfying SR-D-07
— keeps each lock-hold K-bounded, and relieves pressure incrementally; the recovery walk's own dependencies are
pin- or hot-zone-protected at every per-store prune (SR-D-02 safety), so no mid-walk prune can evict a frame the
walk still needs. All three reviewers leaned per-store; W3C found no feasibility objection. Consequences:
SR-OD-02 → RESOLVED; SR-D-02's open timing half closes (safety already agreed three-way) → SR-D-02 RESOLVED;
and SR-D-07's remaining contingency is satisfied (both SR-OD-01 target-N and SR-OD-02 per-store now pinned).

— End of register through interim W3D <-> W3X exploration (SR-D-08 raised; SR-D-06 confirmed RESOLVED;
SR-OD-02 ruled per-store → SR-D-02 closed).


---

## I. ROUND 4 — W3OD APPEND (verdicts on the W3C round + SR-D-08; closing the register)

### HAND-OFF SUMMARY — W3OD -> W3X, Round 4
W3OD verdicted the six findings not yet seen on the design side — SR-C-01..05 and SR-D-08 — each against the
committed W.2 source (file:line cited). Decision-relevant result: the register CLOSES. All six verify cleanly;
SR-C-01 is partial only in that W3OD endorses W3D's summed-plane refinement over the original ×3 formula (same
conclusion W3D reached). With W3X's two interim rulings already recorded (#0036 SR-D-06 RESOLVED, #0037 SR-OD-02
per-store), every BLOCKER and MAJOR is now AGREED/RESOLVED across all three reviewers. No finding remains in
dispute, none awaits W3X, none is deferred. W3D may write the W.3 coder scope + eviction-proof harness.
What W3D most needs to carry into the scope (from this round): the SR-C-03 `Cnr3CacheStoreKind` enum as the
helper's public signature; the SR-C-01 summed-plane byte estimate (not ×3); the SR-D-08 six-point
concurrency checklist as an explicit patch-review gate; SR-C-02's combined pre-reserve contract; SR-C-04's
call-site checklist + cache-hit exemption; SR-C-05's fixed KDT text.

### W3OD comments on the W3C findings

[#0038 | OD | r4 | partial]  **SR-C-01** — agree the principle (compute once, outside the lock, conservative-
high, overflow-checked `uint64_t`, threaded unchanged), and I endorse W3D's refinement (#0030) over W3C's
original ×3 formula. Verified the arithmetic against source: the instance stores `sub_sampling_w/h` and
`bits_per_sample` (`cnr3_plugin_internal.h:34-36`), so the summed-plane estimate is computable from existing
state. On 4:2:0 (CNR3's primary PAL 720x576 workload) the luma plane is full-size and the two chroma planes
are quarter-size each, so total storage = 1.5 plane-equivalents, not 3 — W3D is right that ×3 over-estimates
2× and would roughly halve the effective cache before the §7.2 capacity prune fires. Over-estimation is
memory-safe but a real cache-effectiveness cost on the main workload, so the refinement matters. PARTIAL only
in the W3C-vs-W3D formula sense; the resolved position (summed actual plane bytes) is agreed by D and OD.
Still conservative-or-exact, still overflow-checked. W3D owns the exact formula in the scope.

[#0039 | OD | r4 | agree]  **SR-C-02** — agree, and load-bearing for SR-D-07/SR-D-08. Verified both existing
reservation contracts in source: `pin_list.reserve_for_additional_pins(1U)` before the lock
(`cnr3_cache_core.cpp:714`) and the four prune vectors reserved before the lock (`:1048-1050` + the
`detached_slots.reserve(max_remove_count)` sites). The combined helper must MERGE both before its single lock:
reserve one pin entry IF the store is an AS2 consumer, AND reserve all four prune vectors to `max_remove_count`,
then call only `_locked` inside. Any allocation under `cache_mutex_` is exactly the single-activation-only
shortcut SR-D-07 forbids, so this is not just hygiene — it's a concurrency-correctness requirement. Agreed with
the failure rule W3D stated (#0031): on any reservation failure, no cache mutation has occurred.

[#0040 | OD | r4 | agree]  **SR-C-03** — agree; the most signature-shaping finding, and the `Cnr3CacheStoreKind`
enum is the right resolution. Verified the two-layer structure from source: the plugin wrapper
`cnr3_store_live_output_frame_for_authoritative_return` (`cnr3_arAllFramesReady.cpp:532`) owns the
authoritative-return/duplicate-winner/VSFrame layer and calls the inner cache store
`cnr3_store_live_output_frame_for_return` (`:487`); live sites at `:804`/`:1361` call the wrapper. The combined
cache-core helper replaces the CACHE operation only — the VSAPI/return layer stays in plugin code and the
cache core stays VSAPI-free (the existing helpers already document "no addFrameRef/freeFrame" — e.g.
`cnr3_cache_core.cpp:996`, `:1063`). W3D's enum over the two orthogonal axes (is_checkpoint × should_pin) with
the four named kinds is better than a bool: it makes "production never pins (§4.1) / AS2 always pins"
un-mis-callable. Agreed.

[#0041 | OD | r4 | agree]  **SR-C-04** — agree. Verified the live store sites are NOT centralised (production
via the authoritative-return wrapper at `:804`/`:1361`, recovery floor `:1011-1019`, recovery hole
`:1219-1228`, frame-0 `:1540-1543`) and that cache-hit is a distinct return path that adds no slot
(`cnr3_get_frame_live_cache_hit_return` `:597`, dispatched at `:1601`). So the scope's explicit call-site
replacement checklist + the patch-review grep that no old store-only public call remains is exactly right (it
also discharges my own #0001 read-first obligation), and the cache-hit exemption is correct: it routes through
neither store nor prune nor retire (retirement is lazy and rides the store-prune; a pure cache-hit activation
correctly retires nothing, and zones retire on the next store activation). Attaching a prune to cache-hit would
be outside §7.5. Agreed.

[#0042 | OD | r4 | agree]  **SR-C-05** — agree, MINOR. The fixed KDT text with W3C's field set (instance,
target N, stored frame, store kind, store/retire/prune status, capacity + checkpoint trigger flags, selected
count, detached count), emitted after the helper returns and outside the lock, statuses via `cnr3_status_name`,
guarded by `CNR3_KEYSTONE_DEV_TRACE`, is the right visibility contract — and `Cnr3CachePruneExecutionSummary`
(`cnr3_cache_core.h:412-419`) supplies the counts/flags so the helper can report them without new plumbing.
W3D owns the exact grep tokens in the scope (the W.2 grep-token lesson). Agreed.

### W3OD comment on SR-D-08

[#0043 | OD | r4 | agree]  **SR-D-08** — agree; all six items verify against source, and consolidating the
concurrency guarantee into a patch-review checklist is the right move (it makes SR-D-01's one-critical-section
property checkable rather than assumed). Verified the two load-bearing items directly: (item 2) `cache_mutex_`
is `std::mutex`, explicitly NON-recursive with a source comment forbidding `std::recursive_mutex` because it
"would hide" re-entry (`cnr3_cache_core.h:1528-1531`) — so an accidental public re-entry inside the hold
DEADLOCKS and is caught in test, not silently nested; (item 5) the §8.2 "no `freeFrame()` while `cache_mutex_`
is held" rule is explicit in source (`:45`, `:1112`), with detached refs released only after unlock
(`:728`, `:789`, `:876`, `:896`, `:953`) — which is also what prevents a `cache_mutex_`↔VapourSynth
lock-ordering deadlock. Items 1 (one hold, no gap — trigger read inside the same hold as the prune that acts on
it: no TOCTOU), 3 (call-local working vectors, not shared members — matches existing `execute_bounded_prune_pass`
locals), 4 (outside-lock work touches only immutable instance state + local vectors), and 6 (no new
sync machinery; the sole sync point stays the one non-recursive mutex) all follow from the cache core's
existing documented discipline and the SR-C-02 pre-reserve contract. SR-D-08 does not change SR-D-01's
decision; it hardens it. Agreed — the six items go into the scope as a patch-review gate, ticked not assumed.

### W3OD closing verdict

[#0044 | OD | r4 | agree]  **Register CLOSED from the W3OD seat.** Every BLOCKER and MAJOR is AGREED or
RESOLVED across all three reviewers, with W3X's two rulings recorded (#0036 SR-D-06 RESOLVED, #0037 SR-OD-02
per-store). Nothing is in dispute, awaiting W3X, or deferred. The §7.5 design is source-verified end to end:
the combined helper composes proven `_locked` primitives under one non-recursive lock (SR-D-01/SR-D-08); the
W.2 observation gates the prune for both checkpoint and non-checkpoint output (SR-D-06); retirement uses
activation target N before a per-store prune (SR-OD-01/SR-OD-02); the proof bar is returned-frame correctness
+ observable both-trigger eviction + target-not-evicted + discriminating ordering selftest (SR-D-04/SR-OD-03);
and the API/lifecycle constraints are pinned (SR-C-01..05). W3D may write the W.3 coder scope + eviction-proof
harness. No wiring code was written in this review.

— End of register through Round 4 (W3OD appended; register CLOSED pending W3D scope authorship).

---

## J. ROUND 5 — W3C FINAL APPEND (SR-D-08 verdict + last-gasp check)

### HAND-OFF SUMMARY — W3C -> W3X/W3D, Round 5

W3C performed the requested final coder-side trip against the committed W.2 source (`src(8).zip`, marker `CMS07-W.2-hot-zone-observation-arInitial`). I verdicted the only outstanding item, SR-D-08, and performed a last-gasp scan of the closed register for source/lifecycle/lock-discipline contradictions before W3D writes the W.3 scope.

Decision-relevant result: W3C AGREES with SR-D-08. The six-point checklist is the correct patch-review gate for the combined helper's concurrency safety. Source verifies the underlying cache-core discipline it depends on: one non-recursive `cache_mutex_`, public outer-lock / internal `_locked` split, pre-reserved local vectors before lock entry, immutable/local outside-lock work, detached/free-after-unlock, and no new synchronization machinery. Because the W.3 combined helper does not exist yet, these are not a claim that the future patch already satisfies them; they are a scope/patch-review gate that must be ticked on the W.3 patch.

Last-gasp result: I found no new W3C blocker or major issue. My earlier SR-C-01 `width * height * storage_bytes * 3` suggestion is superseded by W3D/W3OD's summed-plane refinement; I accept that refinement as the correct scoped result. No new SR-C finding is raised.

### W3C comment on SR-D-08

[#0045 | C | r5 | agree]  **SR-D-08** — agree. This is the right concurrency-safety checklist for the W.3 combined helper, and it is in W3C's lane because it turns SR-D-01/SR-D-07 into concrete implementation and patch-review conditions. I verified the checklist against the committed W.2 source as follows. Items 1 and 2 are supported by the existing lock model: `Cnr3OutputCacheCore` has exactly one `std::mutex`, public lock-owning operations acquire it at their outer boundary, and internal helpers that already hold the mutex must call `_locked` helpers rather than public observers/helpers (`cnr3_cache_core.h:614-629`, `1118-1127`, `1519-1531`). That makes the required W.3 shape one uninterrupted lock hold across steps 1-5, with no public lock-owning method called inside it. Item 3 is supported by the existing bounded prune public entry, which creates the prune vectors as function-local scratch, reserves them before lock entry, and passes them into `execute_bounded_prune_pass_locked` (`cnr3_cache_core.cpp:1033-1051`, `1063-1075`); the locked helper's contract also states that those vectors must have enough reserved capacity before entry and that detached slots are released after the public helper exits the lock scope (`cnr3_cache_core.h:1355-1372`). Item 4 is consistent with the closed SR-C-01/SR-C-02 shape: the byte estimate is computed from immutable instance format/dimensions outside the cache lock, and reservation work touches only local vectors/pin-list capacity, not shared cache state. Item 5 is directly supported by the existing free-after-unlock discipline: the header states no `freeFrame()` while `cache_mutex_` is held (`cnr3_cache_core.h:43-46`, `1108-1113`), and existing store/prune paths deliberately nest lock scopes so rejected/detached owned refs release after unlock (`cnr3_cache_core.cpp:656-664`, `683-691`, `722-741`, `1059-1078`). Item 6 is also consistent with the source: there is no existing lock-free/atomic cache-state scheme for W.3 to extend, and the design keeps the sole synchronization point as the non-recursive `cache_mutex_` rather than adding `std::atomic` or secondary locks. Agreement is conditional only in the ordinary patch-review sense: because the W.3 combined helper is not implemented yet, the future patch must explicitly satisfy all six items; SR-D-08 is not a new design change, it is the required checklist proving the patch preserves the cache core's existing discipline.

### W3C last-gasp comment

[#0046 | C | r5 | agree]  **Last-gasp W3C review** — no new blocker, major, or minor finding raised. I re-read the closed register's current decisions against the committed W.2 source shape relevant to W3C's lane: combined helper as a `Cnr3OutputCacheCore` public method, one lock across §7.5 steps 1-5, per-store prune, `retire(current_frame = activation target N)`, explicit `Cnr3CacheStoreKind`, pre-reserve-before-lock, call-local prune vectors, plugin-side VSAPI/authoritative-return boundary, call-site replacement checklist, cache-hit exemption, and fixed KDT visibility. I found no source contradiction requiring the register to reopen. The one W3C-origin item I would clarify is already resolved in the register: SR-C-01's original `width * height * storage_bytes * 3` proposal was too conservative for 4:2:0/4:2:2 cache effectiveness and is superseded by W3D/W3OD's summed-plane byte estimate; W3C accepts that refined formula for scope. No new SR-C finding is raised. W3D may proceed to write the W.3 coder scope with SR-D-08 included as a mandatory patch-review checklist.

### State after W3C Round 5 final append

- SR-D-08 now has W3D + W3OD + W3C agreement.
- W3C raises no last-gasp finding.
- No blocker, major, or minor item remains open from W3C's perspective.
- W3D may write the W.3 coder scope and designer-owned eviction-proof harness from the closed register.

— End of W3C Round 5 final append.

