# CNR3 Initial Coding Plan v001

## Status

Approved initial coding plan for the CMS07.0 restart.

This document records the agreed text-only layout and phase plan for beginning CNR3 CMS07.0 implementation. It is a planning document only. It does not itself create files, rename files, copy salvage code, update Visual Studio project files, wire `getFrame`, or implement any behaviour.

The immediate approved path is:

```text
CMS07-A.1  layout-signoff
CMS07-A.2  project-skeleton-introduction
```

CMS07-A.1 is approved. CMS07-A.2 may proceed only through explicit, reviewable change instructions.

## Controlling authority

The controlling design authority is the latest prevailing CMS, currently CMS07.0.

If this plan conflicts with CMS07.0 on a design, cache-core, reference-count, recovery, atomic-scope, VapourSynth lifecycle, constant, instrumentation, or first-milestone point, CMS07.0 wins and this plan is corrected.

If this plan conflicts with the Production Spec §3A on register-owned process, coding, diagnostics, salvage, governance, or handover rules, Production Spec §3A wins and this plan is corrected.

The diagnostics companion specification `cnr3_diagnostics_specification_v1_2.md` and later is approved for implementation planning. It remains subordinate to CMS07.0 and Production Spec §3A where it restates their rules.

## Clean restart rule

This is a clean CMS07.0 restart, not a continuation of CMS06.x.

Do not treat old chats, old memories, CMS06.x phase state, old handover B/C material, old source layout, or old source code as active authority.

Old code may be used only as dead reference material for verified salvage, and only where explicitly approved. Old code must not be copied into new `.h/.cpp` files without explicit per-case approval.

The old `.txt` files remain reference-only.

## No-action rule

Until a phase explicitly authorises a change:

```text
Do not create files.
Do not rename files.
Do not copy old salvage code.
Do not update .vcxproj / .filters.
Do not wire getFrame.
Do not change mutex or lock scoping.
Do not implement cache behaviour.
Do not implement pixel salvage.
```

CMS07-A.2 is the first phase that will introduce skeleton files and project metadata edits, and only by exact change instructions.

## Comment and notes preservation rule

Comments and notes must preserve meaning, context, rationale, invariants, and future-maintainer safety.

Do not compact or remove comments merely for brevity when they explain:

```text
ownership or lifetime
VSFrame reference discipline
pin / unpin discipline
lookup-ref acquired/released/transferred accounting
atomic scopes and lock boundaries
V5 firewall reasoning
frameData allocation/discharge/free discipline
checkpoint-vs-pin distinction
hot-zone-vs-pin distinction
previous OUTPUT vs source[n-1]
scene-change recursive reset
diagnostic gate semantics
temporary proof scaffold boundaries
high-bit-depth arithmetic / overflow prevention
```

Obsolete CMS06-era comments should not be carried forward if they would mislead. Where removed, they should be replaced with CMS07-correct explanation if the underlying concept still needs maintainer context.

## Diagnostics and temporary proofing rule

There are three distinct categories:

```text
1. Production code
   Required for correctness.

2. Ongoing diagnostics
   D-SUM-* summaries.
   DIAG_* names.
   Observe only.
   Print to stderr only.
   Must not alter correct output or control flow.
   Must not be required for production correctness.

3. Temporary proof scaffolds
   SCAFFOLD_* names.
   Clearly bounded comment blocks.
   Temporary proof-only code.
   Must be easy to locate and remove.
   Must not become required for production correctness.
```

Diagnostic summaries are implemented pragmatically when their related mechanisms exist. They are not themselves a driver for implementing functionality before the design calls for that functionality.

Temporary proofing diagnostics are not the same as ongoing diagnostic summaries, even if they reuse some counters. They must be clearly marked as temporary proofing blocks between bounded comments.

No formatting, printing, heap-heavy summary construction, or long-running diagnostic work may occur inside CMS atomic/locked scopes.

## Instance identity rule

A process-wide atomic monotonic allocator may assign human-readable `instance_id` values at filter instance creation.

This allocator is only for diagnostics and log separation.

It is not shared filter state and must not be used for:

```text
cache ownership
frame lookup
scheduling
recovery
cross-instance communication
```

Each CNR3 filter instance owns its own:

```text
cache manager
diagnostics accumulators
processing configuration
response tables
memory statistics
runtime state
```

The instance-ID allocator is the only intended cross-instance mechanism in the initial layout.

## Layout principles

The layout should make the CMS07 separation physically hard to violate.

```text
Cache manager:
    Owns VSFrame* reference slots, ordered index, pools, pins, per-invocation
    pin-list, checkpoints, hot zones, prune, recovery planning, validation,
    and cache diagnostics.

Pixel/frame processing:
    Receives source/current frame plus explicit previous OUTPUT and produces
    pixels. It must not find, cache, pin, recover, prune, or schedule anything.

Response tables and memory diagnostics:
    Cache-independent utilities.

VapourSynth integration:
    Later only. Owns parameter parsing, frameData, requestFrameFilter /
    getFrameFilter, VS error mapping, and calls into cache/pixel layers once
    the isolated cache-core milestone is proven.
```

## Proposed source/header layout

### `src/cnr3_build_config.h`

Purpose:

```text
central compile-time gates
diagnostic compute/print gate pairs
temporary proof/scaffold gates
no behaviour-critical production code hidden behind disabled diagnostic/proof gates
```

Contents should include:

```text
CNR3_DIAG_COMPUTE_DSUMxx_*
CNR3_DIAG_PRINT_DSUMxx_*
compile-time checks that print gates cannot be enabled without compute gates
CNR3_SCAFFOLD_* gates only for explicitly temporary proof blocks
large visible comments explaining DIAG_* vs SCAFFOLD_* semantics
```

This file should not include VapourSynth headers unless unavoidable. It should mostly define build-time switches.

### `src/cnr3_common.h`

Purpose:

```text
very small common project definitions
no old strict cache
no CMS06 output cache
no old proof phase fields
no monolithic old Cnr3Data
```

Possible contents:

```text
small integer helpers where genuinely shared
project version/edit string
Cnr3InstanceId / instance identity helper if appropriate
small common status type only if it does not grow into a god-struct
common include hygiene only if necessary
```

Do not repeat the old `cnr3_common.h.txt` shape. The old file mixed parameters, old strict cache, CMS06 cache manager, proof diagnostics, memory diagnostics, response tables, and runtime state in one struct. That coupling must not return.

### `src/cnr3_instance_config.h`

### `src/cnr3_instance_config.cpp`

Purpose:

```text
hold immutable or mostly immutable per-instance settings after create
keep plugin parameter parsing separate from cache internals
hold instance identity without introducing shared runtime state
```

Likely structures:

```text
Cnr3InstanceIdentity
    instance_id

Cnr3UserParameters
    mode
    ln/lm/un/um/vn/vm
    scdthr
    scene_chroma
    blend option if retained

Cnr3FormatConfig
    width/height
    format id / color family / sample type checks
    bits_per_sample
    sample_peak
    bytes_per_sample
    subSamplingW/subSamplingH
    frame byte size used for cache ceiling

Cnr3ProcessingConfig
    scaled thresholds
    scene_change_threshold
    response table config and/or built response tables later
```

The cache manager should not depend on pixel-layer parameters it does not need.

### `src/cnr3_owned_frame_ref.h`

### `src/cnr3_owned_frame_ref.cpp` optional

Purpose:

```text
move-only RAII wrapper around one owned VSFrame* reference
baseline mechanism for local ownership safety
explicit release_to_caller / transfer path where a ref is intentionally returned to VapourSynth
explicit reset/free path for local release
```

Important comments to retain/enhance:

```text
wrapper owns exactly one reference
move transfers ownership
destructor frees if still owned
transfer nulls the wrapper
copy is disabled
cache lookup/pin semantics do not live here
```

The RAII owned-ref wrapper is baseline in CMS07.0. It is not optional in the ownership model.

### `src/cnr3_cache_core.h`

### `src/cnr3_cache_core.cpp`

Purpose:

```text
CMS07 cache-manager core
no pixel processing
no response-table construction
no VapourSynth getFrame lifecycle code except unavoidable VSFrame* / VSAPI* ref management
```

Likely internal structures:

```text
Cnr3CacheSlot
    frame_number
    VSFrame* frame_ref
    pin_count
    is_checkpoint
    pool/list/index membership state if needed

Cnr3CacheIndex
    ordered frame-number index

Cnr3CachePoolState
    non-checkpoint pool
    checkpoint pool

Cnr3HotZone
    low
    high
    last_observed_frame
    active/dwindling/retire state as needed

Cnr3InvocationPinEntry
    frame_number
    slot identity or stable handle
    VSFrame* owned lookup ref if applicable
    consumed/null state

Cnr3InvocationPinList
    entries owned by one request/frameData activation

Cnr3RecoveryPlan
    requested_frame
    bounded search low/high
    start frame
    present reused outputs
    holes to compute
    source frame numbers needed later
```

Indicative public surface only:

```text
cnr3_cache_init(...)
cnr3_cache_destroy(...)
cnr3_cache_clear(...)

cnr3_cache_prepare_recovery_plan_for_arinitial(...)
cnr3_cache_store_computed_output_and_pin_for_invocation(...)
cnr3_cache_find_and_pin_present_output(...)
cnr3_cache_unpin_invocation_pin_list(...)
cnr3_cache_prune_bounded(...)
cnr3_cache_validate_integrity(...)
```

This module is where the CMS07 AS1-AS7 operations live.

The exact AS contents must be implemented exactly as CMS07.0 §8.7 defines them. The lock scopes are designer-owned and may not be shrunk, split, merged, reordered, or reinterpreted.

### Cache atomic-scope placement

Use Option A:

```text
Keep AS1-AS7 implementations and comments in cnr3_cache_core.cpp.
Do not split them into a separate atomic-scopes file at first.
```

Reason:

```text
Splitting AS functions into a separate file can hide the state the lock protects.
Keeping AS functions near the cache state improves maintainability and reduces
the chance that a future edit treats an AS as a generic helper.
```

Each AS block/function should carry comments preserving the CMS07.0 §8.7 content and section pointers.

If an AS comment and CMS07.0 §8.7 diverge, CMS07.0 §8.7 wins and the comment is corrected.

### Correct AS1-AS7 comment baseline

The following is the corrected baseline for comments and implementation planning. The exact implementation must still be checked against CMS07.0 §8.7 when coding.

```text
AS1  arInitial plan-and-pin
     CMS07.0 pointers: §8.7, §9.1

     Phase-1 descending bounded search [max(0,N-B), N].
     Pin the start point and every present reused frame.
     Catalogue output holes.
     Append every pin to frameData pin-list.
     Update/slide hot zone(s) for N.

     One indivisible lock acquisition.

AS2  arAllFramesReady per-hole store-and-pin
     CMS07.0 pointers: §8.7, §9.2

     First-in-best-dressed check.
     Store computed output or adopt existing winner.
     Pin it.
     Append to pin-list.

     One lock acquisition PER hole.
     Compute happens outside before this.

AS3  reused-frame pin during ascending fill
     CMS07.0 pointers: §8.7, §9.2

     Confirm output[K] present.
     AddFrameRef under lock.
     Append to pin-list.

     Find-and-pin is one indivisible unit.

AS4  final unpin
     CMS07.0 pointers: §8.7, §9.2

     For every entry on the pin-list, unpin/decrement.

     One lock acquisition for the whole list at end of arAllFramesReady.

AS5  bounded prune decide+detach
     CMS07.0 pointers: §8.7, prune/eviction sections

     Evaluate composite eviction predicate.
     Select up to K victims, greatest-distance-first.
     Detach each victim slot from the index using the central remove helper.
     Collect freed VSFrame* refs into a local list.

     Batch freeFrame occurs outside this scope.

AS6  checkpoint establish
     CMS07.0 pointers: §8.7, §6.3, §6.4

     On store of a grid frame or detected-cut frame, set is_checkpoint.
     Insert into checkpoint pool / ordered index.

     Folded into the relevant AS2 store scope using the same lock.
     Not a separate lock.

AS7  zone retirement / merge
     CMS07.0 pointers: §8.7, §5.5, §5.6

     Test no-pins-in-range plus decay margin.
     Mark zone inactive or merge.

     Performed under the same lock during AS1 or the prune pass.
     Never split.
```

There is no shutdown/clear AS. Shutdown clear is governed by the reference-count / teardown obligation, including release of everything and warning on non-zero pins. It is not an AS6 or AS7 critical-section definition.

### V5 firewall comment baseline

The V5 firewall warning must appear near AS implementation comments:

```text
VapourSynth frame reference counts are internally atomic only for the individual
addFrameRef/freeFrame operation. That atomicity gives no permission to move,
split, shrink, merge, or reorder CMS07 cache lock scopes.

The protected operation is the multi-step cache decision, such as find-then-pin,
store-then-pin, decide-then-detach, or checkpoint-establish-with-store, not merely
the refcount bump.
```

### `src/cnr3_cache_diagnostics.h`

### `src/cnr3_cache_diagnostics.cpp`

Purpose:

```text
ongoing D-SUM cache summaries
no behaviour changes
formatting and printing outside locks
```

Early summaries likely relevant to cache-core proof:

```text
D-SUM-04  Ownership / pin / lookup-ref balance summary
D-SUM-05  Cache integrity / teardown summary
D-SUM-08  Cache store / duplicate-store / first-in-best-dressed summary
D-SUM-10  Prune / eviction safety summary
D-SUM-11  Hot-zone operation summary
```

Later, once recovery exists:

```text
D-SUM-03  Recovery-search summary
D-SUM-12  Recovery planning / hole-filling summary
D-SUM-13  Recalculation histogram
```

Diagnostics are hard gates when assigned to a phase. A partial fail is a fail.

### `src/cnr3_memory_diagnostics.h`

### `src/cnr3_memory_diagnostics.cpp`

Purpose:

```text
CMS07 / D-SUM-02 memory diagnostics
old memory diagnostics pair is approved salvage reference, not direct authority
cache-independent utility
```

Expected responsibilities:

```text
take process/system snapshot
accumulate sample statistics
record baseline / pre-cleanup / post-cleanup / final samples
print D-SUM-02 summary to stderr
```

Memory movement is interpretive. It helps detect leaks, runaway growth, and failure to release after cleanup, but min-to-max movement alone is not proof of a leak. Persistent post-cleanup elevation is more important than normal in-run growth.

### `src/cnr3_response_tables.h`

### `src/cnr3_response_tables.cpp`

Purpose:

```text
response-table construction and lookup
cache-independent
no Cnr3Data dependency
no direct cache dependency
```

Likely structures:

```text
Cnr3ResponseTableConfig
    table offset/size or CMS07/CNR2-compatible equivalent
    sample_peak
    scaled thresholds
    mode narrow/wide flags

Cnr3ResponseTables
    table_y
    table_u
    table_v
```

The pixel-layer arithmetic and response-table construction are not open to independent redesign. CMS07.0 §13 V8.1 specifies the pixel-layer arithmetic and salvage rules.

### `src/cnr3_frame_processing.h`

### `src/cnr3_frame_processing.cpp`

Purpose:

```text
luma copy
downsampled-luma buffer
scene-change detection
recursive chroma blend using explicit previous OUTPUT
no cache lookup
no pinning
no recovery planning
no source request/retrieve lifecycle
```

Likely processing boundary:

```text
process frame N from:
    source[N]
    previous_output[N-1] when required
    destination frame
    immutable processing config
    response tables
    VSAPI plane access

return:
    success/failure
    scene-change/reset metadata
    whether recursive blend was used
```

The explicit predecessor boundary from the dead code is a useful salvage concept. The strict-streaming wrapper using `old_strict_cache.prev_output` is not.

The pixel layer must use previous OUTPUT, not `source[n-1]`.

CNR2 recovery/predecessor logic is forbidden.

### Pixel arithmetic baseline

Pixel arithmetic is settled by CMS07.0 §13 V8.1 and is a salvage / pixel-layer concern, not a cache-core concern.

When implemented in the later pixel salvage phase:

```text
compute in native subsampling
compute at native pixel bit depth
use int64 accumulator for weighted blend
use shift2 = 2 * depth, also expressible as depth << 1
do not promote/demote pixels to a separate working format
restrict to planar YUV 420/422/440/444, 8..16-bit
adopt CNR2 response-table construction
adopt downSampleLuma
adopt in-compute scene-change accumulation/threshold
```

Weighted blend formula:

```text
dst = (weight*prev + (shift - weight)*cur + shift1) >> shift2
```

Where:

```text
prev = previous OUTPUT sample
cur = current source sample
weight is int64
shift2 = 2 * depth
```

Do not re-derive a different integer scheme. If the spec formula is believed wrong, raise a proposed CMS change rather than implementing a different arithmetic path.

### `src/vapoursynth-Cnr3.cpp`

Purpose, later only:

```text
plugin registration
parameter parsing
instance creation/destruction
source frame request/retrieve lifecycle
frameData allocation/discharge/free discipline
calls into cache manager and pixel processor once cache-core proof is complete
```

This file should be thin. It should not contain cache algorithms or pixel loops.

For now, this remains deferred because the first milestone requires no `getFrame` / VapourSynth wiring until the cache core is proven in isolation.

### `src/cnr3_cache_core_selftest.h`

### `src/cnr3_cache_core_selftest.cpp`

or:

```text
src/cnr3_cache_core_test_driver.cpp
```

Purpose:

```text
isolated cache-core proof driver
no VapourSynth getFrame integration
simulate request patterns and frame refs as needed
exercise AS scopes, pin/unpin, store, duplicate store, prune, teardown
```

This may be temporary or semi-permanent. If temporary proofing code is used, it must be bounded with obvious comments and `SCAFFOLD_*` naming so it can be found and removed later.

## Slow work outside locks

The following must remain outside CMS atomic/locked scopes:

```text
source requests
source retrieval
pixel compute
batch freeFrame after detach
diagnostic formatting
diagnostic printing
heap-heavy table construction
heap-heavy summary formatting
long-running proof logic
```

Minimal counter updates may occur inside a lock only where they are part of the CMS-defined atomic state update.

## Proposed upcoming phases/subphases

The restart implementation phases are proposed as CMS07-A through CMS07-K.

These phase names are approved as the initial coding path.

## CMS07-A — Layout and project skeleton, no behaviour

### CMS07-A.1 — layout-signoff

Purpose:

```text
finalise file/header layout
fold in AS register correction
fold in AS5 greatest-distance-first comment
fold in CMS07.0 section pointers for AS comments
approve phase labels
no files changed yet
```

Exit gate:

```text
layout approved
phase labels approved
AS1-AS7 comments corrected against CMS07.0 §8.7
instance-ID boundary recorded
no code changes yet
```

Status:

```text
PASS / approved
```

### CMS07-A.2 — project-skeleton-introduction

Purpose:

```text
create empty/minimal new CMS07 files
add include guards / #pragma once
add durable explanatory comments
update .vcxproj / .filters only to include the new skeleton files and remove stale active .h/.cpp entries
no cache behaviour
no getFrame wiring
no pixel salvage
no old-code copy
```

Exit gate:

```text
Visual Studio project opens cleanly
x64 Debug/Release project metadata intact
old .txt files remain reference only
no getFrame wiring
no cache behaviour
```

## CMS07-B — Build configuration and diagnostics foundation

### CMS07-B.1 — build-config-gates

Purpose:

```text
add cnr3_build_config.h
add D-SUM compute/print gate pattern
add temporary SCAFFOLD_* convention
add compile-time print-without-compute errors
```

Exit gate:

```text
gates compile
DIAG observes only
SCAFFOLD clearly temporary
no production correctness hidden behind disabled gates
```

### CMS07-B.2 — diagnostics-foundation

Purpose:

```text
add minimal diagnostics support types
add stderr-only print helpers if needed
add D-SUM summary formatting helpers
no cache behaviour
```

Exit gate:

```text
stderr only
formatting outside locks by design
summary helpers usable by later D-SUM modules
```

### CMS07-B.3 — memory-diagnostics-salvage-D-SUM-02

Purpose:

```text
convert old memory diagnostics reference into CMS07/D-SUM-02 shape
remove stale CMS06 wording and old cache-manager include
preserve/enhance meaningful notes and legends
no cache dependency
```

Exit gate:

```text
D-SUM-02 can sample and print
memory diagnostics remain interpretive, not proof by themselves unless designated hard gate
```

## CMS07-C — Cache-core data model and RAII ownership

### CMS07-C.1 — owned-frame-ref-wrapper

Purpose:

```text
implement RAII owned-ref wrapper
move-only
free on destruction unless transferred
no cache policy inside wrapper
```

Exit gate:

```text
no copy ownership
transfer nulls source
release/free exactly once
```

### CMS07-C.2 — cache-slot-index-pool-skeleton

Purpose:

```text
add slot model
add ordered frame-number index
add non-checkpoint/checkpoint pool placeholders
add cache manager init/destroy skeleton
```

Exit gate:

```text
slot/index invariants documented
no stale index entries possible by construction
```

### CMS07-C.3 — cache-counters-and-validation-baseline

Purpose:

```text
add ownership/pin/ref counters
add validation summary hooks
add D-SUM-04 and D-SUM-05 initial support
```

Exit gate:

```text
empty cache validates
teardown validates
D-SUM-04/05 can report empty-state PASS
```

## CMS07-D — Pin-list and AS skeleton

### CMS07-D.1 — per-invocation-pin-list

Purpose:

```text
add per-invocation pin-list type
add record/consume/null-on-consume discipline
add cleanup/discharge path
```

Exit gate:

```text
pin-list cleanup idempotent
single ownership documented
cleanup/consume attribution distinct
```

### CMS07-D.2 — AS-scope-stubs-with-lock-boundary-comments

Purpose:

```text
add cache-wide lock skeleton
add AS scope functions/stubs
add exact comments describing inside-lock/outside-lock requirements
preserve CMS07.0 §8.7 AS wording and qualifiers
```

Exit gate:

```text
critical-section comments match CMS07.0 §8.7
AS5 includes greatest-distance-first
AS6 is folded into AS2, not a separate lock
AS7 is under AS1 or prune lock, never split
no shutdown/clear AS exists
no slow work inside lock
```

### CMS07-D.3 — pin-unpin-balance-proof

Purpose:

```text
implement pin increment/decrement mechanics
record pins inside same atomic as pin
end unpin-list path
```

Exit gate:

```text
pin/unpin balance = 0
no underflow
non-zero shutdown pin emits warning
D-SUM-04/05 report clean proof
```

## CMS07-E — Store, lookup, duplicate handling

### CMS07-E.1 — store-helper-first-in-best-dressed

Purpose:

```text
implement single store helper
store ref into slot
first-in-best-dressed duplicate result
loser path frees computed-but-unstored frame
```

Exit gate:

```text
no duplicate overwrite
duplicate discard releases owned ref
D-SUM-08 active
```

### CMS07-E.2 — lookup-pin-record-helper

Purpose:

```text
implement find/pin/record inside one atomic
add lookup-owned-ref accounting if used by that path
```

Exit gate:

```text
lookup acquired == released + transferred
pin recorded in same atomic
no TOCTOU gap
```

### CMS07-E.3 — checkpoint-flag-store-path

Purpose:

```text
implement checkpoint flag on store
keep checkpoint separate from pin
no checkpoint-as-pin language
```

Exit gate:

```text
checkpoint is eviction-protection flag only
checkpoint does not imply pin
```

## CMS07-F — Hot zones and bounded prune

### CMS07-F.1 — hot-zone-state-update

Purpose:

```text
implement zone slide/spawn/merge state
update activity at request-arrival equivalent
no active-liveness claims from zones
```

Exit gate:

```text
hot zones are prune hints only
D-SUM-11 initial active
```

### CMS07-F.2 — composite-eviction-predicate

Purpose:

```text
implement predicate: never evict pinned/checkpoint/in-zone frames
document each protection's separate meaning
```

Exit gate:

```text
eviction never selects pinned/checkpoint/in-zone slot
D-SUM-10 active
```

### CMS07-F.3 — bounded-prune-detach-then-free

Purpose:

```text
decide/detach under lock
select victims greatest-distance-first
batch freeFrame outside lock
K-bound prune pass
```

Exit gate:

```text
no freeFrame inside lock for detached batch
no stale index entries
bounded prune respects K
D-SUM-10 clean
```

## CMS07-G — Isolated cache-core proof milestone

### CMS07-G.1 — synthetic-cache-core-driver

Purpose:

```text
add isolated test/proof driver
simulate frame refs and request orders enough to exercise cache paths
no getFrame
```

Exit gate:

```text
sequential and out-of-order synthetic patterns run
no VapourSynth lifecycle wiring
```

### CMS07-G.2 — ownership-proof-run

Purpose:

```text
prove pin/unpin balance = 0
prove lookup-ref balance = 0
prove no leaks
prove no double-free
prove no invalid eviction
prove shutdown clear releases all
```

Exit gate:

```text
all first-milestone proof obligations PASS
partial fail is FAIL
```

### CMS07-G.3 — Design Compliance Review

Purpose:

```text
run CMS07 design compliance review
confirm AS scopes match CMS07.0 §8.7
confirm no old concepts reintroduced
confirm diagnostics/proof scaffolds obey naming and removal rules
```

Exit gate:

```text
DCR clean or issues explicitly recorded
commit message supplied on PASS
```

This completes the first milestone: cache-core ownership/pinning/eviction proof before behaviour, pixel layer, or `getFrame` integration.

## CMS07-H — Recovery planning, still isolated if possible

### CMS07-H.1 — bounded-descending-search

Purpose:

```text
implement recovery search bounded to [max(0,N-B), N]
search for nearest present output
checkpoint flag irrelevant to search hit
```

Exit gate:

```text
bounded-start honesty failures = 0
D-SUM-03 active
```

### CMS07-H.2 — ascending-fill-holes-plan

Purpose:

```text
identify holes
reuse present frames
compute only genuine holes later
no blanket backward source window
```

Exit gate:

```text
holes identified match plan
source requests correspond to genuine holes only
D-SUM-12 active
```

### CMS07-H.3 — recovery-recalculation-observability

Purpose:

```text
add recalculation histogram
interpret recomputation with ownership/store/recovery summaries
```

Exit gate:

```text
D-SUM-13 active
recalculation does not cause ownership/store errors
```

## CMS07-I — Pixel salvage preparation

### CMS07-I.1 — response-table-module-salvage

Purpose:

```text
implement response tables per CMS07 V8.1 / CNR2
keep module cache-independent
preserve explanatory comments around x/o, raised-cosine weighting, and default mode semantics
```

Exit gate:

```text
response tables build independently
no cache dependency
```

### CMS07-I.2 — pixel-processing-explicit-predecessor-boundary

Purpose:

```text
bring forward explicit predecessor processing boundary
no strict-streaming wrapper
no source[n-1] substitution
no cache policy
```

Exit gate:

```text
pixel layer accepts previous OUTPUT explicitly
pixel layer cannot find/recover/cache predecessor
```

### CMS07-I.3 — native-depth-int64-blend-and-scene-reset

Purpose:

```text
implement CMS07 V8.1 pixel arithmetic
native subsampling/native bit depth
int64 accumulator
shift2 = 2*depth
scene-change fresh-start metadata
```

Exit gate:

```text
no alternative arithmetic scheme
scene-change metadata produced
pixel layer does not set cache flags
```

## CMS07-J — VapourSynth integration preparation

### CMS07-J.1 — frameData-lifecycle-structure

Purpose:

```text
define per-invocation frameData
allocate in arInitial
discharge before free
free on all final/error paths
```

Exit gate:

```text
frameData owns pin-list until discharged
no core cleanup hook assumed
```

### CMS07-J.2 — arInitial-source-request-planning

Purpose:

```text
wire source request planning only after cache/recovery plan exists
request source N plus genuine holes only
no blanket backward window
```

Exit gate:

```text
VS-LIFECYCLE-01 respected
no unrequested source retrieve possible by design
```

### CMS07-J.3 — arAllFramesReady-compute-store-return

Purpose:

```text
retrieve only requested source frames
compute holes in order
store-and-pin each computed output
return N with transfer accounting
final unpin
```

Exit gate:

```text
lookup acquired == released + transferred
returned output counted as transferred
intermediates counted as released
```

## CMS07-K — First real plugin build/test

### CMS07-K.1 — VS2026 project update

Purpose:

```text
add approved source files to .vcxproj / .filters
keep x64 settings intact
no stale active old .h/.cpp entries
```

Exit gate:

```text
Visual Studio project metadata intact
new files visible in project
old .txt reference files not active build inputs
```

### CMS07-K.2 — minimal plugin build

Purpose:

```text
build Debug
build Release
no behavioural tuning yet
```

Exit gate:

```text
Debug x64 builds
Release x64 builds
no unexpected project-setting loss
```

### CMS07-K.3 — controlled VapourSynth smoke test

Purpose:

```text
small clips only
D-SUM required summaries enabled
fail on ownership/cache/lifecycle anomaly
```

Exit gate:

```text
smoke test completes
no ownership/cache/lifecycle hard-gate failures
diagnostic summaries are readable and stderr-only
```

## Immediate approved next target

Proceed to:

```text
CMS07-A.2 — project-skeleton-introduction
```

CMS07-A.2 must be implemented only through exact, reviewable change instructions.

Expected CMS07-A.2 work:

```text
create new empty/minimal CMS07 skeleton files
add durable explanatory comments
add no behaviour
add no getFrame wiring
add no pixel salvage
copy no old code
update .vcxproj / .filters deliberately
preserve x64 Debug/Release settings
remove stale active old .h/.cpp entries from project metadata
keep old .txt files reference-only
```

## Suggested commit message for this planning document

Title:

```text
CMS07-A.1: add initial coding plan
```

Body:

```text
Add the initial CMS07 coding plan for the CNR3 restart.

Record the approved file/header layout, CMS07-A through CMS07-K phase sequence,
diagnostics and scaffold boundaries, instance-ID boundary, and AS register
comment requirements before starting implementation.

Include the corrected CMS07.0 AS mapping:
AS6 is checkpoint establishment folded into the relevant AS2 store scope,
AS7 is zone retirement/merge under AS1 or prune lock, and shutdown/clear is
not an AS scope.

Carry forward the A.1 polish requirements:
AS5 comments must preserve greatest-distance-first victim selection, AS
comments should keep CMS07.0 section pointers, and per-AS comments must
preserve the CMS07.0 §8.7 qualifiers verbatim.
```
