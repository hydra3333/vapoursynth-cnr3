#pragma once

/*
    CNR3 owned VSFrame reference wrapper.

    CMS07-A.2 skeleton only.

    This header will later define the baseline move-only RAII wrapper for one
    owned VSFrame* reference.

    Ownership rules to preserve when implemented:
        - the wrapper owns exactly one reference or owns nothing;
        - copy is disabled;
        - move transfers ownership;
        - destructor frees the owned reference if still held;
        - transfer/release-to-caller nulls the wrapper;
        - cache lookup and pin semantics do not live in this wrapper.

    The wrapper is a local ownership tool. It is not the cache index, not a
    pin-list entry, and not a recovery-plan object.
*/