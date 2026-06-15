#pragma once

/*
    CNR3 isolated cache-core selftest/proof harness.

    CMS07-A.2 skeleton only.

    This module may later provide an isolated cache-core proof driver.

    It must not wire VapourSynth getFrame.

    Intended later use:
        - simulate frame references and request orders;
        - exercise AS scopes;
        - exercise pin/unpin balance;
        - exercise store and duplicate-store handling;
        - exercise prune and teardown validation;
        - support the CMS07-G isolated cache-core proof milestone.

    If any temporary proof code is added here, it must use SCAFFOLD_* naming and
    be clearly bounded for later removal.
*/