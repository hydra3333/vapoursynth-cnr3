# CNR3 — CODER HANDOFF PREAMBLE (mid-DIAG.2b, fresh coder session)

Read this FIRST, then the attached docs in the order given. This bridges a coder-session change
(the previous coder session reached its length limit mid-task). You are the CODER (W3C) in the
three-party discipline: W3X = coordinator (relays, runs builds/commits), W3D = designer/reviewer
(scopes + reviews the DIFF), W3C = you (implement to scope, propose back before commit). Nothing is
lost across the session change: the coordinator and designer hold continuity, and the previous
coder's own written review (attached) carries that session's reasoning.

## Where the project is RIGHT NOW
- Repo: hydra3333/vapoursynth-cnr3, branch dev_cache_manager, VS2026, x64 /arch:AVX2.
- Marshalling-optimisation arc: COMPLETE (~-80%).
- Diagnostics arc: ACTIVE. Committed + pushed so far: DIAG.1 (framework + D-SUM-01 + R-PROCESS-19
  observe-only proof), the selftest skip-pass fix, and DIAG.2a (D-SUM-11 hot-zone writer + D-SUM-10
  prune/eviction/re-churn). Current source baseline = post-DIAG.2a (src.zip attached; it contains
  prune_diag_stats_ and the D-SUM-10 re-churn ring — that confirms it is the right baseline).
- ACTIVE TASK: DIAG.2b — the greenfield cache-core family batch D-SUM-04 (ownership balance),
  D-SUM-05 (cache integrity), D-SUM-08 (store/duplicate). Gates already exist in build_config.h
  (DSUM04_OWNERSHIP_BALANCE / DSUM05_CACHE_INTEGRITY / DSUM08_CACHE_STORE, two-gate #error pattern);
  consume them UNCHANGED. All three families are greenfield (no structs/hooks/writers yet).

## The standing rules that always apply (do not violate)
- R-PROCESS-19: observe-only. With a family's CNR3_DIAG_COMPUTE_* macro OFF, its struct/hooks/writer
  must compile OUT and behaviour must be byte-identical. This is the arc's exit gate.
- R-PROCESS-21: additive placement only. Hook at existing sites; do NOT restructure any proven method
  to host a diagnostic. If clean hosting needs a control-flow change, STOP and propose it first.
- Lock discipline: observe under the cache lock; snapshot; RELEASE; then format+write stderr OUTSIDE
  the lock. Never emit stderr inside a cache/CMS lock. Reuse cnr3_diag_write_line ([DSUM-SUMMARY] tag;
  NOT [KDT-SUMMARY] — that is Keystone dev-trace and collides). Flush per line (the writer default).
- Constness: where a hooked method is const, use a mutable diagnostic member (Option A), consistent
  with the existing mutable cache_mutex_ and DIAG.2a's mutable prune_diag_stats_.
- NEVER git stash (it has broken this repo twice). Use git switch -c wip-name if you must set work aside.
- Read the REAL current source (attached src.zip). Review the DIFF, not the summary. Propose the patch
  back to the designer for review BEFORE any commit; the coordinator runs the real VS2026 proof gate.

## How DIAG.2b got to v2 (why the scope looks like it does)
The PREVIOUS coder session produced a pre-patch inventory review (attached:
CNR3_DIAG2b_pre_patch_inventory_review.md) that CORRECTLY caught three things the designer's v1 scope
got wrong, all verified against source:
  1. D-SUM-04 as a global VSFrame ref balance is UNPROVABLE in DIAG.2b — RAII releases happen via
     Cnr3OwnedFrameRef reset()/destructor/transfer_to_caller() outside hookable call sites, so a naive
     acquired-minus-released counter false-reports leaks. Correct design (which the build_config.h gate
     comment ALREADY prescribes): two narrow provable balances — a slot PIN balance and a LOOKUP-REF
     HANDOFF invariant (acquired == released_by_cache_core + transferred).
  2. D-SUM-05 should hook the single central cache_state_invariants_hold_locked(), not the ~8 scattered
     invariant_violation returns (brittle/invasive).
  3. D-SUM-08 should read Cnr3CombinedStoreAndPruneSummary at the combined store/prune WRAPPER level
     (after store outcome is known), not inside DIAG.2a's prune observer; and count AS2 checkpoint
     promotions only (production-dup promotion is not exposed in the summary — label honestly).
The designer VERIFIED all three against current source and ACCEPTED them. The v2 scope (attached:
CNR3_Patch_Scope_DIAG2b_ownership_integrity_store_v2.md) folds them in. That review is YOUR predecessor
session's reasoning — inherit it.

## Your immediate task
1. Read: this preamble -> the Coder Restart Intro + standing rules (coordinator provides) -> the
   Condensed Plan v1.5 (arc position) -> the DIAG.2b v2 scope -> the v1 inventory review (predecessor
   reasoning) -> the current src.zip.
2. Do the v2 scope's §8 TARGETED CONFIRM (not a fresh broad investigation — the big questions are
   resolved). Key point to confirm: the two D-SUM-04 balances are both fully observable at the named
   sites and net to ZERO — especially whether lookup_refs_transferred is cleanly detectable at the
   OwnedFrameRef adoption/handoff point, and whether any OTHER cache-core lookup-ref release/transfer
   path exists beyond the named ones (a missed path is the one thing that would false-report a leak;
   the S-series zero-balance test is the backstop).
3. Report the §8 confirm back to the designer. Then generate the DIAG.2b patch against src.zip.

## Acceptance criterion to keep in view
D-SUM-04's pin_balance == 0 AND lookup_ref_balance == 0 at teardown across ALL S-series runs is the test
that the siting is COMPLETE. A non-zero balance with no real leak means a missed acquire/release/transfer
path — iterate the siting, do not accept it. That is the convergence target for this family.
