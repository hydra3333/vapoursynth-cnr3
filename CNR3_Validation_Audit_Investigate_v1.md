# CNR3 — VALIDATION AUDIT: per-sample range checks — INVESTIGATE & REPORT (no code)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigation with a decision at the end. NO code changes. Verify against source (and CNR2
where relevant), reason about provenance, and REPORT a per-site recommendation. The output feeds the
scope of the vectorisation-unblocking steps (3a.2 / 3b.2 / and the 3c approach).
**Target file:** `cnr3_frame_processing.cpp`.
**Controlling docs:** CMS07.15, FI-10. **Builds on:** committed AVX2 + 0A + 0B + 3a.1 + 3b.1.

---

## 1. Why this audit exists

`/Qvec-report:2` proved that after call-elimination (3a.1, 3b.1), the hot loops STILL report `506`
"not vectorized" — and the remaining blocker is the **per-sample range-check branch**
(`cnr3_value_is_inclusive_range(sample, 0, sample_peak)` with an early `return invalid_argument`). A
data-dependent early-return inside a loop is fundamentally unvectorizable. So to land the vectorisation
win on the big leaves (especially the ~10k-sample unpack), we must either HOIST these checks out of the
hot loop (keep the check, restructure so the loop is branch-free) or REMOVE the ones that are provably
redundant. This audit decides WHICH, per site — because the answer determines whether 3a.2/3b.2 are
"hoist" patches or "remove" patches, and that changes their scope and risk.

## 2. The six range-check sites (verify from source) and their VALUE PROVENANCE

The key question is NOT "is a range check good practice" in the abstract — it is "at THIS site, can the
checked value actually be out of `[0, sample_peak]` given where it came from?" The sites divide by
provenance into trust classes:

```text
CLASS A — value freshly read from a VapourSynth NATIVE SOURCE plane:
  L2013 / L2046  cnr3_load_native_plane_sample / the inlined unpack (copy_native_plane_to_scalar_buffer)
    -> reads bytes from a VS frame plane, widens to int, checks [0, peak].
    QUESTION: is a VS frame plane GUARANTEED to contain only [0, sample_peak] for its declared
    bits_per_sample? (i.e. can a conformant VS source ever hand us a 10-bit plane with a sample > 1023?)
    If guaranteed -> this check is redundant-on-valid-input.

CLASS B — value read from a SCALAR intermediate buffer WE populated (via the Class-A unpack):
  L702-707   scene-change: current/previous luma, current/previous u, current/previous v
  L924-927   downsample taps: tl/tr/bl/br  (read from the unpacked scalar luma)
    -> these values PASSED THROUGH the Class-A unpack already. If the unpack validated them (Class A),
    re-checking here is belt-and-suspenders on data our own code already vouched for.
    QUESTION: are these buffers ONLY ever populated via the validated unpack path, or is there any
    path that writes them unchecked? If only-via-unpack -> redundant given Class A.

CLASS C — value that is COMPUTED, not read:
  L990-993   blend: current_source, previous_filtered, AND y_response, chroma_response (TABLE LOOKUPS)
  L1894-1897 blend: current/previous downsampled luma, current/previous source chroma
    -> the response values are OUTPUTS of the response tables. Their range depends on how the tables
    are CONSTRUCTED, not on input trust.
    QUESTION: are the response tables constructed so their outputs are ALWAYS in [0, sample_peak] by
    design? If yes -> the response-range check is redundant (guaranteed by table construction). The
    source/filtered samples in these sites are Class-B-like (came through unpack).
```

## 3. What we're asking you to determine (per class)

```text
1. CLASS A (unpack, the highest-value target — this is the ~10k-sample leaf):
   Is a conformant VapourSynth source frame GUARANTEED to hold only [0, sample_peak] for its declared
   bit depth? Check VS API docs / semantics. Consider: what about a NON-conformant or malicious source
   (a broken upstream filter emitting 10-bit-flagged data with 16-bit values)? Is CNR3's posture to
   TRUST the declared depth, or to DEFEND against bad upstream? (This is a policy call we will make;
   you provide the facts.)

2. CLASS B (scene-change, downsample taps): confirm from source that these scalar buffers are populated
   EXCLUSIVELY via the Class-A validated unpack (no other writer). If so, the Class-A check already
   guarantees their range and the Class-B checks are redundant.

3. CLASS C (blend response outputs): from cnr3_response_tables construction (cnr3_response_tables.cpp),
   are the table outputs bounded to [0, sample_peak] BY CONSTRUCTION? If yes, the response-range check
   is provably redundant. (The blend's average/clamp arithmetic — report whether the final blended
   output is clamped independently of these input checks.)

4. CNR2 COMPARISON: did the predecessor CNR2 perform per-sample range validation in its equivalent
   unpack / downsample / blend, or did it TRUST the input and only clamp on output? This is EVIDENCE
   (not decisive): if CNR2 ran in production for years trusting input, that supports Class-A/B being
   safe to trust. Report what CNR2 actually did, factually.

5. For EACH site, recommend one of:
     KEEP    (load-bearing; a real out-of-range value is possible here) -> 3x.2 must HOIST not remove
     HOIST   (needed but movable; validate in a separate vectorizable pass, keep the guarantee)
     REMOVE  (provably redundant given an upstream guarantee; safe to delete from the hot loop)
   with the specific reason and the guarantee it relies on.
```

## 4. The decision this feeds (so you understand the stakes)

```text
If a site is REMOVE  -> the corresponding 3x.2 deletes the branch; the loop should then vectorise cleanly.
                        Biggest win, but ONLY safe if the upstream guarantee is real and stated.
If a site is HOIST   -> 3x.2 validates the whole row/plane in a separate (vectorizable min/max or
                        compare-accumulate) pass, then converts branch-free. Preserves the check,
                        unblocks vectorisation, more code.
If a site is KEEP    -> that leaf may not fully vectorise; we accept it (copy-win only) or reach for
                        Path B (explicit SIMD with masked handling) later.
```

We LEAN toward: Class A is likely TRUST-able for conformant VS input (so REMOVE or a single cheap
plane-level guard rather than per-sample), Class B is likely REMOVE (redundant given A), Class C
response-range is likely REMOVE if tables are bounded by construction. But this is a SAFETY-relevant
call and we will not act on a lean — we need your source-grounded facts on each guarantee first,
especially the VS-source-conformance question (Class A) and the table-construction bound (Class C).

## 5. Constraints on the audit

```text
- NO code changes in this step — investigate and report only.
- Do NOT weaken any invariant on the STRENGTH of a lean; we want the guarantee stated and verified.
- Value-identity on VALID input is non-negotiable regardless of outcome — removing a check must never
  change output for any input the filter is actually given.
- The output-side clamp (final blended sample clamped to [0, peak] at store) is SEPARATE and STAYS —
  this audit is about the per-sample INPUT/intermediate checks, not the final output clamp.
- Flag anything where you are unsure of the guarantee — "unsure" means KEEP/HOIST, not REMOVE.
```

## 6. Report format

Per-site table: site (line) / class / value provenance / can-it-be-out-of-range? / guarantee relied on /
recommendation (KEEP/HOIST/REMOVE) / confidence. Plus the CNR2 factual finding and the VS-source-
conformance finding as the two load-bearing external facts. We then make the policy call (trust vs
defend) and scope 3a.2 (and 3b.2 / 3c approach) accordingly.
