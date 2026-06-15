#pragma once

/*
    CNR3 build configuration and compile-time gates.

    CMS07-A.2 skeleton only.

    This header will later hold:
        - ongoing diagnostics gates using DIAG_* names;
        - matching compute/print gate pairs for D-SUM summaries;
        - compile-time checks that a print gate cannot be enabled without its
          matching compute gate;
        - temporary proof scaffold gates using SCAFFOLD_* names.

    Rule:
        DIAG_* gates observe only.
        SCAFFOLD_* gates are temporary proof scaffolds.
        Production correctness must not depend on a disabled diagnostic or
        temporary proof gate.

    No gate logic is introduced in CMS07-A.2. That belongs to CMS07-B.1.
*/