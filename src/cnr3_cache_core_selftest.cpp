#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

Cnr3Status cnr3_cache_core_selftest_empty_model() noexcept {
    const Cnr3OutputCacheCore cache{};

    if (!cache.empty()) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.slot_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.index_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.checkpoint_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

/*
    CMS07-C.2 cache-core selftest placeholder.

    The only executable selftest in this phase is the empty-model check above.

    Future selftests must verify ownership and lifecycle properties before
    behaviour is trusted. In particular, tests must prove that:
        - pin/unpin operations balance;
        - lookup-reference acquire/release/transfer operations balance;
        - first-in-best-dressed store behaviour preserves the first winner;
        - duplicate-store losers are released or transferred through the
          documented ownership path;
        - prune never evicts pinned, checkpoint-protected, or hot-zone-protected
          frames;
        - bounded recovery planning searches only within its allowed bounds;
        - teardown/clear releases all retained references and reports unsafe
          non-zero pin state.

    Selftest scaffolds must not become production fallback code.
*/
