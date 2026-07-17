# CNR3 — 8-bit VHS hot-path review report v1

**Purpose:** Investigate the post-3a.2 YUV420P8 hot path, classify the remaining cost as memory/allocation-bound versus arithmetic/vectorisation-bound, and recommend the next optimisation sequence.

**Scope:** No code changes. This report is an assessment to feed the next formal patch scope.

**Target module:** `cnr3_frame_processing.cpp`

**Build/profile context:** x64, `/arch:AVX2`, YUV420P8 production case. Builds on committed AVX2 + Lever 0A + Lever 0B + Lever 3a.1 + Lever 3b.1 + Lever 3a.2.

---

## Executive conclusion

For the dominant YUV420P8 case, the next best move is **not** more arithmetic micro-optimisation first. The 8-bit path now looks primarily **memory/allocation bound**, with one important exception: the blend remains a sizeable mixed arithmetic/control-flow leaf.

Recommended sequence:

```text
1. A-lite diagnostic: restrict/pointer-form 8-bit unpack experiment.
   Purpose: prove whether MSVC can vectorise the 8-bit copy and whether that moves total.

2. C1 targeted memory-elimination: direct native-luma -> downsampled-luma bridge.
   Purpose: remove the full-resolution luma scalar temporary and one full native->scalar copy before downsample.

3. F / 3c scalar production-fast blend path.
   Purpose: inline blend, hoist invariant table/bit-depth checks, apply Tier-2/Tier-3 trust policy.

4. B scratch/pooling only after the above, with fmParallel-safe lifetime proof.
   Purpose: attack ntdll/vector allocation churn.

5. D exact SIMD downsample and E scene-change local accumulator later.
   Both are valid, but unlikely to be the next large total mover.
```

This sequence prioritises **memory traffic elimination first**, because the post-3a.2 8-bit profile tells us branch/vectorisation work alone is no longer moving the total.

---

## Post-3a.2 YUV420P8 leaf table

Using the post-3a.2 runs and the current source shape:

| Leaf / area | Absolute samples, approximate | Loop body / work | Classification | Assessment |
|---|---:|---|---|---|
| `cnr3_copy_native_plane_to_scalar_buffer` | clean runs ~6,900-7,800 combined | 8-bit native byte read -> widen to `int` -> write `resolved_samples`; later publish `resolved_samples` to scalar plane | **Memory / allocation bound** | The 8-bit loop has almost no arithmetic left. Residual cost is byte->int expansion, full-plane int writes, temporary vector, and publish pass. |
| `cnr3_process_chroma_plane_from_downsampled_luma` | ~6,600-6,700 | four scalar reads, signed diffs, response table lookups, int64 blend, output temp | **Mixed arithmetic + memory + call/control overhead** | Largest real-math leaf. Still untyped/buffered and still calls per sample into helper stack. |
| `cnr3_stage_scalar_plane_to_native_bytes` / staging | ~4,700 | scalar `int` output -> native bytes, staging buffer, store validation | **Memory / validation-control bound** | Mostly repack memory traffic and native-store validation. |
| `ntdll` + `std::vector<int>::resize` / assign | ntdll ~8,000; resize ~1,500-2,200 | allocation/free/zero-fill churn | **Allocation / memory bound** | Now proportionally important. Needs lifetime/thread-safety design before patching. |
| `cnr3_downsample_luma_plane_to_chroma_grid` | ~2,600-2,750 after 3b.1 | scalar taps, range checks, exact average | **Small arithmetic/control leaf** | 3b.1 already reduced it locally; total stayed flat. Further exact SIMD likely small. |
| `cnr3_detect_scene_change_from_scalar_planes` | not top leaf in supplied profile | per-frame scalar diff accumulation, optional chroma | **Arithmetic/control, frequency-hot** | Worth later, but only if scene-change is commonly enabled and profile shows it. |

The load-bearing finding remains: Lever 3a.2 vectorised the 16-bit branch, but the production profile is YUV420P8, and the 8-bit conversion path stayed scalar / effectively flat. The known-leaf list in the review brief matches the source shape after 3a.2, except that the latest measured `copy_native_plane_to_scalar_buffer` combined number is closer to ~7k-8k than the earlier ~10k estimate.

---

## Candidate assessment

### A. `__restrict` / pointer-form 8-bit copy

**Classification:** diagnostic memory/arithmetic boundary test.  
**Risk:** low if scoped narrowly.  
**Expected total movement:** probably small to flat, but highly informative.

I recommend doing this first as a **small experiment**, not because I expect a large win, but because it directly answers the open question: can MSVC vectorise the 8-bit unpack if aliasing/indexing is made obvious?

Important nuance: I would not scope this as just sprinkling `__restrict`. The source loop still uses indexed writes into `resolved_samples[(y * width) + x]` and a separate publish loop. A stronger A-lite test would use row pointers / pointer increments and, where MSVC accepts it, `__restrict` on:

```text
const uint8_t* source row
int* resolved row
int* scalar publish row
```

Acceptance should be:

```text
- /Qvec-report:2: does the 8-bit conversion loop vectorise?
- profile: if vectorised but total flat, 8-bit copy is memory-bound.
- if vectorised and total drops, keep and commit.
- if not vectorised or no structural value, discard or leave only if source becomes cleaner.
```

Use A as a direct diagnostic for the memory-bound hypothesis.

---

### B. Allocation pooling / resize-hoist

**Classification:** memory/allocation attacker.  
**Risk:** moderate to high because of lifetime/threading.  
**Expected total movement:** plausible, but not safe enough as the immediate next patch.

The source still allocates many per-activation vectors: eight main chroma-grid scalar vectors in the frame triplet path, plus internal temporaries in unpack/downsample/blend/staging. The profile's `ntdll` and `resize` figures make this attractive.

But this must be designed around final execution semantics. A simple instance-global scratch pool is only safe if one active processing activation per instance is invariant and remains invariant under final fmParallel integration. If not, scratch must be activation-owned or checked out under a proven lifetime discipline.

Recommendation: **defer B until after A/C1/F**, then scope it as a scratch-lifetime proof, not as an ordinary micro-optimisation.

---

### C. Buffer elimination / native->native / pass fusion

**Classification:** strongest memory attacker.  
**Risk:** high in the broad form, but there is a narrower safe subcase.

The broad "stop materialising int buffers" version is too large for the next step. However, source review exposes a highly relevant **C1 subcase**:

```text
C1: direct native-luma -> downsampled-luma bridge
```

Current bridge shape:

```text
native luma plane
  -> full-resolution source_luma_scalar vector
  -> cnr3_downsample_luma_plane_to_chroma_grid
  -> chroma-grid downsampled luma vector
```

For YUV420P8, that means a full-resolution luma byte->int expansion and full int-buffer materialisation before producing the quarter-area chroma-grid guide. This happens twice per output frame: current source luma and previous filtered luma.

A targeted C1 would keep the existing **output** downsampled luma buffers, but avoid the **full-resolution intermediate**:

```text
native luma bytes
  -> exact tap-average directly
  -> chroma-grid int downsampled luma
```

This preserves the downstream data-flow and avoids buffer-free blend feeding. It touches P.9A/P.4A territory, but it is much narrower than full C. It is probably the first candidate with a real chance of moving the 8-bit total after A.

Recommendation: **C1 should be the first serious memory-elimination patch after the cheap A diagnostic.** The broad C idea is correct but too large; C1 is the tractable version.

---

### D. Exact SIMD downsample

**Classification:** arithmetic attacker on a small leaf.  
**Risk:** low-to-medium only if exact; high if using biased average forms.  
**Expected total movement:** small.

GAIS-style two-level `VPAVGB` is rejected because it is biased and recurrence-amplified. Only the widen-add-four-taps-plus-two-shift form is acceptable.

After 3b.1, the downsample function is only ~2.6k samples and did not move total even after a local function reduction. Exact SIMD here may be satisfying, but it is not the next best total-throughput lever.

Recommendation: **defer D.** It becomes more attractive if C1 creates a direct native-downsample helper where the exact SIMD kernel can be used in a clean, isolated 8-bit interior path.

---

### E. Scene-change local accumulator

**Classification:** arithmetic/control-flow hygiene.  
**Risk:** low-to-medium, because summary/early-exit semantics must remain exact.  
**Expected total movement:** small unless scene-change is enabled in the benchmark and profile shows it.

The source currently updates `stats.diff_total` inside the loop and early-exits once the threshold is exceeded. A local accumulator could improve register use and vectoriser hygiene, but the exact reported `diff_total`, `samples_examined`, and early-exit point must not change.

Recommendation: **defer E**, but keep it as a small later cleanup if scene-change is expected to be commonly enabled by users.

---

### F. Blend / 3c typed-native path

**Classification:** mixed arithmetic + memory + control-flow.  
**Risk:** medium-high.  
**Expected total movement:** likely meaningful, but exact SIMD/gather is not the first form to attempt.

The source shows `cnr3_process_chroma_plane_from_downsampled_luma` still calls `cnr3_blend_chroma_sample_from_response_tables` per sample. That helper recomputes or revalidates bit-depth/table geometry/sample bounds, performs response-table lookups, then calls another helper for the fixed-point blend. This is a large call/control/invariant overhead stack on top of real arithmetic.

The GAIS `vpshufb` 256-entry LUT idea is interesting but not a slam dunk; it requires multi-stage LUT construction because `vpshufb` selects within 16-byte lanes.

Recommendation: do **3c scalar-production-fast path** before explicit SIMD:

```text
- production-private blend loop
- inline table lookup and fixed-point blend
- hoist sample_peak, table geometry, shift, shift1, table pointers
- apply validation policy:
    Tier-2 input samples trusted because they came through Tier 1
    Tier-3 response values trusted if tables are builder-proven
- keep final output/staging validation
```

This should remove per-sample helper overhead and redundant validation without taking on SIMD gather complexity yet.

---

## Specific answer: is the 8-bit copy memory-bound?

Assessment: **yes, very likely**, but A should still be run as the cheap confirmation.

Evidence:

```text
- The 8-bit loop now does almost no arithmetic: byte load, widen, int write.
- 3a.2 moved validation out of the hot path but did not reduce the 8-bit total.
- The targeted unpack leaf stayed roughly flat across clean runs.
- ntdll / vector allocation / resize remain large enough to perturb totals.
- The remaining copy cost is mostly full-plane memory traffic and temporary-buffer churn.
```

Working classification:

```text
8-bit unpack:
  memory-bound unless A proves otherwise
```

A is still worth doing because it is low-risk and resolves whether reason 501 is hiding a compiler-form problem. Do not invest in a large 8-bit copy-vectorisation arc if A comes back vectorised-but-flat.

---

## Recommended sequence

### Step 1 — A-lite 8-bit unpack vectorisation diagnostic

Small patch scope:

```text
Target:
  cnr3_copy_native_plane_to_scalar_buffer, 8-bit path only

Change:
  pointer-row form, optional MSVC restrict annotation if acceptable,
  no contract change, no validation-policy change.

Goal:
  see whether the 8-bit conversion loop flips from scalar/reason 501 to vectorized.

Commit rule:
  commit only if it is value-clean and either measurably helps or materially clarifies/cleans the source.
```

### Step 2 — C1 direct native-luma downsample bridge

This is the recommended first **serious** performance lever.

```text
Target:
  cnr3_downsample_native_luma_plane_to_scalar_chroma_grid

Change:
  avoid full-resolution source_luma_scalar materialisation;
  directly read native luma bytes and compute exact chroma-grid downsample.

Preserve:
  Tier-1 source validation
  exact P.4A tap geometry and rounding
  existing downsampled output buffer
  downstream blend/scene-change shape
```

For YUV420P8, this attacks the kind of memory traffic that the profile now suggests is the real bottleneck.

### Step 3 — F / 3c scalar production-fast blend

Do not begin with explicit SIMD. Begin with scalar inlining and validation-policy application.

```text
Target:
  cnr3_process_chroma_plane_from_downsampled_luma

Change:
  production-private loop with inlined response lookup and fixed-point blend;
  hoist invariant geometry/scale checks;
  remove Tier-2/Tier-3 redundant per-sample checks only where provenance is proven.
```

This is likely the next real arithmetic/control-flow win.

### Step 4 — B scratch allocation strategy

Do this as a design/proof step:

```text
- identify all per-frame vectors
- decide activation-local vs instance-pool checkout
- prove fmParallel/fmUnordered lifetime safety
- then patch
```

Given `ntdll` scale, this may matter substantially, but the lifecycle risk is too high for an ad hoc optimisation.

### Step 5 — D and E later

```text
D exact SIMD downsample:
  defer until after C1, because a direct native-downsample helper may be the better kernel host.

E scene-change local accumulator:
  defer until profile shows scene-change enabled and non-trivial.
```

---

## Finding beyond the candidate menu

The biggest extra finding is the **native-luma downsample bridge**. It is not just "downsample" and not just "copy"; it is a full-resolution luma materialisation step feeding a quarter-area guide. That is the most attractive narrow memory-elimination target because it removes a large intermediate without changing the downstream architecture.

Name the next investigation/scope:

```text
CNR3 Lever C1 / 3b-bridge: direct native-luma to scalar chroma-grid downsample
```

not "generic buffer elimination." That keeps it narrow, measurable, and P-series-gated.

---

## Notes for next scope

A good next scope should explicitly distinguish:

```text
A-lite:
  a low-risk diagnostic of the 8-bit unpack loop form and MSVC vectorisation.

C1:
  a real memory-traffic removal step for YUV420P8, targeting the native-luma downsample bridge.
```

Do not combine A-lite and C1 unless the designer deliberately chooses a larger combined experiment. Keeping them separate preserves measurement clarity.
