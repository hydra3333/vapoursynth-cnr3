# CMS07-DIAG.3c.2 v3 — remove duplicate X/E legend line

## Purpose

Tiny follow-up to the approved DIAG.3c.2 v2 patch after the four-way log showed that each emitted DSUM-PLANTRACE legend printed the Set 4 X/E legend line twice.

This v3 overlay removes exactly one duplicate call to:

```text
[DSUM-PLANTRACE] legend R-code   X = not_reached                    E = error_here
```

The intended result is one X/E legend line per plantrace block.

## Primary patch

Apply this overlay only if the DIAG.3c.2 v2 patch is already applied locally:

```text
CMS07-DIAG.3c.2-v3-remove-duplicate-xe-legend-overlay.patch
```

## Optional full replacement patch

For a clean post-DIAG.3c.1 tree where DIAG.3c.2 has not yet been applied, use the full replacement patch instead:

```text
CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail-v3.patch
```

Do not apply both.

## Changed file

```text
src/cnr3_diagnostics.cpp
```

## Delta summary

```text
- Remove one duplicate cnr3_diag_plantrace_write_line() block for the X/E legend.
- No bail-site edits.
- No FAILED-record semantic changes.
- No gate changes.
- No marker change.
- No cache-core, project-file, pin-list, or production-enum changes.
```

## Sandbox validation

Overlay validation, applied on top of the DIAG.3c.2 v2 patch:

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
remaining X/E legend source occurrences in cnr3_diagnostics.cpp: 1
```

Full replacement validation, applied to clean post-DIAG.3c.1 source:

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
remaining X/E legend source occurrences in cnr3_diagnostics.cpp: 1
```

## Required local proof

After applying the overlay to the current v2-tested tree:

```text
- Rebuild Debug|x64 and Release|x64.
- Re-run DIAG.3c.2 all-on four-way.
- Re-run DIAG.3c.2 macro-off four-way.
- Re-run DIAG.3c.2 restored-all-on four-way if macro-off is toggled.
- Confirm exactly one X/E legend line per emitted block.
- Confirm macro-off still emits no DSUM-PLANTRACE.
- Continue with induced arInitial and recovery-bail proof.
- Capture final git diff -- src\cnr3_build_config.h and git status --short.
```
