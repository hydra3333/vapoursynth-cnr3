#pragma once

#include "VapourSynth4.h"

#include "cnr3_common.h"

/*
    CNR3 owned VapourSynth frame-reference boundary.

    CMS07-B.2.3 introduces the small RAII wrapper that owns one VapourSynth
    frame reference and releases it with VSAPI::freeFrame() unless ownership is
    explicitly transferred out.

    This module is intentionally narrow:
        - it may know about VSFrame and VSAPI;
        - it may release an owned VSFrame reference;
        - it must not request frames;
        - it must not add frame references;
        - it must not inspect frame pixels;
        - it must not own cache state;
        - it must not print diagnostics.

    Cache slots, consumer pins, checkpoints, hot zones, recovery, and prune
    policy remain in cnr3_cache_core.*.

    Important ownership rule:
        A non-null VSFrame pointer may be adopted only with a non-null VSAPI
        pointer. The VSAPI pointer must remain valid for the lifetime of the
        adopted frame reference.
*/

class Cnr3OwnedFrameRef {
public:
    Cnr3OwnedFrameRef() noexcept = default;

    ~Cnr3OwnedFrameRef() noexcept;

    Cnr3OwnedFrameRef(const Cnr3OwnedFrameRef&) = delete;
    Cnr3OwnedFrameRef& operator=(const Cnr3OwnedFrameRef&) = delete;

    Cnr3OwnedFrameRef(Cnr3OwnedFrameRef&& other) noexcept;
    Cnr3OwnedFrameRef& operator=(Cnr3OwnedFrameRef&& other) noexcept;

    /*
        Return the currently held frame pointer for observation only.

        The caller does not receive ownership from get(). The owned reference
        remains owned by this Cnr3OwnedFrameRef.
    */
    [[nodiscard]] const VSFrame* get() const noexcept;

    /*
        True when this wrapper currently owns a frame reference.
    */
    [[nodiscard]] bool has_frame() const noexcept;

    /*
        Adopt a VapourSynth frame reference.

        The caller gives ownership of exactly one frame reference to this object.
        On success, this object is responsible for releasing it unless the frame
        is later transferred out.

        Passing nullptr as frame clears the current owned reference and succeeds.
        Passing a non-null frame with a null VSAPI is rejected and leaves the
        current owned reference unchanged.
    */
    [[nodiscard]] Cnr3Status reset_to_owned_frame(
        const VSFrame* frame,
        const VSAPI* vsapi
    ) noexcept;

    /*
        Release any currently owned frame reference using VSAPI::freeFrame().

        After reset(), this object is empty.
    */
    void reset() noexcept;

    /*
        Transfer ownership of the currently held frame reference out.

        After transfer_to_caller(), this object is empty. The caller owns the
        returned frame reference and must either return it to VapourSynth or
        release it exactly once through the appropriate ownership path.

        nullptr means this object was empty.
    */
    [[nodiscard]] const VSFrame* transfer_to_caller() noexcept;

private:
    const VSFrame* frame_ = nullptr;
    const VSAPI* vsapi_ = nullptr;
};
