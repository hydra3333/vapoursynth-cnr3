# CNR3 Memory Diagnostics -- Formatted Output Specification v3.4

**v3.4 (supersedes v3.3):** removed `process_pagefile_usage` from the Other block (== process_private_usage; kept the well-labeled core metric). DYNAMIC accumulated set now 13. This closes the metric-cluster cleanup -- no remaining duplicates or mislabels.

**v3.3 (supersedes v3.2):** also REMOVED `perf_physical_total` (duplicate of system_total_phys) from Static totals, and `system_avail_pagefile` + `system_used_pagefile` (commit-family, not pagefile) from the Other block. DYNAMIC accumulated set now 14. Static totals = system_total_phys, perf_kernel_total. `process_pagefile_usage` (== process_private_usage) flagged as the last remaining duplicate candidate. See overlay directive v3 for byte-exact layout.

**v3.2 (supersedes v3.1):** `commit_limit` and `system_total_pagefile` REMOVED ENTIRELY -- both are the same elastic value (RAM + current pagefile) wearing a fixed-"total/limit" label, which misleads; `commit_total` (the actual commit charge) is retained as the clear commit metric. Dynamic set stays the original 16. Static totals = system_total_phys (+ perf_physical_total [duplicate candidate], perf_kernel_total). See the combined overlay directive v2 for the byte-exact layout. 

**v3.1 (supersedes v3):** commit_limit reclassified STATIC->DYNAMIC (elastic = RAM + current pagefile; accumulated min/avg/max, shown with deltas); system_total_virtual (128TB address space) DROPPED; system_total_phys printed as the top static context line of the core table (label "Total Physical Memory") AND retained in Static totals; core-table row order revised (see the combined overlay directive for byte-exact layout); column widths kMemMetricW=24/kMemValW=14/kMemPctW=11 (fixes 134,217,727.88 overflow) with headers derived from the constants. **Supersedes v2.** Read in conjunction with the CMS (v7.15+) and the DIAG.4 scope/decisions. Changes from v2:
(1) gating is PURE COMPILE-GATE (CNR3_DIAG_COMPUTE/PRINT_DSUM02_MEMORY two-gate; the v2 `d->debug` runtime gate is
DELETED -- no debug field exists in live source and no committed D-SUM family uses one); (2) the post-cleanup
snapshot is enabled by a NEW UNGATED PRODUCTION teardown `output_cache.clear()` (Section 7 -- its own reviewable
production change, NOT a diagnostic); (3) `Cnr3MemoryStats` accumulation is WIDENED to all dynamic metrics
(Section 4); (4) the printed core tables stay EXACTLY as v2, with a new "Other memory statistics" block below
them (Section 5); (5) interval default 1000, `<= 0` disables the periodic (Section 3).

## 1. Purpose
Unchanged from v2: process/system memory movement at defined lifecycle points, delta columns against the
baseline taken at filter creation, for leak/runaway-growth/pressure detection. Trend correlation, not ownership
attribution (no attempt to split VS memory from CNR3 memory).

## 2. Snapshot points
| Point | When | Label | Legend |
|---|---|---|---|
| Baseline | end of cnr3_create_filter, after successful Cnr3FilterData init | `at cnr3_create (baseline)` | Yes |
| Periodic | arAllFramesReady, `interval > 0 && frame > 0 && frame % interval == 0` | `frame=N` | No |
| Pre-cleanup | cnr3_free_filter, after existing D-SUM/plantrace summaries, BEFORE the production clear | `before cache clear` | No |
| Post-cleanup | cnr3_free_filter, immediately AFTER the production clear (Section 7) | `after cache clear (clear=<status>)` | No |
| Summary | cnr3_free_filter, before `delete data` | `summary (N samples)` | Yes |

All output gated on the D-SUM-02 compile gates ONLY. Emission via `cnr3_diag_write_line` +
`cnr3_diag_flush_stderr` exclusively (never cache_diagnostics, never raw fprintf, never stdout). No formatting
or printing inside any cache atomic/locked scope.

## 3. Periodic interval
`inline constexpr int CNR3_MEMORY_DIAG_FRAME_INTERVAL = 1000;` sited in `cnr3_build_config.h` adjacent to the
D-SUM-02 gate block. `<= 0` disables the periodic snapshot only (all other points unaffected). Compile-time only.

## 4. Metrics: sample everything, accumulate the dynamic, print core + other
**Sampling (`Cnr3MemorySnapshot`): SALVAGE WHOLE -- no field dropped.** Every process/global/performance field
the superseded sampler produces is captured at every snapshot point (zero marginal cost).

**Accumulation (`Cnr3MemoryStats`): WIDENED to all DYNAMIC metrics** so future promotion to print is a
print-row-only change. Three classes (coder verifies each field's class against the real struct):
```text
DYNAMIC (min/avg/max accumulated):
  process_working_set        process_private_usage      process_pagefile_usage
  system_memory_load_pct     system_avail_phys          system_used_phys
  system_avail_pagefile      system_used_pagefile
  system_avail_virtual       system_used_virtual
  commit_total               commit_limit               perf_physical_avail
  perf_physical_used
  perf_system_cache          perf_kernel_paged          perf_kernel_nonpaged
PEAKS (single running max, no delta):
  peak_working_set           peak_private_usage(=peak pagefile)     commit_peak
STATIC TOTALS (stored once from baseline, no accumulation -- they do not move within a run):
  system_total_phys          perf_physical_total        perf_kernel_total
  (commit_limit MOVED to DYNAMIC in v3.1 -- it is elastic; system_total_virtual DROPPED -- useless 128TB const;
   system_total_pagefile == commit_limit, candidate for removal on request)
```
Rationale: min/avg/max of a constant is noise; statics stay printable (once) without accumulator plumbing.

## 5. Table formats
### 5.1 Core snapshot table -- UNCHANGED from v2 (it is the product of settled discussion)
Exactly the v2 layout and column rules: header `CNR3 memory: instance=I, <label>`; metric field 24 wide
left-aligned; numeric columns 10 wide right-aligned 2dp; Delta(MB) 10 wide signed; Delta(%) 12 wide signed;
baseline row deltas `+0.00`/`+0.00%`; `Start (MB)` = baseline value on every snapshot. Rows: the core five
(process_working_set, process_private_usage, system_avail_phys, system_used_phys, commit_total) + the two peak
rows (`(cumulative peak, no delta)`).

**Every physical line carries the constant-width anchor prefix** `CNR3[<instance>] INFO D-SUM-02:
[DSUM02-SNAPSHOT] ` (snapshots) or `[DSUM02-SUMMARY] ` (summary). Constant prefix preserves column alignment.

### 5.2 NEW -- "Other memory statistics" block, printed BELOW the core table in every snapshot
Same column geometry as 5.1 (24/10/10/12), same Now/Start/Delta/Delta% semantics:
```text
  Other memory statistics:
  process_pagefile_usage       XX.XX      XX.XX     +XX.XX      +XX.XX
  system_memory_load_pct       XX.XX      XX.XX     +XX.XX      +XX.XX   (percent, not MB)
  system_avail_pagefile     XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  system_used_pagefile      XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  system_avail_virtual      XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  system_used_virtual       XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  perf_physical_avail       XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  perf_physical_used        XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  perf_system_cache         XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  perf_kernel_paged         XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  perf_kernel_nonpaged      XXXXX.XX   XXXXX.XX    +XXX.XX      +XX.XX
  commit_peak               XXXXX.XX   (cumulative peak, no delta)
```
`system_memory_load_pct` prints in the same numeric columns as percent values, suffix-noted once. All other
values MB 2dp as in 5.1.

### 5.3 NEW -- "Static totals" block, printed at BASELINE and SUMMARY only (they do not change mid-run)
```text
  Static totals (MB):
  system_total_phys         XXXXX.XX
  system_total_pagefile     XXXXX.XX
  system_total_virtual      XXXXX.XX
  commit_limit              XXXXX.XX
  perf_physical_total       XXXXX.XX
  perf_kernel_total         XXXXX.XX
```

### 5.4 Legend -- v2 5.2 text unchanged, printed at baseline + summary only, PLUS one added line:
```text
  other_statistics       secondary dynamic metrics accumulated for future analysis; interpretive only.
```

### 5.5 Summary table -- v2 5.3 core Min/Avg/Max UNCHANGED, then the Other block with Min/Avg/Max in the same
geometry, then the Static totals block (5.3 above), then the legend.

## 6. Interpretation caveat (bind into the legend commentary)
The production clear (Section 7) releases CNR3's frame references (`freeFrame`). If VapourSynth's own cache
still holds references, buffers remain in VS's pool: the post-cleanup drop reflects CNR3's CONTRIBUTION
released, not bytes returned to the OS. Persistent post-cleanup elevation of process_private_usage remains the
best leak-suspicion signal (v2 guidance unchanged).

## 7. THE PRODUCTION TEARDOWN CLEAR (new, UNGATED, its own reviewable change -- NOT a diagnostic)
### 7.1 What and where
`cnr3_free_filter`, after the existing D-SUM/plantrace summaries, before `delete data`:
```text
existing D-SUM summaries -> plantrace clean-end dump -> [DSUM02 pre-cleanup snapshot]
-> const Cnr3Status teardown_clear_status = data->output_cache.clear();   // UNGATED, all builds
-> [DSUM02 post-cleanup snapshot, label carries clear=<status>] -> [DSUM02 summary]
-> free source -> delete data
```
UNGATED is the point: the clear runs identically with diagnostics compiled out, so the memory family remains
strictly OBSERVE-ONLY (a diagnostic gate must never decide whether a cache operation happens). The diagnostic
only reads around it.

### 7.2 What clear() does (verified against cnr3_cache_core.cpp:1724 + clear_locked at :3966)
Takes cache_mutex_; verifies cache invariants; scans every slot and REFUSES (`lifecycle_violation`, cache
untouched) if ANY `pin_count != 0`; detaches `slots_` by swap; clears `frame_index_`, `checkpoint_slot_
positions_`, `hot_zones_`; re-verifies invariants; releases the lock; the detached `Cnr3OwnedFrameRef`s then
destroy OUTSIDE the lock (freeFrame -- CMS07 no-freeFrame-under-lock rule preserved). It releases exactly the
references the default destructor would release at `delete data`, just earlier and explicitly; it touches no
pin accounting, no checkpoint promotion, no diagnostic counters. Fail-safe: on any precondition failure it is a
no-op and the destructor still frees everything.

### 7.3 Why pins are expected to be zero at this point (the Q1 analysis -- three independent layers)
```text
L1 STRUCTURAL: pins are per-activation (acquired in arInitial, discharged at every arAllFramesReady exit --
   success AND bail paths, via the proven discharge machinery). Checkpoint status IS NOT A PIN and must not
   change pin_count (cache_core.h:1428). No persistent pin class exists. VS calls the free callback only after
   all frame requests complete, so no activation is in flight at cnr3_free_filter.
L2 EVIDENCE: D-SUM-04 ownership balance (pins_acquired == pins_released, balance == 0) PASSED on the committed
   S-series real runs; D-SUM-12 plan balance == 0. The zero-pins-at-teardown claim is already measured, not
   assumed.
L3 RUNTIME GUARD: clear() itself re-verifies (the pin scan) and declines without side effects if the claim is
   ever false. So even if L1/L2 were somehow violated, the clear cannot corrupt state -- worst case the
   post-cleanup snapshot shows no drop and the status says why.
```
### 7.4 Q2: [[nodiscard]] status handling (the ruling)
Capture the status ALWAYS (`const Cnr3Status teardown_clear_status = ...` -- satisfies [[nodiscard]] in all
builds). NEVER abort teardown on non-ok: the destructor still frees everything; the failure is diagnostic
information, not a fatal condition. Surface it two ways: (a) the post-cleanup snapshot label carries
`clear=<status name>` (compiled-in builds); (b) macro-off builds simply ignore the captured local (it is used
by nothing -- coder confirms no unused-variable warning under /W4/WX; if needed, name it with the project's
intentionally-unused idiom). A non-ok status in a real run is a REPORTABLE FINDING (it would mean a pin
survived teardown = lifecycle bug) -- the label makes it visible in the same log being read anyway.

### 7.5 CODER VERIFICATION CHECKLIST for the clear (confirm veracity + safety before patch; designer will
cross-check each item cold at diff review):
```text
V1 Cite every pin-acquire site and its guaranteed matching discharge (incl. all 65 bail paths' discharge
   behaviour) -- confirm no path leaves a pin across activations.
V2 Confirm checkpoints, hot zones, and the recently-evicted ring hold NO pin_count (checkpoint-is-not-a-pin,
   cache_core.h:1428) -- i.e. no persistent pin class.
V3 Confirm VS lifecycle: the free callback cannot run concurrently with getFrame activations (no in-flight
   pins possible at cnr3_free_filter).
V4 Confirm clear() idempotence vs the destructor: after a successful clear(), `delete data` destructor finds
   empty containers and frees nothing (no double-free); after a DECLINED clear(), the destructor still frees
   everything exactly as the current committed code does.
V5 Confirm clear() is exercised by the existing cache-core selftests (cite which test), so production is not
   the first invocation; if NOT covered, propose the minimal selftest addition.
V6 Confirm the [[nodiscard]] capture compiles warning-clean in macro-off /W4 (V7 idiom if needed).
V7 Proof-gate addition: in the A/B run, assert post-cleanup label shows clear=ok, and cross-read the same
   log's D-SUM-04 balance == 0 (ties L2 to L3 in one run).
```

## 8. Gate / fence summary
Compile two-gate only (already wired) + interval constant. Memory module output through cnr3_diagnostics.*
boundary. The ONLY production-behaviour change in DIAG.4 is the Section 7 clear -- ungated, reviewed under
R-PROCESS-21/25 as its own item, with the V1-V7 checklist as its confirm evidence. Everything else is
observe-only. Macro-off (all D-SUM gates off) must be byte-identical INCLUDING the clear (which runs in both).
