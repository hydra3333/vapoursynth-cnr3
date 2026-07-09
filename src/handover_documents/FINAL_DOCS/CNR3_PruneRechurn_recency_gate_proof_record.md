# CNR3 — PRUNE-RECHURN RECENCY-GATE — proof record

**Change:** `CMS07-DIAG.prune-rechurn-recency-gate`
**Type:** analysis-track diagnostic fix (NOT a DIAG-arc step). D-SUM-10 prune/eviction family.
**Record author:** designer (W3D) — produced in lieu of a coder proof-note (coder chat unresponsive this round).

## What changed

Replaced the misleading D-SUM-10 counter `frames_evicted_then_re_requested` with
`frames_recently_evicted_then_re_requested`, gated on eviction recency.

- **cnr3_cache_core.cpp** (`observe_lookup_miss_rechurn_locked`): hoisted the existing normalized `eviction_gap`
  computation (raw `0 -> 1`) to immediately after the `if (!found) return;`, reused that single value for both
  the counter and the gap histogram (no double compute), and gated the increment on
  `eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP`.
- **cnr3_cache_core.h**: added `CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP = CNR3_CACHE_HOT_ZONE_BACK_RADIUS * 3`
  (45 TINY-100 / 150 NORMAL), eviction-count units.
- **Rename replaces** the old counter across 4 sites: field decl (cnr3_cache_diagnostics.h), increment
  (cnr3_cache_core.cpp), D-SUM-10 emission row (cnr3_cache_diagnostics.cpp), selftest fixture
  (cnr3_cache_core_selftest_main.cpp).
- **Untouched:** full ring scan, newest-eviction selection, top-thrashers, `frames_re_requested_repeatedly`,
  gap histogram, and all cache algorithm. No cache/returned-frame behaviour changed — diagnostic accounting only.

## Why the old counter was wrong

The ring lookup matches the EXACT re-requested frame number, so "requested frame" == "evicted frame" always;
frame-number locality is therefore vacuous and cannot be the discriminator. The old counter incremented on ANY
ring match regardless of how long ago the frame was evicted, so it was dominated by intended far-revisits (jumps
back to long-evicted regions) — legitimate workload, not a cache fault. On S9 it read 481, all of which were
distant re-requests. The only meaningful discriminator is EVICTION-RECENCY (`eviction_gap` = evictions elapsed
since the frame was dropped), which the scan already computes.

## Why Z = 3 x BACK_RADIUS (the empirical valley)

Gap-histogram distributions across all scenarios (patched DLL, TINY-100, BACK_RADIUS=15):

```
scenario   1-10  11-50  51-100  101-500  501-1000   old-raw-counter   nature
S9c           0      0       0        0         0    (n/a)             contiguous, no refetch
S9d           0      0       0        0         0    0                 disjoint jumps, no refetch
S9            0      0       0       98       383    481               far revisits (gap >= 101)
S9e           0     40       0        0         0    (n/a)             recovery-local refetch (gap 18-50)
```

Every evict-then-refetch event is either **recovery-local (gap <= 50)** or a **far revisit (gap >= 101)**; the
**51-100 bin is empirically empty in every run** — a stable structural valley between the two populations.
`Z = BACK_RADIUS + 2 = 17` sat below the entire recovery-local population, excluding the very signal the counter
exists to catch (S9e read 0 — a permanently-failing positive control). `Z = 3 x BACK_RADIUS = 45` lands in the
empty 51-100 valley: it catches recovery-local refetches (S9e's gap-18-50 events) while still rejecting far
revisits (S9's events all >= 101). This is the `<= 3x BACK_RADIUS` upper bound flagged during design; the data
landed exactly there.

## Proof matrix (TINY-100, all D-SUM families + plantrace, -r 1) — ALL PASS

```
S9c contiguous (250, shuffle 5, no jumps)      frames_recently_evicted_then_re_requested = 0    PASS
S9d disjoint jumps fwd+back, no revisit         "                                          = 0    PASS
S9  far revisits (jumps 500..2500)              "                                          = 0    PASS  (481 events all gap>=101, rejected)
S9e recency positive control                    "                                          = 40   PASS  (40 events gap 18-50, caught)
```

Histogram cross-check (qualitative, not byte-exact — Z spans multiple printed bins): the counter equals the sum
of near histogram bins within Z. On S9e, counter=40 == `11-50` bin=40 (all 40 events have gap <= 45). Consistent.

Both directions proven: 0 on clean streams AND on far-revisit noise; non-zero on a genuine recent-eviction
refetch. The counter discriminates recovery-local refetches from far-revisit noise as designed.

**S9e committed scenario** (harness / .vpy):
```
assemble_jump_segments(clip_denoised_base, jump_to_frames=[0, 8], segment_length=108,
                       shuffle_in_zones=False, zone_size=1)   # under TINY-100
```

## Commit hygiene checklist (driver = coordinator this round)

- [ ] Debug|x64 + Release|x64 build clean after the rename.
- [ ] Normal selftests (Debug + Release) + four-way pass; counts UNCHANGED (rename + gate only, no new case).
- [ ] D-SUM-10 emission row alignment eyeballed for the longer label name.
- [ ] `build_config.h` staged at NORMAL baseline: TINY-100 off, plantrace off/default (proof toggles NOT committed).
- [ ] Stage only the 5 src files (`git add src/...`); confirm no stray files.
- [ ] S9e scenario preserved in the diagnostic .vpy if it is to be kept as the committed positive control.

## Deferred (explicitly out of scope; do not bundle)

- Maintainability refactor: split `observe_lookup_miss_rechurn_locked` into scan + per-purpose observers
  (counter / histogram / top-thrashers). Its own future scope + byte-identical-output proof.
- Any second/independent "local-window eviction pressure" metric.
