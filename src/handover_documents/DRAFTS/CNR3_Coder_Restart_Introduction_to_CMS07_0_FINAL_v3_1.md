# CNR3 — Development restart on the CMS07.0 cache architecture

*(Coder restart introduction — paste this at the start of a new memoryless chat, ahead of
the attached handover pack files.)*

This chat is a **clean CMS07.0 restart** of CNR3 cache-manager development. Do not treat
older CNR3 memories, prior chats, old handover documents, or old source layout as active
implementation authority unless the attached restart pack explicitly says so.

CNR3 is a VapourSynth API4-only, integer-YUV recursive temporal chroma stabiliser. Its
load-bearing difficulty is that:

```text
output[N] depends on source[N] and already-filtered output[N - 1]
```

The predecessor is the already-filtered output, not merely `source[N - 1]`. Modern
VapourSynth scheduling may request frames out of display order, so CNR3 needs a correct
cache/recovery architecture before any parallel-performance work can be trusted.

A new cache architecture has been designed. It reuses some concepts, but **completely
supersedes the previous CMS06.x cache design and proof path**.

---

## 1. Attachments expected for this restart

Do not proceed from this introduction alone. The following CORE handover files should be
attached:

```text
1. This introduction:
   CNR3_Coder_Restart_Introduction_to_CMS07_0_FINAL_v3_1.md

2. Controlling design:
   cnr3_cache_manager_design_v7_0.md
   (CMS07.0 — included unchanged; controlling design authority.)

3. Project context / standing rules:
   Document_A_CNR3_Project_Context_and_Standing_Rules_v3_1.md
   (generated from the populated Production Spec: canonical context + §3A rules.)

4. Current work plan / first milestone:
   Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_v3_1.md
   (current-state work plan; volatile/subordinate to CMS07.0 and §3A.)

5. Production Spec:
   CNR3_Handover_Pack_Production_Spec_v2_2_POPULATED_3A.md
   (contains the canonical context master §3.2 and populated §3A register-owned rules.)

6. Manifest/checksums:
   CNR3_Handover_Pack_<version>_MANIFEST.md
   (reading order, file integrity, pack status; if not yet attached, say so.)
```

Companion coding-start material may also be attached. It is useful but is **not** part
of the durable handover pack:

```text
7. Current .txt source snapshot after the .h/.cpp -> .txt transition, if available.

8. CNR2 / vscnr2 reference source excerpts/files for pixel-layer salvage, if not relying
   on live lookup.

9. Relevant logs, if any.
```

If **CMS07.0 itself is not attached**, stop and say so. You may comment on this
introduction, but you cannot enumerate rules or propose a compliant layout without the
controlling design.

---

## 2. Hard precedence and old/new separation

Two different situations have opposite responses:

```text
If CMS07.0 conflicts with, or is merely unclear in alignment with, prior material:
    CMS07.0 wins unless the user explicitly says otherwise.

If CMS07.0 itself is silent, ambiguous, or incomplete on an implementation point:
    stop and ask. Do not guess and do not improvise.
```

References to CMS07.0 as controlling mean **CMS07.0 or its later approved successor**.
Specific CMS07.0 section pointers are version-specific and must be re-checked against any
successor. Historical statements about what CMS07.0 superseded stay pinned to CMS07.0.
The filename `cnr3_cache_manager_design_v7_0.md` denotes that specific file.

The old CMS06-era Document B decision log and Document C volatile-state snapshot are
deliberately out of scope as active inputs. CMS07.0 §9A and related sections already
carry forward still-valid hard rules. Excluding the old B/C material is intentional: it
removes the main route by which stale CMS06-era assumptions re-enter.

All existing cache-related code and design is explicitly superseded. Old code is salvage
reference only, and only under the salvage rules in §3A and CMS07.0.

---

## 3. The five highest-risk traps

Do not conflate old and new concepts.

```text
1. Treating pinning as optional/deferred:
   Wrong. Consumer-held pinning is now the mandatory correctness baseline.

2. Reintroducing held-ref-only predecessor reservation:
   Wrong. It is superseded by consumer-held pins on a per-invocation pin-list.

3. Thinking of a checkpoint as a pin:
   Wrong. A checkpoint is a separate eviction-protection flag. There is exactly one pin
   concept: consumer-claim.

4. Treating hot zones as active-frame findability guarantees:
   Wrong. Pins provide active liveness. Hot zones are prune-policy hints only.

5. Reintroducing a blanket bounded-warmup source window:
   Wrong. Recovery uses the CMS two-phase model with dissolved source-window semantics:
   request source N plus genuine holes only.
```

Nothing may be implemented that obstructs the fmParallel end-goal unless it is an
unavoidable, explicitly recorded, temporary stepping-stone that preserves the design path
to fmParallel.

---

## 4. Engineered guards you must respect

### 4.1 Atomic-scope register

CMS07.0 defines an atomic-scope register, AS1-AS7. Treat it as designer-owned and
inviolable.

Every cache critical section is enumerated there, including what happens inside one lock
acquisition and in what order. Implement these scopes exactly. Do not shrink, split,
merge, reorder, or reinterpret the scope contents. If implementation reveals a needed
operation the register does not cover, raise it to the user; do not invent an ad-hoc
lock scope.

### 4.2 V5 firewall

VapourSynth frame reference counts are internally atomic. That protects a single
`addFrameRef` or `freeFrame` operation only. It is not a licence to take a pin outside
the cache lock or to reduce any cache critical section.

The protected thing is the multi-step cache decision, such as find-then-pin or
decide-then-detach, not merely the reference-count bump.

### 4.3 VapourSynth lifecycle rule

Any source frame retrieved in `arAllFramesReady` must have been requested in `arInitial`
of the same activation. Request planning happens at `arInitial`; do not retrieve source
frames that were not requested for that activation.

---

## 5. §3A is now populated: rule enumeration is verification, not first population

The Production Spec now contains a populated §3A Prevailing Rules Register for
REGISTER-OWNED rules. Your job is still to enumerate the prevailing rules back to the
user, but the purpose is now **verification/reconciliation**, not first population.

Distinguish two kinds of rules:

```text
REGISTER-OWNED rules:
    Authority, pack governance, process, architecture/salvage, retired-fact entries,
    resolved candidate dispositions, and related standing rules already recorded in
    Production Spec §3A.

CMS-DEFINED / HANDED-OFF rules:
    Design/cache-core/reference-count/VapourSynth-lifecycle/recovery/constant/
    instrumentation/atomic-scope/first-milestone rules defined in CMS07.0. These are
    not duplicated, indexed, renamed, or re-IDed in §3A.
```

If you identify an apparent missing rule, conflict, ambiguity, or prior-context-derived
candidate, raise it explicitly for user decision. Do not silently treat it as controlling.

---

## 6. Build / transition state

The restart has a deliberate source transition:

```text
Rename all existing .h/.cpp files except VapourSynth4.h and VSHelper4.h to .txt.
```

This keeps old code available as reference for verified salvage but removes it from the
active build. The old binary need not build; GitHub CI may break for now. Builds will be
done in Visual Studio 2026.

Do **not** perform this rename merely from reading this introduction. Do not create files.
Do not copy salvage code. Do not integrate `getFrame`. Do not change mutex/lock scoping.
This brief is **read-understand-propose**, not act.

---

## 7. First milestone — prove ownership before behaviour

Before any VapourSynth wiring, build the cache-manager core in isolation:

```text
- slot = VSFrame* ref + frame number + pin_count + is_checkpoint;
- ordered frame-number index;
- non-checkpoint and checkpoint pools;
- hot-zone state;
- per-invocation pin-list;
- single cache-wide-lock skeleton with inside/outside-lock discipline;
- AS1-AS7 implemented exactly as CMS07.0 defines them;
- pin / unpin + pin-list record/discharge;
- single-ownership / null-on-consume;
- discharge-before-free ordering;
- RAII owned-ref wrapper as baseline;
- composite eviction predicate;
- bounded prune: decide+detach under lock, batch freeFrame outside, K-bound;
- single remove helper.
```

Proof obligations before any `getFrame` integration:

```text
- pin/unpin balance = 0
- lookup-ref balance = 0  (acquired == released + transferred)
- no leaks
- no double-free
- eviction never selects a pinned / checkpoint / in-zone slot
- shutdown clear() releases everything, with a warning on any non-zero pin
```

This milestone proves ownership/pinning/eviction discipline before behaviour and before
the pixel layer is brought forward.

---

## 8. Salvage policy

Salvage is the second step, after the new cache core is proven in isolation.

Likely salvage candidates, subject to explicit per-case approval and verification:

```text
- response-table construction;
- memory diagnostics;
- pixel/frame-processing layer;
- explicit-predecessor pixel-processing boundary.
```

CNR2 / vscnr2 may be used as guidance for pixel maths only:

```text
- response-table construction;
- int64-accumulator weighted blend;
- downsampled-luma;
- in-compute scene-change detection.
```

Never adopt CNR2 recovery or predecessor logic. CNR2 is serialized and substitutes
`source[n-1]` when the previous output is absent. That approximation is exactly what
CNR3's CMS07.0 cache-and-recovery architecture replaces.

Old `.txt` code is not copied into new `.h/.cpp` files without explicit per-case
approval.

---

## 9. Process rules that matter immediately

The authoritative source is Production Spec §3A. The following is only a quick
orientation to likely early failure points:

```text
- Comments: concise, useful, and never safety-incomplete.
- Code updates: exact before/after blocks with uniquely matchable context.
- Phase/SubPhase numbering restarts for CMS07 development.
- PASS or agreed conditional progression should include a Visual Studio / GitHub-style
  commit title and body unless the user asks otherwise.
- Diagnostics are hard gates; a partial fail is a FAIL.
- Diagnostic output goes to stderr, never stdout.
- Human-facing diagnostic summaries must be readable; per-event diagnostics may be
  compact if still clear.
- Observation gates observe only; behaviour-changing scaffolds use SCAFFOLD_* markers,
  not DIAG_* names.
- No printing or long-running work inside locked/atomic scopes.
- Minimise unrelated diffs and do not silently paraphrase agreed rules.
- Any override requires explicit discussion, agreement, and documentation.
```

Consult §3A directly for the complete text.

---

## 10. Proposed layout comes after rule verification

After reading the pack, propose the file/header/structure layout as text only. Include:

```text
- likely .h files and .cpp files;
- internal structures in each;
- includes and dependencies;
- function names/signatures;
- purpose and parameter comments for key functions;
- where the AS1-AS7 operations live;
- where RAII owned-ref handling lives;
- where diagnostics and their compute/print gates live;
- how cache logic is kept separate from pixel/frame processing.
```

Do not create any files until the user explicitly signs off the layout.

---

## 11. Your first response in the new chat

Please respond with:

```text
a) Confirmation of your understanding of the CMS07.0 restart, old/new separation, and
   no-action rule.

b) Any questions or ambiguities you see in CMS07.0 or the handover pack.

c) An enumerated prevailing-rules list for verification/reconciliation, clearly marking
   each item as:
       REGISTER-OWNED (§3A), or
       CMS-DEFINED / HANDED-OFF (CMS07.0).

d) Your proposed file/header/structure layout as a text proposal only — no files yet.
```

If the expected files are missing, say exactly which are missing and limit your response
accordingly.

Do not assume any rule carries over silently. Do not code. Do not create files. Do not
rename files. Do not copy salvage. Do not wire `getFrame`. Do not change lock scoping
without explicit user discussion, agreement, and instruction.
