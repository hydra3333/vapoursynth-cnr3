/*
    CNR3 - VapourSynth API4 temporal chroma stabiliser, based on the
    venerable CNR2/vscnr2.

    CNR3 is a redevelopment inspired by the CNR2/vscnr2 recursive temporal
    chroma-stabilisation model, using VapourSynth API4 only.

    Recursive processing and VapourSynth scheduling:
        The CNR2/vscnr2 algorithm is inherently temporal and recursive, which
        requires in-order serial frame processing in its original execution
        model.

        Processing SOURCE frame N into OUTPUT frame N requires access to the
        already-filtered OUTPUT arising from previously processed SOURCE frame
        N - 1:

            output[N] depends on both source[N] and output[N - 1]

        That makes the CNR2/vscnr2 algorithm naturally serial.

        Older VapourSynth-era recursive filters could sometimes rely on
        compatibility-style scheduling parameters and assumptions. In
        particular, fmFrameState meant only one thread would call a filter's
        getFrame function at a time and only one frame would be processed at a
        time.

        However, VapourSynth API4 documentation says fmFrameState is for
        compatibility only and MUST NOT BE USED IN NEW FILTERS. VapourSynth is
        also moving away from API3: current Windows binaries no longer
        distribute the API R3 headers, and general API R3 plugin support is
        only retained for now.

        CNR3 therefore HAS TO CHANGE from CNR2/vscnr2-era fmFrameState and API3
        assumptions in order to survive as a maintainable modern VapourSynth
        filter.

        CNR3 will initially target fmUnordered integration, then move through
        fmParallelRequests if approved by the phase plan, and finally move to
        fmParallel.

    Project scope:
        - VapourSynth API4 only.
        - Integer planar YUV only.
        - Primary target material: analogue video captures with temporal chroma
          instability, such as VHS/VHS-C and related restoration sources.
        - Correct recursive chroma-stabilisation behaviour before parallel
          performance work.

    Load-bearing recursive fact:
        Modern VapourSynth scheduling can and does request frames out of
        display order, so CNR3 must not rely on display-order calls, a single
        previous-output variable, or strict serial predecessor state.

        CNR3 is not a stateless image filter.

        To compute OUTPUT frame N, the filter needs:
            - SOURCE[N]
            - already-filtered OUTPUT[N - 1]

        The predecessor is the previous filtered OUTPUT, not SOURCE[N - 1].

            output[N] depends on source[N] and output[N - 1]

        This recursive dependency is the central reason CNR3 needs its own
        cache/recovery architecture.

    CMS07.0 restart architecture:
        CMS07.0 holds the new cache design and proof path.

        The first implementation milestone is the cache-manager core in
        isolation, before VapourSynth getFrame wiring and before pixel-layer
        salvage.

        The CMS07 cache manager is a correctness subsystem. It owns output
        frame reference slots, ordered index state, consumer-held pins,
        per-invocation pin-lists, checkpoint flags, hot zones, prune policy,
        recovery planning, validation, and cache diagnostics.

        It must prove:
            - pin/unpin balance is zero;
            - lookup-reference accounting balances;
            - no VSFrame reference leaks;
            - no double-free;
            - eviction never selects a pinned, checkpoint, or in-zone slot;
            - shutdown clear releases all cached references, with a warning on
              any non-zero pin count.

    File role:
        This translation unit is the VapourSynth integration layer.

        It should remain thin. Its long-term responsibilities are plugin
        registration, parameter parsing, instance creation/destruction,
        VapourSynth source-request/retrieve lifecycle, frameData allocation and
        cleanup, error mapping, and calls into the cache and pixel-processing
        layers after those layers are proven.

        It must not contain cache algorithms, recovery algorithms, prune policy,
        pixel loops, response-table construction, or memory-diagnostic internals.

        During the CMS07-A project-skeleton phase, this file is intentionally a
        skeleton only. It must not wire getFrame, implement cache behaviour, or
        copy old salvage code.

    Filter-mode posture:
        The final operational target is fmParallel.

        Interim development may pass through safer or narrower stages, but code
        and design must not introduce assumptions that block eventual safe
        fmParallel operation unless the exception is explicit, temporary,
        justified, and recorded.

        Do not reintroduce fmFrameState compatibility assumptions.
        Do not reintroduce old strict-streaming output authority.
        Do not rely on call order as proof that OUTPUT[N - 1] exists.

    Pixel-layer boundary:
        Pixel/frame processing is a separate layer.

        When the pixel layer is later salvaged, it receives SOURCE[N] and an
        explicit previous OUTPUT frame supplied by the cache/recovery layer.
        The pixel layer must not find, cache, pin, recover, prune, schedule, or
        substitute predecessor frames.

        CNR2/vscnr2 may be used as pixel-maths guidance only. CNR3 must not
        adopt CNR2's serialized recovery/predecessor approximation. In
        particular, CNR3 must not substitute SOURCE[N - 1] for the required
        previous filtered OUTPUT[N - 1].

    Diagnostics and output:
        CNR3 must never write diagnostics, debug messages, status messages, or
        summaries to stdout. In common VapourSynth pipelines, stdout may carry
        frame data.

        Diagnostic and debug text must go to stderr.

        VapourSynth creation errors must use mapSetError().
        VapourSynth frame-processing errors must use setFilterError().

        Ongoing diagnostics use DIAG_* gates and observe only.
        Temporary proof scaffolds use SCAFFOLD_* gates, are clearly bounded,
        and must not become required for production correctness.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_common.h"

#include "VapourSynth4.h"
#include "VSHelper4.h"

/*
    CMS07-A.2 skeleton only.

    VapourSynth plugin registration and getFrame wiring are intentionally not
    introduced in this phase.
*/
