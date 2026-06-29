# CNR3 W.2 — Coder Build Scope: live hot-zone observation at arInitial (CMS07.14 §7.6)

**Version note (v1.1):** corrects a role-boundary error in v1.0 §4/§6 — the proof harness is a DESIGNER
deliverable (adapted from the existing `test_000_Example_576p50.*` golden set and run by the coordinator),
NOT something the coder proposes. The coder's deliverable is the patch only (the `cnr3_arInitial.cpp` change +
the `cnr3_build_config.h` marker bump); the coder still owns that the temporary KDT line is correctly placed,
guarded, and emits the right `instance`/`N`/`status` so the harness can observe it. All technical substance
(3a single common point, the ordering prerequisite, observation-only/no-retirement, the KDT visibility
requirement) is unchanged from v1.0.

**Phase:** W.2 (second of the live cache-pressure wiring arc; the §7.6 prerequisite for W.3's store-prune).
**Authority:** CMS07.14 §7.6 (the decision), §5.7 (hot-zone update at arInitial), §4.1 (production never pins),
§7.5 (the W.3 combined helper this enables).
**Provenance:** Step 0 review, SR-C-02 / SR-D-02 / SR-D-03 / SR-D-07; `CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md`.
**Scope discipline:** read-first / propose / review / prove — like a K or D plugin-only phase. This is a DLL-only
live-wiring phase: it touches `cnr3_arInitial.cpp` (+ the build marker), NOT the cache core. NO code until this
scope is reviewed and approved.

---

## 0. Why W.2 is wiring, not new logic (the inverse of W.1)

W.1 added a genuinely new cache-core primitive. W.2 is the opposite: the cache core already has a proven,
locked `record_hot_zone_observation(int)` (it has had ZERO live callers — DELTA audit). W.2 wires that one
call into the live `cnr3_arInitial` path so produced output[N] is protected from a same-activation prune.

Why this matters (the §7.6 reason): a produced final output (the branch target, the frame-0 fresh start) is
NOT pinned — production-never-pins, CMS §4.1, confirmed in source: the target/frame-0 stores go through the
no-pin store helpers, unlike AS2 consumer inputs which ARE pinned on `pin_list`. So when W.3 later runs a
store-and-prune in the SAME activation, the only thing that keeps output[N] out of the prune candidate set is
hot-zone membership: observing N at arInitial centres a zone `[N - BACK_RADIUS, N + FORWARD_RADIUS]` so N is
inside its own active zone and excluded by the §7.1 "outside every active hot zone" clause. W.2 establishes
that prerequisite; W.3 relies on it. Without W.2, W.3's prune could evict the very frame the activation just
produced.

It is lower-risk than W.1 (no new selection logic, no cache-core change), but it is live-getFrame wiring with
no cache-core selftest to catch a mistake, so the PROOF must make the wire-in *visible* (it actually fired on
every branch), not merely "it compiled and ran" (see §4).

---

## 1. READ-FIRST (orient before proposing; do not modify yet)

Confirm your understanding of these against the committed source, and report the read-first outcome BEFORE
proposing the patch. Flag any mismatch as a finding; do not paper over it.

1. **`cnr3_arInitial` dispatch (`cnr3_arInitial.cpp`).** Confirm the single dispatcher reaches FOUR terminal
   publish paths, each setting `request_data->branch`, `requested_frame=n`, `source_requested`, `*frame_data`,
   then `requestFrameFilter(...)` and returning NULL:
   - cache-hit -> `cnr3_publish_live_cache_hit_return` (after `lookup_frame_and_record_pin(n)` succeeds);
   - frame-0 fresh start -> `cnr3_publish_live_frame0_fresh_start` (n==0);
   - predecessor-present -> `cnr3_publish_live_predecessor_present_compute_from_pinned_predecessor`
     (after `lookup_frame_and_record_pin(n-1)` succeeds);
   - recovery -> `cnr3_start_live_recovery` (exact-anchor or floor-fresh-start; else a refusal/error path).
   Confirm the KEY FACT for §3: all four branches (and the refusal path) pass through the single dispatcher
   entry, where `n` is known, BEFORE any branch decision.
2. **`record_hot_zone_observation` API (`cnr3_cache_core.h`/`.cpp`).** Confirm: signature
   `[[nodiscard]] Cnr3Status record_hot_zone_observation(int frame_number)`; it takes `cache_mutex_` ITSELF
   (so it must NOT be called while already holding the lock — at the top of arInitial no lock is held);
   it validates the frame number (`invalid_argument` on a bad N) and delegates to the `_locked` form. It needs
   ONLY the frame number — not the request kind (SR-D-03). This is the call W.2 REUSES; it is proven and W.2
   must NOT modify it.
3. **The existing arInitial KDT pattern to mirror.** Confirm `cnr3_trace_live_recovery_refusal` is guarded by
   `#if defined(CNR3_KEYSTONE_DEV_TRACE)`, emits a single `[KDT] instance=%d N=%d ...` line to **stderr**, and
   has a `(void)`-cast else branch so the build is warning-clean when the gate is off. W.2's temporary
   observation KDT mirrors this exactly (§4). `data.config.instance_id.value` is the instance id in scope.
4. **Project membership (Document A v3.10 build-env map).** Confirm `cnr3_arInitial.cpp` is **DLL-project
   ONLY** (it is NOT in `cnr3_cache_core_selftest`). Therefore W.2 changes NO cache-core selftest and the
   selftest count STAYS 54; its proof is the live path / a coordinator A/B harness, not a new selftest (§4).
5. **Retirement is NOT here.** Confirm from CMS §5.6 / SR-C-05 that hot-zone *retirement*
   (`retire_decay_eligible_hot_zones`) is LAZY inside the prune pass — i.e. it belongs to W.3's combined
   helper, NOT to arInitial. W.2 wires OBSERVATION only (§5).

---

## 2. The wiring's contract (what W.2 must build)

A single live call to the proven observation primitive, on every getFrame branch, made visible by a temporary
KDT line. Stated as a contract:

**The call.** Invoke `data.output_cache.record_hot_zone_observation(n)` at arInitial, once per activation,
for the requested frame `n`. The result is `[[nodiscard]]` — check it.

**The point (see §3).** Designer lean: a SINGLE call at the top of `cnr3_arInitial`, after the `request_data`
allocation null-check and BEFORE the cache-hit `lookup_frame_and_record_pin`, so all four branches and the
refusal path traverse it. Observation needs only `n` (SR-D-03), so it does not need to know the branch.

**Coverage (the §7.6 requirement).** Observation must occur on EVERY branch: cache-hit, frame-0 fresh start,
predecessor-present, recovery. On cache-hit the output is also pinned, but observing N is still correct and is
what §5.7 specifies (register zone activity at arInitial regardless of branch); do NOT special-case or skip
cache-hit. The branches that strictly DEPEND on it are the ones whose produced output[N] is unpinned (frame-0
fresh start; the recovery/predecessor target stored for return) — those are the ones W.3's prune could
otherwise evict.

**Ordering (state it so W.3 can assume it).** Observation of N happens at arInitial, which entirely precedes
arAllFramesReady, so it is necessarily BEFORE any arAllFramesReady store-and-prune for the same activation.
W.3's §7.5 combined helper RELIES on this: output[N] is already zone-protected when the W.3 prune runs. The
W.2 scope must record this ordering guarantee explicitly as the prerequisite it delivers.

**Failure handling.** If the call returns non-ok (only `invalid_argument` for an invalid N is expected on the
normal path; N is valid by construction): set a filter error and discard the unpublished frameData via the
existing `cnr3_delete_unpublished_frame_data` (uniform with the other arInitial failure paths). Per CMS §9A.6
item 5, a zone touched by a request that then fails needs NO rollback — it retires naturally by decay; so a
recorded observation followed by a later branch error is benign and requires no undo.

**Observe-only.** Observation mutates internal hot-zone state ONLY. It must not change any produced frame, any
branch tag, any source request, or any pin. In W.2 nothing in the live path consumes hot-zone state yet (the
live prune is W.3), so observation is provably output-neutral in this phase (see §4 byte-identity proof).

**Atomicity / lock discipline.** `record_hot_zone_observation` takes `cache_mutex_` itself; call it where no
lock is held (the top of arInitial qualifies). It is a separate short locked op from the cache-hit
`lookup_frame_and_record_pin` — do NOT fuse them (fusing would be a cache-core change, out of scope). The KDT
line is emitted AFTER the call returns (outside any lock), per R-PROCESS-13.

---

## 3. One design question for the coder to answer in the proposal

WHERE to place the observation call. Two shapes — propose which, with reasoning:

- **(3a) Single common point (designer lean):** one `record_hot_zone_observation(n)` at the top of
  `cnr3_arInitial`, after the allocation null-check and before the cache-hit lookup. Rationale: all four
  branches and the refusal path traverse this point; observation is branch-kind-independent (SR-D-03), so no
  per-branch information is lost; and a single pre-dispatch call structurally CANNOT miss a branch — a missed
  branch is an unprotected output, exactly the §7.6 hazard. Failure-safe per CMS §9A.6 item 5.
- **(3b) Per-branch:** a call inside each of the four publish helpers. Enumerable, but four sites to keep
  correct as branches evolve, with no benefit (observation needs no branch kind). Only choose this if the
  read-first finds a reason the single point is unsafe or unreachable for some branch (none is expected from
  source).

Designer lean: **(3a)**. Confirm from the source that the single point covers all four branches without a skip
path, and state your choice and why. If you find any branch that does NOT pass through the proposed single
point, that is a finding — raise it; do not silently fall back to (3b) without saying so.

---

## 4. Proof obligations (DLL-only: designer A/B harness + visible KDT; selftest count STAYS 54)

There is no cache-core selftest for this phase (arInitial.cpp is DLL-only). The risk of a DLL-only wiring
phase is a "pass" that never actually fired the wire-in.

**Ownership of the proof (read this first).** The proof harness is a **DESIGNER deliverable**: the designer
adapts the closest existing harness (the golden `test_000_Example_576p50.*` request-order catalogue) into a
phase-specific `test_W2_*` fileset, defines the expected `HOT-ZONE-OBSERVED N=…` line per branch, and the
**coordinator runs it**; the designer verifies the result. The CODER does NOT propose, build, or own the
`.vpy`/`.bat`. The coder's W.2 deliverable is the **patch only** (§6). The one proof responsibility the coder
DOES own: that the temporary KDT line is correctly placed at the single observation point, correctly guarded
by `CNR3_KEYSTONE_DEV_TRACE`, and emits the right `instance`/`N`/`status` text — so the designer's harness can
grep it. The exact KDT line text is fixed below (the harness greps it; the coder emits it).

The proof has two required halves:

1. **Visibility — the wire-in is SEEN to fire on every branch (per SR-C-06).** The coder adds a TEMPORARY KDT
   line at the observation point, mirroring `cnr3_trace_live_recovery_refusal`: guarded by
   `CNR3_KEYSTONE_DEV_TRACE`, stderr-only, `(void)`-cast else branch. **Fixed text (the designer's harness greps
   this, so the coder must emit it verbatim):**
   `[KDT] instance=%d N=%d HOT-ZONE-OBSERVED status=%s` (status `ok` on success). This is explicitly OUTSIDE the
   formal D-SUM framework and is foldable into D-SUM-11 during the diagnostics arc (SR-C-06). The designer's
   harness drives each of the four branches at least once and confirms a `HOT-ZONE-OBSERVED N=<expected>` line
   appears for each, correlated by N with the existing per-frame branch KDT line:
   - cache-hit (re-request an already-produced frame — the harness defeats the downstream VS core cache with
     `SetVideoCache(mode=0)` on the CNR3 node per the K.1F lesson, else the re-request may never re-enter
     getFrame and the branch won't fire);
   - frame-0 fresh start (request frame 0);
   - predecessor-present (sequential request after a stored predecessor);
   - recovery (a far cold jump landing on branch-d, as the D-series / S5 harnesses do).
2. **Output neutrality — observation changed no frame bytes.** The designer's harness confirms the produced
   output bytes are IDENTICAL to a pre-W.2 build for the same scenario (observation is observe-only; nothing in
   the live path consumes hot-zone state until W.3, so by construction there is no live reader of what
   observation writes). This is the live-path analogue of the observe-only discipline (R-PROCESS-12 Part B).

Plus the standard cache-core regression: the four-way selftest STILL reports **54/54** (Debug 54/54 / Release
54/54 / forced-fail 53/54 exit 1 / verbose 54/54) and the count is UNCHANGED — proving the cache core is
undisturbed by a DLL-only change. The selftest count staying at 54 is expected and correct for a plugin-only
wiring phase (the third category: live behaviour added, no selftest — Document A R-PROCESS-20 clarification).

(Goldens here are not pixel chains but branch coverage: the designer's harness exercises all four branches
and confirms each emits one observation KDT line with the correct N, plus byte-identity vs the pre-W.2 build.
The designer owns this harness and its expected per-branch N values; the coder is not asked to construct it.)

---

## 5. What W.2 must NOT do

- NO prune wiring of any kind (that is W.3's §7.5 combined store-and-prune helper). W.2 records observations;
  nothing consumes them yet.
- NO hot-zone RETIREMENT call (`retire_decay_eligible_hot_zones`). Retirement is LAZY inside the prune pass
  (CMS §5.6 / SR-C-05) and belongs to W.3. Do not let it creep into arInitial.
- NO cache-core change. `record_hot_zone_observation` is proven; REUSE it, do not modify it, and do not add a
  variant. (Proven-code-stays-proven, R-PROCESS-21.)
- NO fusing the observation into the cache-hit/predecessor `lookup_frame_and_record_pin` (separate locked op;
  fusing would be a cache-core change). Two short locked ops at arInitial is correct for single-activation and
  forward-compatible with fmParallel.
- NO change to the four publish helpers' dispatch logic, branch tags, source-request sets, or pin behaviour;
  NO change to §6.3 prose, the constants, MAX/MIN_RETAIN, or any AS scope.
- SINGLE-ACTIVATION scope only (SR-D-04): write it to the final lock discipline so the concurrent
  (fmParallel) case adds validation/contention stress, not a restructure (FI-06/07). No global flag, no
  caller-side cache peek, no live-path shortcut justified by "only one activation today."

---

## 6. Deliverable + cadence

**Coder deliverable: the patch only.** Not the harness, not the run commands.

1. **Read-first outcome** (confirm §1, esp. the single-common-point reality and the observation API shape) +
   **proposal** (answer §3: which placement, with source reasoning) — for designer review. NO patch yet. (The
   designer prepares the proof harness in parallel — see below — so the coder does not and should not sketch
   it.)
2. After approval: canonical-LF patch (`git diff -U10`) — `cnr3_arInitial.cpp` (the single observation call +
   the fixed-text temporary KDT line) + `cnr3_build_config.h` (marker bump only). DLL-project-only; cache-core
   selftest UNCHANGED. This is the whole of the coder's deliverable.
3. The coordinator applies the patch, builds both projects Debug+Release, runs the four-way (expect UNCHANGED
   **54/54 / 54/54 / 53/54 exit 1 / 54/54** — proves the cache core is undisturbed), AND runs the
   **designer-provided** `test_W2_*` A/B harness that drives all four branches.
4. The designer verifies the harness result: (a) the KDT shows `HOT-ZONE-OBSERVED` with the correct N on each
   of the four branches; (b) output bytes identical to the pre-W.2 build; (c) the four-way count is still 54.
   On PASS -> commit.

**Designer deliverable (separate from the coder's patch): the proof harness.** Adapted from the golden
`test_000_Example_576p50.*` request-order catalogue into a `test_W2_*` fileset, with `SetVideoCache(mode=0)`
added so every branch re-enters getFrame, a single scenario (or two) that hits all four branches, the expected
`HOT-ZONE-OBSERVED N=…` per branch, and the run/grep commands. Run by the coordinator; cross-checked by the
designer against `cnr3_arInitial.cpp` reality before it is relied upon. (The harness's grep string must match
the fixed KDT text in §4 that the coder emits.)

Marker on commit: `CMS07-W.2-hot-zone-observation-arInitial` (or the agreed marker scheme). This delivers the
§7.6 prerequisite; W.3 (the combined live store-and-prune helper, §7.5 six-step order) then wires §7.2 + §7.4
into the live path, relying on the W.2 observation to keep produced output[N] zone-protected.
