#include "cnr3_cache_core.h"

#include <climits>

Cnr3CacheSlotId Cnr3CacheSlotIdSource::allocate() noexcept {
    const int value_to_return = (next_value_ > 0) ? next_value_ : 1;

    next_value_ = (value_to_return < INT_MAX) ? (value_to_return + 1) : 1;

    return Cnr3CacheSlotId{ value_to_return };
}

int Cnr3CacheSlotIdSource::next_value_for_diagnostics() const noexcept {
    return next_value_;
}

bool cnr3_cache_slot_has_frame(
    const Cnr3CacheSlot& slot
) noexcept {
    return slot.frame.has_frame();
}

bool cnr3_cache_slot_is_indexable(
    const Cnr3CacheSlot& slot
) noexcept {
    return cnr3_cache_slot_id_is_valid(slot.slot_id) &&
        cnr3_frame_number_is_valid(slot.frame_number) &&
        slot.frame.has_frame();
}

bool Cnr3OutputCacheCore::empty() const noexcept {
    return slots_.empty() &&
        frame_index_.empty() &&
        checkpoint_slot_positions_.empty();
}

std::size_t Cnr3OutputCacheCore::slot_count() const noexcept {
    return slots_.size();
}

std::size_t Cnr3OutputCacheCore::index_count() const noexcept {
    return frame_index_.size();
}

std::size_t Cnr3OutputCacheCore::checkpoint_count() const noexcept {
    return checkpoint_slot_positions_.size();
}

/*
    CMS07-C.1 cache-core data model placeholder.

    Mutating cache-core functions start in later CMS07-C subphases.

    The comments below are intentionally placed in this source file because AS
    implementation will live here initially. Keeping AS functions close to the
    state they protect reduces the risk of hiding or weakening the lock-scope
    contract.

    V5 firewall:
        VapourSynth's internal frame-reference count atomicity protects only a
        single addFrameRef/freeFrame operation. It does not make any CMS07 cache
        critical section optional, smaller, splittable, reorderable, or movable.

        The protected operation is the whole cache-state decision, not merely
        the frame-reference bump. A future find-and-pin operation, for example,
        is a single cache-lock operation because the find and the add-ref/pin
        record must not be separated by a prune window.

    Atomic-scope rule:
        CMS07 AS1-AS7 are designer-owned, mandatory, indivisible cache-lock
        scopes. Do not shrink, split, merge, reorder, or reinterpret them
        without explicit CMS update / user approval.

    Slow work rule:
        Source requests, source retrieval, pixel compute, batch freeFrame after
        detach, diagnostic formatting/printing, and heap-heavy summary
        construction must happen outside CMS07 cache atomic/locked scopes.
*/

/*
    CMS07 AS1 - arInitial plan-and-pin.

    What must happen inside the lock:
        - Phase-1 descending bounded search [max(0, N - B), N].
        - Pin the start point and every present reused frame.
        - Catalogue output holes.
        - Append every pin to the frameData pin-list.
        - Update/slide hot zone(s) for N.

    What must not happen inside the lock:
        - VapourSynth source requests.
        - VapourSynth source retrieval.
        - Pixel compute.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition, indivisible.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS2 - arAllFramesReady per-hole store-and-pin.

    What must happen inside the lock:
        - First-in-best-dressed check.
        - Store computed output or adopt the existing winner.
        - Pin the stored/adopted output.
        - Append the pin to the frameData pin-list.
        - If the stored frame is a grid checkpoint or detected-cut checkpoint,
          establish checkpoint state in this same store scope.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Batch freeFrame.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition per hole. Compute happens outside before this
        scope. AS6 checkpoint establishment is folded into this store scope, not
        implemented as a separate lock.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS3 - reused-frame pin during ascending fill.

    What must happen inside the lock:
        - Confirm output[K] is present.
        - Add/retain the frame reference under the cache lock.
        - Append the pin to the frameData pin-list.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.

    Lock rule:
        Find-and-pin is one indivisible unit. Do not separate the lookup from
        the add-ref/pin record.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS4 - final unpin.

    What must happen inside the lock:
        - For every entry on the frameData pin-list, unpin/decrement exactly
          once.
        - Discharge the pin-list entries so cleanup cannot unpin them again.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition for the whole pin-list at the end of
        arAllFramesReady.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS5 - bounded prune decide-and-detach.

    What must happen inside the lock:
        - Evaluate the composite eviction predicate.
        - Select up to K victims, greatest-distance-first.
        - Detach each victim slot from the index using the central remove
          helper.
        - Collect freed VSFrame references into a local list for later release.

    What must not happen inside the lock:
        - Batch freeFrame.
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition for decide-and-detach. The batch freeFrame work is
        outside this scope.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS6 - checkpoint establish.

    What must happen inside the lock:
        - On store of a grid frame or detected-cut frame, set is_checkpoint.
        - Insert the frame into the checkpoint pool / ordered checkpoint index.

    Lock rule:
        AS6 is not a separate lock. It is folded into the relevant AS2
        store-and-pin scope using the same lock.

    Do not split AS6 out of AS2 without explicit CMS update / user approval.
*/

/*
    CMS07 AS7 - zone retirement / merge.

    What must happen inside the lock:
        - Test no-pins-in-range plus decay margin.
        - Mark a zone inactive/retired or merge zones as required.

    What must not happen inside the lock:
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        Performed under the same lock during AS1 or the prune pass. Never split
        into an ad-hoc separate lock.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/
