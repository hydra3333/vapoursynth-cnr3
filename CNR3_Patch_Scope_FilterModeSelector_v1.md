# CNR3 — PATCH SCOPE: compile-time filter-mode selector — v1

**Marker on success:** `CMS07-SCAFFOLD.filter-mode-selector`
**Baseline:** `CMS07-DIAG.frame-lifecycle-bail-counters` (current committed tree).
**Track:** build-configuration scaffold (same class as `CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY`:
behaviour-affecting ONLY when a non-default choice is selected). **Committed default preserves current
behaviour exactly** (fmUnordered). No cache, diagnostic, or pixel-path code changes.

## 1. Goal, in plain English

The VapourSynth filter mode is currently hardcoded as the single token `fmUnordered` inside the
`createVideoFilter` call (vapoursynth-Cnr3.cpp:573). The project's own phase plan (comments at
vapoursynth-Cnr3.cpp:37-39, 106) has always intended eventual movement `fmUnordered ->
fmParallelRequests -> fmParallel`. To run mode experiments reproducibly — starting with the first-ever
fmParallel run (A2 territory) — the mode becomes a compile-time three-choice selector in
`cnr3_build_config.h`, so any mode run is a one-comment flip on a committed, self-documenting
configuration instead of an untracked hand-edit.

## 2. The selector

**Location: `cnr3_build_config.h`, immediately BELOW the `CNR3_EDIT_VERSION` marker line** (W3X placement
ruling), as the first configuration block. Well-commented, uncomment-exactly-one style, matching the file's
existing conventions:

```cpp
/*
    ---------------------------------------------------------------------------
    FILTER MODE SELECTION — uncomment exactly ONE of the three lines below.

    This selects the VapourSynth filter mode passed to createVideoFilter, i.e.
    how the VapourSynth engine is allowed to DRIVE this plugin. The plugin never
    "sees" the mode directly; it only experiences the request pattern the mode
    permits.

    CNR3_FILTER_MODE_UNORDERED (SHIPPING DEFAULT)
        One getFrame activation at a time (serial compute), but the engine may
        REQUEST frames out of order and keep several requests in flight.
        This is the proven mode all committed behaviour was validated under.

    CNR3_FILTER_MODE_PARALLEL_REQUESTS (phase 2 of the mode plan)
        Serial compute, but the engine issues source-frame requests for several
        activations concurrently. Intermediate step; NOT yet validated.

    CNR3_FILTER_MODE_PARALLEL (final operational target; A2 test territory)
        Multiple getFrame activations may run CONCURRENTLY on the same filter
        instance. This is the mode the cache's locking/pinning/ownership design
        exists to survive, and the mode the bail-after-compute / duplicate
        counters were built to observe. First runs under this mode are
        experiments: expect the race-arm counters (post-compute discards,
        duplicate-winner returns, K/L plan codes) to become reachable.

    Exactly one must be defined; zero or more than one is a compile error.
    The selected mode is printed in the run log at filter creation so every log
    self-documents which mode produced it.
    ---------------------------------------------------------------------------
*/
#define CNR3_FILTER_MODE_UNORDERED 1
//#define CNR3_FILTER_MODE_PARALLEL_REQUESTS 1
//#define CNR3_FILTER_MODE_PARALLEL 1
```

Plus, nearby (or in the same block), the exactly-one guard:

```cpp
#if (defined(CNR3_FILTER_MODE_UNORDERED) + defined(CNR3_FILTER_MODE_PARALLEL_REQUESTS) + defined(CNR3_FILTER_MODE_PARALLEL)) != 1
#error "CNR3 filter mode: uncomment exactly ONE of CNR3_FILTER_MODE_UNORDERED / _PARALLEL_REQUESTS / _PARALLEL in cnr3_build_config.h"
#endif
```

(Coder may implement the count via a cleaner constexpr/macro sum if preferred — the requirement is a hard
compile error on zero or multiple selections, with a plain-english message.)

## 3. Consumption

- `vapoursynth-Cnr3.cpp:573`: replace the hardcoded `fmUnordered` token with a macro/constexpr selected by
  the three defines (e.g. `CNR3_SELECTED_FILTER_MODE`), defined once near the selector or in a small header
  block. No other argument of `createVideoFilter` changes.
- **Mode line in the log:** at filter creation, print one INFO line naming the selected mode in plain text,
  e.g. `CNR3[1] INFO CONFIG: filter_mode=fmUnordered (compile-time selector)`. Place it with/near where the
  edit_version marker is first logged, so every log carries both provenance facts together. Also add
  `filter_mode` to any existing config/startup summary block if one exists (coder to report what exists and
  propose exact placement in the confirm-report).
- The A1 tool spec will later read this line as log provenance; keep the format simple
  `filter_mode=<token>`.

## 4. Fence

- Default build (`CNR3_FILTER_MODE_UNORDERED`) must be behaviourally IDENTICAL to the current commit: same
  mode token compiled, no control-flow change anywhere, diagnostics untouched.
- No change to cache code, diagnostics families, counters, or emission other than the single new mode line.
- R-PROCESS-25: vapoursynth-Cnr3.cpp registration code is proven; the change is the single token
  substitution + the print line, nothing else moved.
- `edit_version` -> `CMS07-SCAFFOLD.filter-mode-selector`.

## 5. Proof gate (default selection)

1. **Canonical 4-way**: 56/56 unchanged (forced-fail 55/56 e1). The selftest binary does not register the
   VS filter, so the selector must compile cleanly there too (coder confirms the guard doesn't trip in the
   selftest translation units).
2. **Byte-identical vs prior commit:** build default Release and `fc /b` its S8 y4m output against the S8
   output of the current committed build (the diagS8 ON artefact from the last gate run is retained and
   valid for this). Identical bytes proves the selector's default changes nothing.
3. **Mode line prints:** `filter_mode=fmUnordered` appears once at creation in the run log; and flipping
   the comment to `CNR3_FILTER_MODE_PARALLEL` compiles and prints `filter_mode=fmParallel` (build-and-print
   check only — no behavioural claims are made or tested for the non-default modes in THIS patch's gate).
4. **Guard check:** zero-selected and two-selected both fail the build with the plain-english error
   (coder demonstrates both in the delivery note; no artefacts committed from these).

## 6. What this patch is NOT

It does not validate fmParallel or fmParallelRequests behaviour — it only makes selecting them
reproducible and self-documenting. The first fmParallel run (planned: plain in-order L1, TINY-100,
plantrace on, no `-r 1`) is an EXPERIMENT under the A2 umbrella, run on the committed selector by flipping
one comment locally; its findings are assessed by the designer against the lifecycle/race counters built
for exactly that purpose. Any defect it surfaces is new work, not a defect of this patch.

## 7. Delivery requirements

Usual R-PROCESS-25 note: exact file:line of every change; confirmation nothing else in the registration
path moved; the two guard-failure demonstrations; placement proposal for the mode line if the suggested
location doesn't fit the existing startup logging.
