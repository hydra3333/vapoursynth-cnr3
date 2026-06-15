#include "cnr3_cache_core_selftest.h"

/*
    CMS07-B.2.9 cache-core selftest placeholder.

    Executable selftests start only after CMS07-C introduces real cache-core
    state and public testable operations.

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
