#pragma once

/*
    CNR3 pixel/frame processing.

    CMS07-A.2 skeleton only.

    This module will later perform pixel-layer work:
        - luma copy;
        - downsampled-luma buffer construction;
        - scene-change detection;
        - recursive chroma blend using explicit previous OUTPUT.

    It must not:
        - find predecessor frames;
        - cache frames;
        - pin frames;
        - recover frames;
        - prune frames;
        - schedule source requests;
        - substitute SOURCE[N - 1] for OUTPUT[N - 1].

    The pixel layer receives SOURCE[N] and an explicit previous OUTPUT frame
    supplied by the cache/recovery layer.

    CNR2/vscnr2 may be used as pixel-maths guidance only. CNR3 must not adopt
    CNR2's serialized recovery/predecessor approximation.
*/