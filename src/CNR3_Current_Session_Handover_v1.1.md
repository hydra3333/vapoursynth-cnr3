# CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.1  
**Date:** 2026-06-03  
**Status:** Volatile current-state document; update at every session boundary.  
**Current design authority:** CMS06.

---

## C1. Read order for a new chat

Read in this order:

1. `CNR3_Project_Context_and_Rules_v1.1.md`
2. `CNR3_Decision_Log_v1.1.md`
3. `CNR3_Current_Session_Handover_v1.1.md`
4. CMS06 or a later cache design spec that explicitly supersedes CMS06
5. current relevant source files
6. latest logs if the task depends on test evidence

Rules for the new chat:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06 as the detailed cache-manager design authority.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in "Do not implement in the next session".
```

CMS06 is the current cache design authority. Earlier CMS05.x documents are superseded except as history.

If a companion design spec contains a "current implementation state" snapshot, this Document C overrides it for current implementation status.

---

## C2. Repository/code context

Known repository:

```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Important current files:

```text
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_build_config.h
cnr3_response_tables.h
cnr3_response_tables.cpp
cnr3_memory_diagnostics.h
cnr3_memory_diagnostics.cpp
old_cnr3_strict_cache.h/.cpp or old strict cache content, depending on current file layout
```

Current names:

```text
New output cache:
    Cnr3OutputCacheManager
    cnr3_output_cache_*

Old strict cache:
    OldCnr3StrictStreamCache
    old_cnr3_strict_cache_*

Avoid in new code:
    Cnr3CacheManagerV005
    Cnr3CacheManager for the old strict cache
    cache_manager_v005
    cnr3_cache_manager_*
    CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS
    v005 wording in new comments
```

Diagnostic strings may still contain CMS05/CMS05-3A because they identify historical proving phases. CMS06 is the design authority.

---

## C3. Current exact implementation status

Current phase state:

```text
Completed:
    CMS05-3B: readable full diagnostics and teardown proof.
    CMS05-3C: throttled full per-frame output-cache summaries.
    CMS05-3D: compact per-frame store/prune trace.
    CMS05-3E: reduced full-summary cadence to every 100th frame plus near-final/final.
    CMS06 alignment: moved output-cache hot-zone update to arInitial.
    Stale comment cleanup in cnr3_output_cache_manager.h completed.

Current output authority:
    old_strict_cache remains the source of returned output.
    output_cache is not output-authoritative.

Current output-cache role:
    output_cache stores/prunes real produced frames for proving only.
    output_cache is cleared during cnr3_free.
```

CMS06 Section 14 predates the latest diagnostic work completed in this chat. Use this Document C as the current implementation-state authority.

---

## C4. Latest test evidence

Latest successful smoke test used:

```text
vspipe -r 1 --info "D:\TEST\Vapoursynth_x64_R76\test_cnr3-8bit.vpy"

vspipe -r 1 --progress --filter-time "D:\TEST\Vapoursynth_x64_R76\test_cnr3-8bit.vpy" NUL
```

Important `--info` result:

```text
No frame processing occurred.
hot_zone_updates_at_arInitial=0
store attempts=0
clear successes=1
```

Important 10-frame `--progress --filter-time` result after moving hot-zone update to `arInitial`:

```text
hot_zone_updates_at_arInitial=10
hot_zone_new_zone_requests=1
hot_zone_allocations=1
hot_zone_slides=9
hot_zone_max_active_observed=1

store:
    attempts=10
    successes=10
    failures=0
    checkpoint_successes=1
    non_checkpoint_successes=9

prune_after_store:
    attempts=10
    successes=10
    failures=0

invariants:
    invariants_ok=1
    integrity_errors=0
    validation_attempts=20
    validation_successes=20
    validation_failures=0
    ref_balance_errors=0

before clear:
    total_cached_frame_count=10
    addframeref_total=10
    freeframe_total=0
    balance=10

after output_cache clear:
    total_cached_frame_count=0
    addframeref_total=10
    freeframe_total=10
    balance=0
    clear attempts=1
    clear successes=1
    clear failures=0
```

Hard-gate result:

```text
PASS
```

Do not proceed from any future phase if integrity/ref-balance/validation/store/prune/clear counters show unexpected values.

---

## C5. Current diagnostic policy

Current output-cache diagnostics:

```text
Compact CMS05-3A trace:
    one line per processed frame.

Full output-cache summary:
    after create;
    frame 0;
    frame 1;
    every 100th frame;
    one frame before final;
    final frame;
    before cnr3_free cleanup;
    after output_cache clear;
    any store/prune failure.
```

For a 10-frame clip, the log still looks large because full summaries occur at frame 0, frame 1, frame 8, frame 9, and lifecycle points. This is expected.

For longer clips, compact traces give per-frame visibility and full summaries are much less frequent.

Future option, not yet implemented:

```text
Add compact/full diagnostic mode later if long logs become impractical.
```

---

## C6. Immediate next task

Recommended next task:

```text
Begin CMS06 Phase CMS02-F:
    cache-hit reuse under fmUnordered.
```

CMS06 Phase CMS02-F high-level requirements:

```text
- Implement cnr3_output_cache_find_frame_and_add_ref().
- At the start of cnr3_get_frame arAllFramesReady, check output-cache hit.
- If hit, return cached frame via caller-owned reference.
- If miss, proceed with normal strict-path computation for now.
- Instrument cache_hits_at_arAllFramesReady and caller-side lookup-ref counters.
- Use addFrameRef while holding cache mutex.
- Caller must free or transfer lookup-owned refs on every exit path.
- Perform a design-compliance review before CMS02-G.
```

Important prerequisite already satisfied:

```text
cnr3_output_cache_update_hot_zones() now runs at arInitial.
```

Before coding CMS02-F, upload the latest source files, especially:

```text
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_build_config.h
```

---

## C7. Do not implement in the next session unless explicitly chosen

Do not implement these while starting CMS02-F:

```text
- checkpoint recovery / hole-filling walks (CMS02-G)
- bounded warm-up recovery (CMS02-H)
- non-checkpoint pinning (CMS02-I)
- fmParallelRequests wiring (CMS02-J)
- full fmParallel support
- changes to recursive blend maths
- changes to scene-change detection
- mass renaming of CMS05 diagnostic strings
- compact/full diagnostic modes
```

---

## C8. Remaining cleanup/deferred notes

### C8.1 Diagnostic naming cleanup

Some diagnostic strings still say CMS05 or CMS05-3A.

Current interpretation:

```text
CMS06 is the current design authority.
CMS05/CMS05-3A strings identify the historical implementation proving phase.
They are not runtime correctness issues.
```

Priority:

```text
Low. Do not mass-rename now unless doing a deliberate diagnostic wording cleanup pass.
```

### C8.2 Future diagnostic mode

Future option:

```text
Add compact/full diagnostic mode or debug-level option if long logs become impractical.
```

Priority:

```text
Low. Defer until logs become a real obstacle.
```

### C8.3 Non-checkpoint pinning

Status:

```text
Deferred by design.
```

Promotion rule:

```text
If predecessor_missing_when_expected becomes non-zero in realistic testing,
non-checkpoint pinning becomes mandatory before continuing.
```

Reference:

```text
CMS06 Section 4.4.
```

---

## C9. Safety checks before any future commit

Before committing cache-related changes:

```text
Build:
    Debug build must succeed.
    Release build should succeed before larger phase commits.

Run:
    short realclip or blankclip smoke test where relevant.
    targeted test required by the current phase.

Check output-cache diagnostics:
    invariants_ok=1
    integrity_errors=0
    validation_failures=0
    ref_balance_errors=0
    store_failures=0 unless deliberately testing failure paths
    prune_after_store_failures=0 unless deliberately testing failure paths
    cache_addframeref_total - cache_freeframe_total matches total_cached_frame_count before clear
    cache_addframeref_total - cache_freeframe_total is 0 after clear
    clear_successes=1 after teardown when cached frames existed
```

For phases involving lookup-owned references, also require:

```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

at quiescent points.

Hard gate:

```text
If any of the above checks show unexpected values, stop.
Do not proceed to the next task until the discrepancy is understood.
```

Unexpected diagnostic values may indicate broken ownership, mutex discipline, stale cache index, bad pruning, dangling frame risk, or leaked `VSFrame` references.

---

## C10. Recent commit messages or suggested commit messages

Recent commit messages or suggested commit messages:

```text
Phase CMS05-3B: make output-cache diagnostics readable
Phase CMS05-3C: throttle output-cache frame diagnostics
Phase CMS05-3D: add compact per-frame output-cache trace
Phase CMS05-3E: reduce full frame-summary frequency
CMS06: move output-cache hot-zone update to arInitial
Clean up stale output-cache manager phase comment
```

---

## C11. New-chat starter prompt

Use this prompt when starting a new chat:

```text
We are continuing CNR3 development.

Please read the uploaded documents in this order:

1. CNR3_Project_Context_and_Rules_v1.1.md
2. CNR3_Decision_Log_v1.1.md
3. CNR3_Current_Session_Handover_v1.1.md
4. CMS06 cache design specification
5. Current source files/logs

Important:
- The new chat has no memory of prior chats.
- Treat CNR3_Current_Session_Handover_v1.1.md as the source of truth for current state.
- Treat CNR3_Decision_Log_v1.1.md as the source of truth for settled decisions.
- Treat CMS06 as the detailed design reference.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in the current handover's "Do not implement" section.

First, confirm your understanding of the current state and immediate next task.
Then wait for the current code files if they have not already been uploaded.
```
