#include "cnr3_cache_core.h"

/*
    CNR3 CMS07 cache core implementation.

    CMS07-A.2 skeleton only.

    AS placement decision:
        Keep AS1-AS7 implementations and comments in this translation unit.

    Reason:
        Splitting AS functions into a separate file can hide the cache state the
        lock protects. Keeping AS functions near the cache state reduces the
        chance that a future edit treats an AS as a generic helper.

    Correct AS comment baseline for later implementation:

        AS1  arInitial plan-and-pin
             CMS07.0 pointers: section 8.7, section 9.1

             Phase-1 descending bounded search [max(0,N-B), N].
             Pin the start point and every present reused frame.
             Catalogue output holes.
             Append every pin to frameData pin-list.
             Update/slide hot zone(s) for N.

             One indivisible lock acquisition.

        AS2  arAllFramesReady per-hole store-and-pin
             CMS07.0 pointers: section 8.7, section 9.2

             First-in-best-dressed check.
             Store computed output or adopt existing winner.
             Pin it.
             Append to pin-list.

             One lock acquisition PER hole.
             Compute happens outside before this.

        AS3  reused-frame pin during ascending fill
             CMS07.0 pointers: section 8.7, section 9.2

             Confirm output[K] present.
             AddFrameRef under lock.
             Append to pin-list.

             Find-and-pin is one indivisible unit.

        AS4  final unpin
             CMS07.0 pointers: section 8.7, section 9.2

             For every entry on the pin-list, unpin/decrement.

             One lock acquisition for the whole list at end of arAllFramesReady.

        AS5  bounded prune decide+detach
             CMS07.0 pointers: section 8.7, prune/eviction sections

             Evaluate composite eviction predicate.
             Select up to K victims, greatest-distance-first.
             Detach each victim slot from the index using the central remove
             helper.
             Collect freed VSFrame* refs into a local list.

             Batch freeFrame occurs outside this scope.

        AS6  checkpoint establish
             CMS07.0 pointers: section 8.7, section 6.3, section 6.4

             On store of a grid frame or detected-cut frame, set is_checkpoint.
             Insert into checkpoint pool / ordered index.

             Folded into the relevant AS2 store scope using the same lock.
             Not a separate lock.

        AS7  zone retirement / merge
             CMS07.0 pointers: section 8.7, section 5.5, section 5.6

             Test no-pins-in-range plus decay margin.
             Mark zone inactive or merge.

             Performed under the same lock during AS1 or the prune pass.
             Never split.

    There is no shutdown/clear AS. Shutdown clear is governed by the
    reference-count / teardown obligation, including release of everything and
    warning on any non-zero pin count. It is not an AS6 or AS7 critical-section
    definition.

    V5 firewall:
        VapourSynth frame reference counts are internally atomic only for the
        individual addFrameRef/freeFrame operation. That atomicity gives no
        permission to move, split, shrink, merge, or reorder CMS07 cache lock
        scopes.

        The protected operation is the multi-step cache decision, such as
        find-then-pin, store-then-pin, decide-then-detach, or
        checkpoint-establish-with-store, not merely the refcount bump.

    No cache behaviour is introduced in CMS07-A.2.
*/