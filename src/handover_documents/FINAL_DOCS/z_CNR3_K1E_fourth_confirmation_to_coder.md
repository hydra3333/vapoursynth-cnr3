# K.1E — fourth (and final) confirmation before you produce the branch-(c) patch

This is the fourth point I owed you, alongside the three already confirmed. With this in
hand you have everything needed to produce the K.1E branch-(c) patch (live
predecessor-present frame-1 compute) for review. Two parts: a marking requirement on any
temporary code, and one thing for you to confirm back.

## 1. Temporary live-path code must be uniformly marked AND self-document its removal

Any code you introduce for K.1E that is temporary — i.e. that exists only to stand the
live getFrame path up incrementally and is meant to be removed or replaced at a later
keystone phase (K.1F/K.1G or the branch-(d)/return-authority work) — must satisfy BOTH of
the following, with no exceptions and no per-coder variants:

- **One greppable marker, the agreed convention (R-PROCESS-12 Part C).** Every temporary
  construct carries BOTH the comment tag `// BEHAVIOURAL-SCAFFOLD:` AND a macro named with
  the agreed `SCAFFOLD_*` prefix, so that *every* active scaffold in the live path is found
  with a single search. Do not invent a new tag or prefix; use the one already in use. A
  temporary construct that is not behind the standard marker is not acceptable, even if it
  is "obviously temporary."

- **An inline unwind annotation — what replaces it, and when.** Immediately at the marker,
  state in a comment (a) what the temporary code does, (b) what the permanent code that
  replaces it will be, and (c) the phase at which it is expected to be unwound (e.g.
  "replaced by the real predecessor-consume path at branch-(c) completion" /
  "retired when K.1B real-VSFrame-return lands"). A reader must be able to tell, from the
  marker alone, that the code is temporary and how it leaves — without reconstructing it
  from the phase history.

The point is that when we come to unwind, the removal set is found by one search and each
item carries its own exit plan; nothing temporary is left to be discovered by accident
later. (This also keeps the cumulative live-path honest under R-PROCESS-21: the temporary
scaffolding is visibly temporary and is not allowed to quietly become load-bearing.)

A note on scope, to be explicit: K.1E *adds* the live predecessor-present compute path; it
must not modify the proven pixel path or the proven cache core to do so. If standing up the
live path appears to require touching proven code, that is a design question to raise before
writing it, not something to fold into the patch (R-PROCESS-21).

## 2. Please confirm: the K.1C passthrough scaffold is fully removed from the committed tree

Before (or as the first part of) the K.1E patch, please confirm that the committed tree on
`dev_cache_manager` contains no residual K.1C live-getFrame passthrough scaffold. Concretely,
please confirm that the following are gone from the committed source (or, if any is
deliberately retained as a permanent guard rather than scaffold, say so explicitly and say
why):

- the K.1C passthrough scaffold guard (e.g. `CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD`);
- the `SCAFFOLD_NOT_FILTERED` path (and any "return the source frame unfiltered" passthrough
  that existed only to prove the getFrame wiring);
- any orphaned passthrough-return branch left behind once K.1D introduced the real
  copyFrame output.

State this in the K.1E patch cover note (which files it touched, and that no K.1C scaffold
remains). I will independently audit the committed `src/vapoursynth-Cnr3.cpp` and
`src/cnr3_build_config.h` against this on my side before applying — so it is worth your
stating exactly what you removed (or kept, with reason), so the two views can be matched.
For clarity, the K.1C R-ARCH-06 fences are a separate matter: if any of those are intended
to remain as permanent correctness guards rather than scaffold, name them and say so — I am
asking only that the *temporary passthrough* scaffold is confirmed gone, not that permanent
guards be removed.

## Summary

- Temporary K.1E live-path code: uniform `// BEHAVIOURAL-SCAFFOLD:` + `SCAFFOLD_*` marker
  AND an inline unwind annotation (what replaces it / when). One search finds them all.
- K.1E adds the live compute path; it does not modify proven code (raise it if it seems to).
- Confirm in the patch cover note that no K.1C passthrough scaffold remains in the committed
  tree (or name what is deliberately kept as a permanent guard, with reason).

With those two settled, please produce the K.1E branch-(c) patch for review under the usual
PDAP cycle (propose -> review -> approve -> apply -> test -> commit), with the `git diff -U10`
wider context and the expected four-way selftest totals.
