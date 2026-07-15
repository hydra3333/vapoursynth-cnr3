# CMS07-FEATURE.cnr2-descriptive-option-parser v3 source-only cumulative patch notes

## Purpose

This v3 patch is a **single cumulative source-code patch** for the approved CNR2/CNR3 descriptive option-parser work.

It is cut against the current GitHub source upload `src(33).zip` and intentionally excludes README/user-documentation changes because the repository already has `README.md` and the earlier v1 patch incorrectly tried to create it as a new file.

## Apply rule

Apply **only this v3 patch** for the source-code change.

Do **not** apply v1 first.
Do **not** apply v2 first.

The README/user-documentation update is deferred to a later documentation-only patch.

## Changed files

```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

## Included work

- Bumps the marker cleanly to:
  `CMS07-FEATURE.cnr2-descriptive-option-parser`
- Adds the intended operational-defaults marker provenance note.
- Adds create-time parsing, validation, and application for the 11 descriptive CNR3 options:
  - `y_threshold`
  - `y_strength`
  - `u_threshold`
  - `u_strength`
  - `v_threshold`
  - `v_strength`
  - `y_curve`
  - `u_curve`
  - `v_curve`
  - `scene_threshold`
  - `scene_chroma`
- Keeps CNR2-equivalent defaults.
- Uses strict `wide` / `narrow` curve validation.
- Keeps threshold zero valid and preserves existing centre-only table-builder behaviour.
- Includes `scene_chroma` in the base patch as plumbing-only.
- Does not change scene math.
- Does not change blend math.
- Extends `response_config` emission with `scene_threshold` and `scene_chroma` from resolved live config values.
- Adds the full parser-site maintainer documentation block in `vapoursynth-Cnr3.cpp`.

## Deferred documentation item

The README/user-facing documentation from v6.1 remains required but is deferred because the earlier v1 patch represented `README.md` as a new file and therefore conflicted with the real repository.

Track as a separate follow-up:

```text
CMS07-DOC.cnr2-descriptive-options-readme
```

That follow-up should update the actual repository documentation file and satisfy the README half of the v6.1 documentation proof gate.

## Apply commands

From repository root:

```bat
git status --short

git apply --check --ignore-whitespace CMS07-FEATURE.cnr2-descriptive-option-parser_v3-source-only-cumulative.patch
git apply --ignore-whitespace CMS07-FEATURE.cnr2-descriptive-option-parser_v3-source-only-cumulative.patch

git diff --check
git diff --stat
git status --short
```

Expected changed tracked source files after apply:

```text
M src/cnr3_build_config.h
M src/vapoursynth-Cnr3.cpp
```

Existing purposeful deletions and untracked local patch/spec files may also still appear in `git status --short`; they are outside this patch.

## Sandbox validation

Validated against a repo-like tree reconstructed from `src(33).zip`:

```text
git apply --ignore-whitespace --check: PASS
git apply --check --whitespace=error: PASS
git apply --ignore-whitespace: PASS
git diff --check: PASS
```

## Gate adjustment note

Because README/user-facing documentation is deferred, the original v6.1 documentation proof gate is not fully satisfied by this source-only patch alone.

For this patch, prove:

- parser-site maintainer comment is present and complete;
- source parser behaviour passes the runtime/proof tests;
- response_config emits resolved live config values;
- no-args output is byte-identical to the committed interim build.

Then run the README/user-documentation follow-up before claiming the full documentation gate complete.
