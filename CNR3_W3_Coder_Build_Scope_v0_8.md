# CNR3 W.3 — Coder Build Scope (v0.8, by W3D)

**Status:** v0.8 — folds W3C's v0.7 implementation-clarity item: the summary-population guardrail now applies at
the PUBLIC-WRAPPER level (not only the shared impl), so a wrapper-level early return — recovery-hole guard
failure, `frame_byte_count == 0`, or pre-reservation failure, all of which exit before the shared impl runs —
still produces a fully diagnosable per-invocation KDT line. No design change; W3C: "no design-level
reservations." **Coder-ready for patch proposal.** No wiring committed until the coder's patch is reviewed
against source and passes four-way.

### Changes in v0.8 (W3C v0.7 review)
- **Wrapper-level early-summary init** → §C guardrail a (+ §A.2 hole-guard note): each public wrapper sets
  `store_kind`, `stored_frame_number`, `activation_target_frame`, and the early-failure `store_status` before
  ANY return, including wrapper-level guard / validation / reservation-failure exits that precede the shared
  impl. Generalised from the hole-guard case to all wrapper-level early returns.

**Baseline (verified):** `CNR3_EDIT_VERSION = "CMS07-W.2-hot-zone-observation-arInitial"` (`cnr3_build_config.h:25`),
four-way 54/54. CMS07.14 controlling. All line references are against this committed W.2 tree.

### Changes in v0.6b (W3C unknown-unknown cross-check, on top of v0.6/v0.6a)
- **By-value RESOLVED** → §A.2 cross-check box: confirmed correct on all of (a)-(e); source forbids by-reference
  (`:722-729`).
- **AS2 duplicate normalization** → §A.2 / §E.4: `store_owned_frame_and_record_pin_locked` returns `ok` (not
  `duplicate`) on an AS2 duplicate (`:1998-2003`, `:2074-2081`), recording it only in
  `as2_summary.duplicate_existing_slot`. AS2 wrappers MUST normalize `store_status` from the sub-summary, not
  the helper return, or AS2 duplicate visibility is silently lost.
- **Summary-population guardrails** → §C: identity fields set before early returns; production leaves
  `as2_summary` default; AS2 requires `pin_recorded` before retire/prune; retire-ok/prune-fail returns the
  prune hard status with all statuses preserved.
- **AS2 asymmetry blast-radius check** → §A.2: source-swept — normalization localized to one function, two
  existing consumers (both already correct, both in W.3 scope), no pre-existing latent bug; the four
  `duplicate`-branching sites are all production-path. Plus a recommended CMS contract note (§H) to protect
  planned consumers.

**Baseline (verified):** `CNR3_EDIT_VERSION = "CMS07-W.2-hot-zone-observation-arInitial"` (`cnr3_build_config.h:25`),
four-way 54/54. CMS07.14 controlling. All line references are against this committed W.2 tree.

**Baseline (verified):** `CNR3_EDIT_VERSION = "CMS07-W.2-hot-zone-observation-arInitial"` (`cnr3_build_config.h:25`),
four-way 54/54. CMS07.14 controlling. All line references are against this committed W.2 tree.

### Prior changes — v0.7 (wording), v0.6b (unknown-unknown), v0.6 (third-pass), v0.5 (second-pass), v0.4 (SCOPE-01..10)
- v0.7: live-code footprint clarified (§A.2); fuller CMS note (§H).
- v0.6b: by-value RESOLVED by source (`:722-729`); AS2 duplicate normalization (§A.2/§E.4); summary-population
  guardrails (§C); AS2 asymmetry blast-radius check (§A.2) + CMS note (§H).
- v0.6: by-value wrappers; `Invalid` kind + non-ok summary defaults; E.4 refcount accounting.
- v0.5: wrapper return-status contract; E.4 two-layer ownership; allocation-bar scoping.
- v0.4: concrete three-wrapper API + result summary; plugin-side byte estimate; V1 hole-guard preserved;
  state-based ownership; cache-core-local detached-victim RAII; live constants; KDT from the summary;
  edit-version bump. (Full per-item map in §I.)

**Authority chain:** CMS07.14 §7.4/§7.5/§7.6 → closed r8 register → this scope → coder patch. Every decision
traces to a closed finding (§I); W3C-SCOPE resolutions are cited inline.

### Changes in v0.4 (what each W3C-SCOPE item resolved)
- **01** → §A.2: concrete public API + new `Cnr3CombinedStoreAndPruneSummary` result struct; `ok`/`duplicate`
  both proceed.
- **02** → §A.4: byte estimate computed PLUGIN-SIDE once per activation and passed in; cache-core validates
  `!= 0`, does not compute; ceil-shift chroma dims.
- **03** → §A.2/§B: V1 RESOLVED — recovery-hole wrapper preserves the plan/hole-membership validation before
  lock; generic AS2 `_locked` store used inside.
- **04** → §A.3 step 6: ownership cleanup is STATE-based (`has_frame`/RAII), never status-based.
- **05** → §A.3 step 6: detached victims are cache-core-local, RAII-freed post-unlock, no plugin handoff;
  VSAPI-free boundary clarified.
- **06** → §A.2: THREE distinct public wrappers (production / AS2-floor / recovery-hole), no ambiguous shared
  `pin_list`.
- **07** → §A.3: live values pinned — `retain_checkpoint_count = CNR3_CACHE_CHECKPOINT_MIN_RETAIN` (10),
  `max_remove_count = CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS` (8).
- **08** → §D: KDT fields come from the new summary struct; store-kind-to-name helper; booleans `0/1`.
- **09** → §H: `CNR3_EDIT_VERSION` bump to `CMS07-W.3-combined-live-store-prune-helper` + `cnr3_build_config.h`
  in changed files.
- **10** → this header: version corrected to v0.4.

---

## 0. WHAT THIS BUILDS

A small family of cache-core public methods — three thin wrappers over one shared private implementation —
that each perform, in ONE `cache_mutex_` critical section: store the just-produced frame (via the right
existing `_locked` store), record an AS2 pin if it is a consumer input, retire decay-eligible hot zones for the
activation target, evaluate the §7.2 + §7.4 triggers and run one bounded prune that detaches victims; then
(after unlock) the input frame and detached victims are released by RAII. The five live store sites in
`arAllFramesReady` are re-pointed at the matching wrapper. The composed primitives already exist and are
selftested; W.3 is the wrappers + a result summary + plugin-side byte estimate + one new selftest + a temporary
KDT + the edit-version bump.

---

## A. THE COMBINED HELPER

### A.1 Location and shape (SR-D-03, SR-D-01)
- New **public methods on `Cnr3OutputCacheCore`** (the class owning `cache_mutex_` and the `_locked`
  primitives), over one shared private impl. NOT `arAllFramesReady`-local. The selftest count rises from 54.
- The impl composes existing `_locked` primitives under ONE `cache_mutex_` acquisition; release happens by
  RAII after the lock scope. NOT "public store (lock/unlock) then separate public prune (lock/unlock)"
  (the SR-D-01 gap hazard).
- **VSAPI-free boundary (clarified, SCOPE-05):** the cache core makes NO direct VSAPI calls. It MAY hold and
  destroy `Cnr3OwnedFrameRef` objects, whose destructor calls `freeFrame` (`cnr3_owned_frame_ref.cpp:69`) —
  this is the existing RAII boundary and it runs AFTER the lock is released. The plugin-side
  authoritative-return / duplicate-winner / VSFrame layer (`cnr3_arAllFramesReady.cpp:532`) stays in plugin
  code.

### A.2 Public API — three wrappers, one summary (SCOPE-01, SCOPE-03, SCOPE-06)

Distinct wrappers so production-vs-AS2 is un-mis-callable (no shared ambiguous `pin_list`):

```cpp
// Production output (frame-0, predecessor target, recovery target) — NO pin.
[[nodiscard]] Cnr3Status store_production_output_and_prune(
    int stored_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,            // BY VALUE — wrapper takes ownership (matches existing public stores)
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CombinedStoreAndPruneSummary& out_summary);

// Recovery FLOOR (AS2 consumer, pinned) — no recovery-plan validation (the floor is the search anchor).
[[nodiscard]] Cnr3Status store_as2_floor_and_prune(
    int stored_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,            // BY VALUE
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CachePinList& pin_list,
    Cnr3CombinedStoreAndPruneSummary& out_summary);

// Recovery HOLE (AS2 consumer, pinned) — preserves the plan/hole-membership guard, THEN stores.
[[nodiscard]] Cnr3Status store_recovery_hole_and_prune(
    const Cnr3CacheRecoverySearchPlan& recovery_plan,
    int hole_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,            // BY VALUE
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CachePinList& pin_list,
    Cnr3CombinedStoreAndPruneSummary& out_summary);
```

**Ownership idiom (W3C-3rd-01) — public wrappers take `Cnr3OwnedFrameRef` BY VALUE; the shared private impl
takes `Cnr3OwnedFrameRef& frame`.** This matches every existing public store
(`store_owned_frame_and_record_pin` `:796`, `store_checkpoint_owned_frame` `:771`, `store_noncheckpoint_owned_frame`
`:745`, the hole wrapper `:748` — all by value), which then pass the local into the `_locked` helper by ref
(e.g. `:690`). Call sites keep `std::move(cache_owned_frame)` / `std::move(floor_owned_frame)` /
`std::move(hole_owned_frame)`. The wrapper now owns the frame as a LOCAL: on success the `_locked` store moves
from it; on duplicate/reject it still owns the loser, which its destructor frees after the lock scope (A.3 step
6). State-based cleanup is therefore a property of the wrapper's own local frame, not a mutation of a
caller-owned lvalue, and early failures (invalid `frame_byte_count`, failed pre-reservation, hole-guard
failure) release the local frame unambiguously by RAII.

> **CROSS-CHECK REQUESTED (W3X → W3C) — by-value vs by-reference ownership.**
>
> *W3X's question and why it is being asked:* W3X has been bitten before by by-reference / by-value parameter
> changes in past projects (with human developers, including W3X), where such a change had consequential impact
> not only on the code but on the underpinning LOGIC embodied in it. Before accepting the v0.6 by-value wrapper
> signatures, W3X asks two things: **(1) what is the RIGHT thing to do**, and **(2) what are the consequential
> impacts on logic and code, and can these ALL be chased down and confirmed beforehand?**
>
> *W3D's take (source-grounded, for W3C to confirm or challenge):* by-value is right for three converging
> reasons — it is the correct C++ sink idiom for a move-only owning type (a by-reference `&` on such a type is
> ambiguous about whether the callee took ownership and leaves the caller a moved-from husk); it matches all
> four existing public stores (`:745/:771/:796/:748`); and it localizes the duplicate/reject cleanup to the
> wrapper's own LOCAL frame rather than splitting it across the caller boundary. On blast radius, W3D found the
> change is the SAFE case, not the dangerous one: (a) the three wrapper names appear **0** times in the tree —
> additive, no existing caller to break; (b) the five call sites being rewired already `std::move(...)` into
> the existing by-value stores (`:521/:527/:1015/:1224/:1542`), so the ownership handoff is zero-delta
> like-for-like; (c) `Cnr3OwnedFrameRef` is move-only (copy ctor/assign `= delete`, `cnr3_owned_frame_ref.h:38-39`),
> so a missed `std::move` FAILS TO COMPILE — no silent semantic drift, unlike the copyable-type case that bites;
> (d) the four-way + E.4 refcount accounting prove the runtime ownership at patch time. W3D's read: keeping
> by-reference would be the riskier choice here, and by-value changes WHERE the owning object lives (and thus
> where RAII fires), not WHAT happens to the frame — no logic divergence to chase.
>
> *Ask of W3C:* independently investigate against the committed source and form your OWN view. Specifically
> confirm or refute: (a) the three wrappers have no existing callers (additive); (b) the call-site handoff is
> truly like-for-like by-value; (c) the move-only compiler-enforcement claim; (d) whether by-value vs
> by-reference changes any underpinning LOGIC or lifetime behaviour anywhere — not merely style — that W3D may
> have missed; and (e) any consequential impact that CANNOT be confirmed before the patch is written, if such
> exists. Report agree/disagree with reasons, citing source.
>
> **RESOLVED (W3C independent cross-check, v0.6a→b):** by-value confirmed correct on all of (a)-(e); no
> unconfirmable lifetime consequence. W3C surfaced an in-source directive W3D had not cited —
> `cnr3_cache_core.cpp:722-729` explicitly documents the by-value keep-alive-across-nested-lock pattern and
> forbids changing it to by-reference (doing so would free the rejected loser while holding `cache_mutex_`,
> violating the freeFrame-outside-lock rule). The by-value choice is settled by source. The same sweep found a
> separate, off-axis issue — AS2 duplicate normalization — folded below (§A.2 normalization rule, §E.4).

**Result summary (SCOPE-01)** — `Cnr3CachePruneExecutionSummary` (`:412-419`) has only trigger/count fields, so
a new struct carries the rest (drives KDT, authoritative-return, and selftest observability):

```cpp
struct Cnr3CombinedStoreAndPruneSummary {
    Cnr3CacheStoreKind          store_kind = Cnr3CacheStoreKind::Invalid;     // not enum-zero "valid" kind
    int                         stored_frame_number = -1;
    int                         activation_target_frame = -1;
    Cnr3Status                  store_status = Cnr3Status::invariant_violation;  // non-ok sentinel, NOT ok(=0)
    Cnr3Status                  retire_status = Cnr3Status::invariant_violation;
    Cnr3Status                  prune_status = Cnr3Status::invariant_violation;
    Cnr3CacheAs2StoreRecordSummary  as2_summary{};     // valid only for AS2 kinds (struct at :428)
    Cnr3CachePruneExecutionSummary  prune_summary{};
};
```
**Non-ok defaults (W3C-3rd-minor):** `Cnr3Status::ok == 0` (`cnr3_common.h:92`), so default-zero status fields
would falsely read `ok`. Initialise the three statuses to a hard non-ok sentinel (`invariant_violation`) so a
guard-failure or early-return summary never looks like a valid `ok` outcome; likewise `store_kind` defaults to
`Invalid` (see enum below), not the first valid enumerator.

**Store-kind enum (SR-C-03):** `Cnr3CacheStoreKind { Invalid = 0, ProductionNonCheckpoint, ProductionCheckpoint,
As2ConsumerNonCheckpoint, As2ConsumerCheckpoint }` (`Invalid` is the default/sentinel per W3C-3rd-minor;
the four valid kinds are is_checkpoint × should_pin). Each wrapper sets the kind in the summary.

**Store-status-proceed rule (SCOPE-01):** `ok` AND `duplicate` are BOTH successful store outcomes — the impl
proceeds to retire + prune for either. A `duplicate` may still be a valid first-in-best-dressed adopt/pin for
AS2 and may promote checkpoint state. Any hard-error store status → do NOT retire/prune; fall through to
cleanup (A.3 step 6).

**Wrapper return-status contract (W3C-2nd-01) — the wrapper RETURN value is the overall HARD-operation status,
distinct from `out_summary.store_status`:**
- If `store_status` is `ok` OR `duplicate`, AND retire and prune both succeed → the wrapper returns
  `Cnr3Status::ok`.
- `duplicate` MUST NOT escape as the wrapper return value. `cnr3_status_is_ok()` returns true only for
  `Cnr3Status::ok` (`cnr3_common.h:143-146`; `duplicate` is a distinct value, `:99`), so a leaked `duplicate`
  return would be misread as a failure by any call site using that helper. The original store outcome
  (including `duplicate`) lives ONLY in `out_summary.store_status`.
- **Production authoritative-return** decides off `out_summary.store_status`: `ok` → return the computed output
  frame; `duplicate` → discard the computed frame and return the cached winner (first-in-best-dressed). It does
  NOT branch on the wrapper return for this (the wrapper return is `ok` in both cases).
- **AS2 callers** use the wrapper return for hard failure, and `out_summary.as2_summary.pin_recorded` for the
  required pin proof.

**AS2 duplicate normalization (W3C-unknown-unknown) — `store_status` is derived differently for production vs
AS2, because the two `_locked` stores report duplicate differently:**
- *Production* (`store_owned_frame_locked`) RETURNS `Cnr3Status::duplicate` on a duplicate (`:1844-1888`), so
  the production wrapper may use that return directly as `out_summary.store_status`.
- *AS2* (`store_owned_frame_and_record_pin_locked`) does NOT return `duplicate`: on a duplicate it records
  `as2_summary.duplicate_existing_slot = true` / `incoming_frame_rejected = true` (`:1998-2003`), pins the
  existing winner, and RETURNS `Cnr3Status::ok` (`:2074-2081`). So an AS2 wrapper MUST normalize
  `out_summary.store_status` from the sub-summary, NOT from the AS2 helper return:
  ```
  when as2 return is ok:
      if as2_summary.duplicate_existing_slot -> store_status = duplicate
      else if as2_summary.inserted_new_slot  -> store_status = ok
      else                                    -> store_status = invariant_violation; return invariant_violation
  when as2 return is hard error:
      store_status = as2_return; return as2_return
  ```
  An AS2 duplicate still PROCEEDS to retire/prune (the AS2 helper return is `ok` and `pin_recorded` is true) —
  the normalization only restores duplicate VISIBILITY for the KDT and E.4, it does not change control flow.
  `duplicate` still never escapes as the wrapper return (still overall hard status). Without this, every AS2
  duplicate would read `store_status == ok` and the duplicate proof would be silently lost.

  *Blast-radius check (W3D, source-swept):* the duplicate→ok normalization is localized to
  `store_owned_frame_and_record_pin_locked` ALONE (no other store has it). Its only existing consumers are the
  recovery floor (`cnr3_arAllFramesReady.cpp:1013`) and recovery hole (`:1221`), both of which already gate on
  `cnr3_status_is_ok(return) && summary.pin_recorded` and never relied on a `duplicate` return — so there is NO
  pre-existing latent bug, and both are already in W.3's rewiring scope. The four sites that branch on
  `Cnr3Status::duplicate` (`:71/:254/:470/:566`) are ALL on the production store path (where
  `store_owned_frame_locked` returns `duplicate` directly), not the AS2 path. The normalization here serves a
  NEW W.3 reporting need (duplicate visibility in the combined summary for KDT/E.4).

  *W.3 live-code footprint (to avoid misreading the line above):* W.3 DOES change live code — it adds the three
  by-value public wrappers and rewires the live store call sites in `arAllFramesReady` (§B) to call them, with
  ownership moved in by value. What is NOT happening is the repair of an existing AS2 duplicate-status bug —
  there is none; existing AS2 consumers (floor/hole) already use the correct `is_ok` + `pin_recorded` contract.
  The AS2 duplicate contract is merely made EXPLICIT here so W.3's new wrappers, summary, KDT, and selftests
  preserve it correctly while the live store paths are rewired. "Not repairing existing behaviour" refers to
  the AS2 status asymmetry only, NOT to W.3's (real and intended) rewiring of the live store call sites.

**Kind → `_locked` store mapping (verified, §A.5):**

| Kind / wrapper | `_locked` store under the lock | pin? | header |
|----------------|--------------------------------|------|--------|
| Production* (`store_production_output_and_prune`) | `store_owned_frame_locked(frame, is_checkpoint)` | no | `:1226` |
| AS2 floor (`store_as2_floor_and_prune`) | `store_owned_frame_and_record_pin_locked(..., is_checkpoint, pin_list, ...)` | yes | `:1240` |
| AS2 hole (`store_recovery_hole_and_prune`) | same `store_owned_frame_and_record_pin_locked` AFTER the hole guard | yes | `:1240` |

**V1 RESOLVED (SCOPE-03):** there is no `store_recovery_plan_hole_owned_frame_and_record_pin_locked`. The public
hole wrapper (`cnr3_cache_core.cpp:746-791`) does real, NON-mutating validation before delegating: frame-number
validity, `frame.has_frame()`, recovery-plan status, **rejects `hole_frame_number == recovery_plan.requested_frame`**,
and confirms membership in `recovery_plan.hole_frame_numbers`. `store_recovery_hole_and_prune` MUST reproduce
that guard (it is hard-status pre-delegation logic, not cache mutation) BEFORE pre-reservation and lock; on any
guard failure it returns the guard status with NO reservation, NO lock, NO mutation. Only after the guard
passes does it run the shared store-prune impl using `store_owned_frame_and_record_pin_locked`. This preserves
the H.3A hole-consumer guard rather than silently dropping it. The wrapper populates `out_summary` identity
fields (kind, stored/target frames) and, on guard failure, sets `store_status` to the guard status before
returning (see §C guardrail a), so a hole-guard failure still emits a fully diagnosable KDT line.

### A.3 The six-step order (CMS §7.5, SR-D-05, SR-OD-01, SR-OD-02)
Per wrapper, the shared impl, under ONE `cache_mutex_` hold (steps 1-5), with live values pinned (SCOPE-07):
`retain_checkpoint_count = CNR3_CACHE_CHECKPOINT_MIN_RETAIN` (10, `:102`); `max_remove_count =
CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS` (8, `:148`).

1. **Store/adopt** via the kind-mapped `_locked` store (A.2). Record `store_status`; `ok`/`duplicate` proceed.
2. **Set `is_checkpoint`** — folded into the store call (monotonic, §6.6).
3. **Pin if AS2** — folded into `store_owned_frame_and_record_pin_locked` (AS2 wrappers only).
4. **Retire**: `retire_decay_eligible_hot_zones_locked(activation_target_frame)` (`:1175`). BEFORE step 5
   (SR-D-05); uses activation target N (SR-OD-01).
5. **Evaluate triggers + decide/detach** — VERBATIM signature (`cnr3_cache_core.h:1363`, reproduce exactly):
   ```cpp
   [[nodiscard]] Cnr3Status execute_bounded_prune_pass_locked(
       std::uint64_t frame_byte_count,
       std::size_t retain_checkpoint_count,
       std::size_t max_remove_count,
       std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order,
       std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order,
       std::vector<int>& selected_frame_numbers,
       std::vector<Cnr3CacheSlot>& detached_slots,
       Cnr3CachePruneExecutionSummary& out_summary
   );
   ```
   Evaluates BOTH triggers (via `cnr3_calculate_cache_prune_trigger_decision`, `:584`) and detaches victims
   into `detached_slots`. The four working vectors are the ones pre-reserved outside the lock (A.4).
6. **Unlock, then RELEASE by RAII (SCOPE-04, SCOPE-05):**
   - **Detached victims:** `detached_slots` is a cache-core-LOCAL vector; after the lock scope closes its
     `Cnr3CacheSlot`s (and their `Cnr3OwnedFrameRef`s) destruct, freeing the victim frames post-unlock — no
     plugin handoff, exactly like existing public prune (`cnr3_cache_core.cpp:1021-1078`).
   - **Input frame ownership is STATE-based, never status-based:** the wrapper owns the incoming frame as a
     LOCAL (by value, A.2). After the lock, that local `Cnr3OwnedFrameRef` is released IFF it still owns a
     frame (`has_frame()` / its destructor at wrapper return), regardless of the status label. A successful
     insert moved from it (`:1925-1932`); a `duplicate` did NOT (`:1844-1888`); an AS2 duplicate pins the
     existing winner while the incoming stays owned (`:1991-2044`); a hard error after partial movement is
     covered by checking ownership, not status. The header documents this (`:1203-1210`). Because the frame is
     the wrapper's own local, no caller lvalue is mutated. Never `freeFrame` under the lock (§8.2; h:45).

**Per-store timing (SR-OD-02, #0037):** steps 4-5 run after EACH store (per floor/hole/target), each a
self-contained bounded critical section. Safe: walk dependencies are pinned (floor/anchor/stored holes) or
hot-zone-protected (target); unstored holes are not yet cached (SR-D-02). +1 slot per store, so `max_remove ≥ 1`
keeps pace; the live value 8 gives headroom.

### A.4 Byte estimate — PLUGIN-side, passed in (SCOPE-02; SR-C-01)
- **Ownership:** the PLUGIN computes `frame_byte_count` ONCE per activation, before the first combined-helper
  call, from instance state (`video_info`, `bits_per_sample`, `sub_sampling_w/h`; `cnr3_plugin_internal.h:30-36`),
  and passes the SAME value into every per-store call. The cache-core helper does NOT compute it (it cannot see
  `Cnr3FilterData`/`VSVideoInfo`); it VALIDATES `frame_byte_count != 0` and returns `invalid_argument`
  otherwise.
- **Formula (summed actual plane bytes, conservative, NOT ×3):** `bytes_per_sample = (bits_per_sample > 8) ? 2
  : 1`; luma `= width * height`; chroma per plane `= chroma_w * chroma_h` with CEIL-shift dims so odd
  dimensions never under-estimate:
  `chroma_width = (width + ((1 << sub_sampling_w) - 1)) >> sub_sampling_w`,
  `chroma_height = (height + ((1 << sub_sampling_h) - 1)) >> sub_sampling_h`.
  `frame_byte_count = (luma + 2 * chroma) * bytes_per_sample`, in checked `std::uint64_t`. Conservative-high is
  the safe direction (over-estimate prunes slightly early; never under-protects memory). Rationale: `×3`
  over-estimates 2× on 4:2:0 (the primary PAL 720×576 workload), halving effective cache.

### A.5 Composition feasibility (VERIFIED against source)

| Primitive / type | exists | note |
|------------------|--------|------|
| `store_owned_frame_locked` (`:1226`) | yes | generic `(int, Cnr3OwnedFrameRef&, bool is_checkpoint)` — serves both production kinds |
| `store_owned_frame_and_record_pin_locked` (`:1240`) | yes | AS2 store/adopt/pin; precond: pin-list pre-reserved one token |
| `store_recovery_plan_hole_owned_frame_and_record_pin_locked` | **NO** | V1 — only the public variant (`:824`/def `:746`); guard reproduced in the wrapper (A.2) |
| `retire_decay_eligible_hot_zones_locked` (`:1175`) | yes | `(int current_frame)` |
| `execute_bounded_prune_pass_locked` (`:1363`) | yes | precond: four vectors pre-reserved |
| `Cnr3CachePinList::reserve_for_additional_pins` (`:1578`) | yes | the SR-C-02 pin reservation |
| `Cnr3CacheAs2StoreRecordSummary` (`:428`) / `Cnr3CachePruneExecutionSummary` (`:407-419`) | yes | feed the new summary struct |

Pre-reserve (SR-C-02): before the lock, reserve the four prune vectors to `max_remove_count` AND (AS2 wrappers)
one pin-list token; vectors are CALL-LOCAL. Reservation failure ⇒ no lock, no mutation.

**Allocation-bar scoping (W3C-2nd-03):** the pre-reserve / "no new in-lock allocation" rule (SR-C-02, SR-D-08
item 3) applies ONLY to W.3's ADDED call-local working state — the four prune vectors and the AS2 pin-list
token. It does NOT apply to the composed `_locked` store primitive, which inherently and as-already-proven
grows the cache containers under the lock (`frame_index_.emplace` `:1913`, `slots_.push_back` `:1932`,
checkpoint-position list growth). W.3 composes that store primitive unchanged; it neither alters nor re-proves
its internal allocation. Patch review greps for no NEW in-lock allocation in the combined helper ITSELF, while
recognising the called store retains its existing allocation behaviour. A literal "no allocation under the
lock" bar would be impossible (a store must grow the container) and is not the rule.

---

## B. CALL-SITE WIRING (SR-C-04)

| Site | Current call | Line | New wrapper |
|------|--------------|------|-------------|
| Production output (inner store, ckpt/non-ckpt) | `store_checkpoint_owned_frame` / `store_noncheckpoint_owned_frame` | :519 / :525 | `store_production_output_and_prune` (via the inner helper / wrapper at :487/:532, reached from :804 and :1361) |
| Frame-0 proof | `store_checkpoint_owned_frame` | :1540 | `store_production_output_and_prune` |
| Recovery floor | `store_owned_frame_and_record_pin` | :1013 | `store_as2_floor_and_prune` |
| Recovery hole | `store_recovery_plan_hole_owned_frame_and_record_pin` | :1221 | `store_recovery_hole_and_prune` |

**Cache-hit EXEMPT:** `cnr3_get_frame_live_cache_hit_return` (def :597, dispatch :1601) adds no slot → no
store, prune, or retire. **Grep check (SR-C-04 + W3OD #0001):** no old store-only public call remains in
`arAllFramesReady`; every store routes through a wrapper; the VSAPI/authoritative-return layer (:532) preserved.

---

## C. SR-D-08 CONCURRENCY-SAFETY GATE (mandatory patch-review checklist)

Agreed three-way (D #0035, OD #0043, C #0045). Verified at patch review against the patched impl:
1. **One hold, no gap** across steps 1-5; trigger read inside the same hold as the prune (no TOCTOU).
2. **`_locked`-only inside the hold** — no public lock-owning method; `cache_mutex_` non-recursive (h:616-627),
   accidental re-entry deadlocks (caught in test). Reviewer greps the lock scope.
3. **Call-local working state** — four prune vectors + pin reservation are per-call locals, not shared members.
   The pre-reserve / no-new-in-lock-allocation bar covers THESE added structures only; the composed `_locked`
   store keeps its existing in-lock cache-container growth (`:1913`, `:1932`), unchanged and outside W.3's rule
   (see A.5 allocation-bar scoping). Reviewer greps for no NEW in-lock allocation in the combined helper itself.
4. **Outside-lock work immutable/local only** — the (plugin-supplied) byte count and the local reservations.
5. **Free after unlock by RAII** (A.3 step 6); no `freeFrame` under the lock; prevents `cache_mutex_`↔VSAPI
   lock-ordering deadlock.
6. **No new sync machinery** — sole sync point stays the one non-recursive `cache_mutex_`.

**Summary-population guardrails (W3C-unknown-unknown follow-ons):**
- a. **Each public wrapper — not only the shared impl — initializes** `out_summary.store_kind`,
  `stored_frame_number`, `activation_target_frame`, and the relevant early-failure `store_status`, BEFORE any
  return: a wrapper-level guard-failure return (the recovery-hole plan/membership guard), a `frame_byte_count ==
  0` validation return, a pre-reservation-failure return, OR a return out of the shared impl. This matters
  because some early exits happen in the WRAPPER before the shared impl runs (the hole guard is the clearest
  case — A.2 requires it before reservation/lock/mutation), and the KDT emits one line per wrapper invocation,
  so an unpopulated summary would print an undifferentiated line. For a recovery-hole guard failure
  specifically: `store_status` = the guard-failure status; `retire_status` and `prune_status` remain the non-ok
  sentinel (`invariant_violation`); and no reservation, lock, or mutation has occurred. (W3C v0.7 review.)
- b. Production wrappers leave `out_summary.as2_summary` at default and never infer pin state from it.
- c. AS2 wrappers require `out_summary.as2_summary.pin_recorded == true` before retire/prune proceed (the AS2
  helper should only return `ok` with the pin recorded; this explicit invariant protects the new contract).
- d. If retire succeeds but prune fails, the wrapper return is the prune hard status; the summary still
  preserves `store_status`, `retire_status`, `prune_status`, and `as2_summary` so error/cleanup paths are
  inspectable.

---

## D. OBSERVABILITY — temporary KDT line (SR-C-05; SCOPE-08)

One temporary KDT line per wrapper invocation, AFTER the helper returns and OUTSIDE the lock, guarded by
`CNR3_KEYSTONE_DEV_TRACE` (stderr only; `(void)`-cast all args in the `#else` branch), fields drawn from the
new `Cnr3CombinedStoreAndPruneSummary` (NOT from `Cnr3CachePruneExecutionSummary` alone):

```
[KDT] instance=%d target_N=%d stored_frame=%d kind=%s store=%s retire=%s prune=%s \
      cap_trigger=%d ckpt_trigger=%d selected=%zu detached=%zu
```
- `kind=%s` via a small `cnr3_cache_store_kind_name(Cnr3CacheStoreKind)` helper (mirrors `cnr3_status_name`).
- `store/retire/prune=%s` via `cnr3_status_name`.
- `cap_trigger` / `ckpt_trigger` printed as `0/1` from `prune_summary.trigger_decision`.
- `selected` / `detached` from `prune_summary`.
W3D owns the exact token text; the harness greps it verbatim (the W.2 grep-token lesson). Temporary; folds into
D-SUM-10/11/12 later.

---

## E. SELFTEST ADDITIONS (SR-D-03, SR-OD-03)

Count rises from 54; four-way must stay green. The new summary struct gives the observability these assert on.
- **E.1 atomicity/order:** one call per `Cnr3CacheStoreKind`; assert store happened, pin recorded iff AS2,
  retire before select, prune detached the expected victims (by identity), free occurred post-return.
- **E.2 discriminating retire-before-select (SR-OD-03):** decay-eligible zone shielding an otherwise-prunable
  frame; correct order makes that frame the victim, wrong order does not — proven by asserted victim identity.
- **E.3 W.2 interlock:** establish the zone directly via `record_hot_zone_observation_locked(N)` (`:1171`; or
  public `record_hot_zone_observation`, `:684`), store output[N] via the wrapper, assert output[N] is NOT among
  detached victims — for BOTH a non-checkpoint and a cut (checkpoint) output (SR-D-06).
- **E.4 (add) duplicate/ownership — prove BOTH layers distinctly (W3C-2nd-02, adjusted for by-value W3C-3rd-01):**
  - *`_locked` store level:* a `duplicate` store does NOT consume the incoming frame — assert
    `frame.has_frame()` on the wrapper's LOCAL frame immediately after the locked store returns, BEFORE wrapper
    cleanup (`store_owned_frame_locked` duplicate path returns without moving, `:1844-1888`).
  - *public wrapper level:* because the wrapper takes the frame BY VALUE, the caller's object is moved-from at
    the call boundary — so checking the CALLER's `has_frame()` only proves the move, NOT the post-unlock
    release. Prove the release instead via the test owned-frame / refcount accounting: acquired = released +
    transferred, no leak, no double-free; the wrapper returns `ok`; `out_summary.store_status == duplicate`;
    retire and prune ran. The duplicate loser (the wrapper's local) is freed after unlock by RAII.
  - The two facts are at different layers and must not be conflated: the locked store retains the frame; the
    public wrapper frees it after unlock.
  - *AS2 duplicate normalization case (W3C-unknown-unknown):* drive an AS2-kind duplicate and assert
    `out_summary.as2_summary.duplicate_existing_slot == true`, `out_summary.as2_summary.pin_recorded == true`,
    the NORMALIZED `out_summary.store_status == Cnr3Status::duplicate`, AND the wrapper return is
    `Cnr3Status::ok` (retire/prune also succeeded). Contrast with the production duplicate case, which gets
    `store_status == duplicate` directly from `store_owned_frame_locked`. This test catches the exact
    production-vs-AS2 return-status asymmetry.

---

## F. PROOF / TEST PLAN (SR-D-04)

Not byte-identity (W.3 changes cache state by design). Bar: (1) every RETURNED frame byte-correct; (2) eviction
OBSERVABLY fires (KDT + selftests); (3) the just-produced target not evicted by its own prune (E.3); (4)
four-way green. **Designer-owned eviction-proof harness (W3D builds, not coder scope):** golden 576p50 +
`SetVideoCache(mode=0)`, a scenario driving past BOTH triggers (long sequential run = capacity; cut-dense
segment = checkpoint) + a recovery-forcing jump; greps the §D KDT tokens; confirms returned frames match
golden. Delivered alongside W3D's review of the coder patch (runs post-patch).

---

## G. EXPLICITLY OUT OF SCOPE

No concurrency/fmParallel code (SR-D-07; single-activation is validation scope). No cache-core policy changes
(reuse triggers/selectors/retire/hot-zone/store as-built; if a primitive cannot be reused, STOP and raise it).
No VSAPI calls in the cache core (RAII `Cnr3OwnedFrameRef` destruction is the existing boundary, A.1). No
diagnostics-arc work (KDT is temporary).

---

## H. CHANGED FILES + EDIT-VERSION + REMAINING CODER ITEMS

**Expected changed files:** `cnr3_cache_core.h`, `cnr3_cache_core.cpp` (the wrappers + impl + summary struct +
store-kind-name helper), `cnr3_cache_core_selftest.*` (E.1-E.4), `cnr3_arAllFramesReady.cpp` (call-site
wiring + plugin-side byte estimate + KDT), and **`cnr3_build_config.h`** (edit-version bump).

**Edit-version bump (SCOPE-09, mandatory):** set `CNR3_EDIT_VERSION = "CMS07-W.3-combined-live-store-prune-helper"`.
Because `cnr3_build_config.h` is common to both projects, this also forces the DLL and selftest projects to
rebuild.

**Coder items to state in the patch proposal:**
- **CMS amendment (forward-protection, parallel to the patch — not a blocker):** the production-vs-AS2
  `store_status` asymmetry is emergent in code, not a documented contract. Recommend a CMS invariant (W3D+W3C
  agreed wording): *Production cache stores return `Cnr3Status::duplicate` for a first-in-best-dressed duplicate
  store. AS2 pinned stores normalize a duplicate store to overall `ok` when the existing winner is successfully
  pinned and recorded; duplicate visibility is reported through
  `Cnr3CacheAs2StoreRecordSummary::duplicate_existing_slot`. Consumers that need duplicate visibility from AS2
  must read the AS2 summary, not the returned status. In all AS2 duplicate cases, the existing winner remains
  authoritative, the incoming loser is released after the cache lock, and exactly one pin is recorded for the
  existing winner.* Protects planned consumers (diagnostics arc D-SUM-10/11/12, fmParallel) from re-tripping the
  same unknown-unknown.
- **By-value cross-check (W3X-raised, §A.2):** RESOLVED — W3C independently confirmed by-value against source
  (the `:722-729` directive forbids by-reference); no unconfirmable lifetime consequence. See the §A.2 box.
- **V2** (B): host the production call via the inner `cnr3_store_live_output_frame_for_return` rewrite or the
  wrapper re-point — either, if the authoritative-return layer (:532) is preserved and no old path remains.
- The shared impl factoring (one private method behind the three public wrappers) — coder's structural choice,
  provided the SR-D-08 gate and the per-wrapper guards (esp. the hole guard, A.2) hold.

*(V1, V3, V4 from v0.3 are RESOLVED in this scope: V1 → A.2 hole-guard preservation; V3 → A.3 step 6 RAII-local
detached free; V4 → A.3 step 6 state-based input-frame release.)*

---

## I. TRACEABILITY

Register: A.1 ← SR-D-01/03 · A.2 ← SR-C-03, SR-D-06 · A.3 ← §7.5, SR-D-05, SR-OD-01, SR-OD-02(#0037) · A.4 ←
SR-C-01 · A.5/pre-reserve ← SR-C-02 · B ← SR-C-04 · C ← SR-D-08 · D ← SR-C-05 · E ← SR-D-03, SR-OD-03, SR-D-06 ·
F ← SR-D-04 · G ← SR-D-07. SR-D-02 underpins A.3 per-store safety.
W3C scope review: 01 ← A.2 · 02 ← A.4 · 03 ← A.2/B (V1) · 04 ← A.3 step 6 · 05 ← A.1/A.3 step 6 · 06 ← A.2 ·
07 ← A.3 · 08 ← D · 09 ← H · 10 ← header.
W3C second pass: 2nd-01 ← A.2 (return-status contract) · 2nd-02 ← E.4 (two-layer ownership) · 2nd-03 ← A.5 / C item 3 (allocation-bar scoping).
W3C third pass: 3rd-01 ← A.2 / A.3 step 6 (by-value wrapper ownership) · 3rd-minor ← A.2 (Invalid kind + non-ok summary defaults) · E.4 adjusted for by-value.
W3C unknown-unknown cross-check: by-value RESOLVED ← A.2 box · AS2 duplicate normalization ← A.2 / E.4 · summary-population guardrails ← C.

— End of W.3 Coder Build Scope v0.8 (wrapper-level early-summary init folded; coder-ready for patch proposal).
