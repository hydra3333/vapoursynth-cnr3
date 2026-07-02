# CNR3 Tier-2 Chroma-Unpack Fusion — Investigation Report v1

## Executive verdict

**Verdict: PARTIAL.**

The narrow technical question "can the recursive chroma blend read current and previous chroma directly from native byte planes?" resolves **cleanly**: yes. Q1-Q5 are all technically clean. The likely blocker named in the scope, `previous_filtered_chroma` native availability, is **not** a blocker: `views.previous_filtered_u` and `views.previous_filtered_v` are constructed as native read views and remain in scope at the blend call site.

However, the broader objective "eliminate the four pre-unpacked scalar chroma buffers" is **not safe as a blend-only patch**, because those four scalar chroma planes have non-blend consumers in the current source:

1. scene-change detection consumes `current_u`, `previous_u`, `current_v`, and `previous_v`;
2. the scene-reset branch copies `current_u_storage` and `current_v_storage` to the output scalar planes and publishes summaries from the scalar current chroma views.

Therefore, a native-read blend path alone can safely avoid the four chroma unpack buffers only for a no-scene-detection fast path, or for a broader patch that also provides native scene-change detection and a native/current-source reset path. A blend-only patch that simply removes the four scalar chroma buffers unconditionally would break the scene path.

## Source basis

This report is based on the attached `cnr3_frame_processing.cpp` and the supplied investigation scope. The source instance inspected contains the current post-F/3c blend shape and the same frame-triplet chroma unpack/plumbing relevant to this question. The later Staging lever changes scalar-to-native staging, not the native view construction, four chroma unpack call sites, scene detector inputs, or recursive blend inputs.

## Q1 — Multiple consumers / multiple reads

**Finding: clean for blend fusion.**

The current F/3c blend already reads four scalar row values into locals:

- `current_luma`
- `previous_luma`
- `current_chroma`
- `previous_chroma`

It then uses `current_chroma` and `previous_chroma` for both the signed chroma difference and the fixed-point blend terms. A native-fused version can do the same thing: load each native chroma sample once per output sample into local `int` variables, then reuse those locals for both the response-table diff and the blend formula.

The arithmetic does not need to change. The fused form must retain:

```text
chroma_signed_diff = current_chroma - previous_chroma

weight = int64(y_response) * int64(chroma_response)

output = (
    weight * previous_chroma +
    (shift - weight) * current_chroma +
    shift1
) >> shift2
```

**Assessment:** clean. Native direct read should be load-once / widen-once / local-reuse, not repeated native memory reads.

## Q2 — Validation tier interaction

**Finding: clean, but the Tier-1 gate moves into the native blend path.**

Today the Tier-1 native chroma validation is owned by `cnr3_copy_native_plane_to_scalar_buffer`. It validates native plane shape and dimensions, computes `sample_peak`, treats 8-bit as type-bounded, and performs a 16-bit native pre-scan that OR-accumulates out-of-range samples before publishing scalar output.

If the recursive blend consumes chroma directly from native bytes, the fused blend becomes the first native chroma reader for that path. Therefore, the native blend path must include a reject-before-publish pre-pass:

- validate native view shape and dimensions for current and previous chroma;
- for 8-bit, rely on `uint8_t` spanning `[0,255]`;
- for 9..16-bit, scan active native samples using unaligned-safe two-byte `memcpy`, OR-accumulate any `sample > sample_peak`, and return `invalid_argument` before writing output if any out-of-range sample is seen;
- keep the existing scalar luma pre-pass for `current_downsampled_luma` and `previous_downsampled_luma`, because those remain scalar inputs.

This is coherent with the existing F/3c pre-pass design: the pre-pass already exists specifically to preserve no-partial-output before the fused production loop writes to the output plane. The native version would preserve the same proof shape, but with native chroma validation replacing scalar chroma validation.

**Assessment:** clean if explicitly implemented as a native Tier-1 pre-pass. Unsafe if the patch simply drops the existing scalar pre-pass without replacing the native chroma gate.

## Q3 — Downsampled-luma inputs are not chroma

**Finding: clean.**

The current and previous downsampled luma planes are produced separately by `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` and stored as scalar `int` planes. They are still needed by the blend for luma response lookup and are not produced by the four chroma unpack calls.

A chroma-unpack fusion should leave them untouched. Only the chroma roles are candidates:

- `current_source_u`
- `current_source_v`
- `previous_filtered_u`
- `previous_filtered_v`

**Assessment:** clean. Luma remains scalar; chroma may become native.

## Q4 — Previous filtered chroma provenance

**Finding: clean; not a blocker.**

The current call site constructs native read views for both current-source and previous-filtered frames before the chroma unpack calls. In particular, the triplet builder creates:

- `views.current_source_u`
- `views.current_source_v`
- `views.previous_filtered_u`
- `views.previous_filtered_v`

using `cnr3_make_vapoursynth_read_plane_byte_view`. These views are stored in `views`, which remains in scope throughout `cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl`, including at the current recursive blend call sites.

Therefore, `previous_filtered_chroma` is cleanly available as a native byte plane at blend time. It is not only available as a scalar buffer.

**Assessment:** clean. Full native blend fusion for both current and previous chroma roles is possible from an availability/provenance standpoint.

## Q5 — Stride / subsampling / native offset availability

**Finding: clean.**

The native read-plane view records:

- data pointer;
- width;
- height;
- stride in bytes;
- bit depth.

The triplet compatibility function verifies that current-source, previous-filtered, and destination chroma planes match in width, height, and bit depth, and that chroma dimensions are compatible with luma dimensions and the subsampling factors.

Therefore, each native chroma plane has enough information at blend time for:

```text
row = base + y * stride_bytes
sample address = row + x * storage_bytes
```

The correct 16-bit access discipline remains the same as in prior levers: unaligned-safe `memcpy`, not `uint16_t*` row casts.

**Assessment:** clean.

## Additional load-bearing finding — scene path prevents full blend-only buffer elimination

**Finding: blend-only full elimination is not safe for all runtime modes.**

The four chroma scalar buffers are not used only by the recursive blend. They are also consumed by scene-change logic:

1. `cnr3_detect_scene_change_from_scalar_planes` takes `current_u`, `previous_u`, `current_v`, and `previous_v`;
2. if a scene reset fires, the code assigns `output_u_storage = current_u_storage` and `output_v_storage = current_v_storage`, then publishes output summaries from the scalar current chroma views.

Consequently, an unconditional patch that deletes the four chroma scalar buffers and only teaches the blend to read native chroma would leave the scene detector and reset branch without inputs.

There are three feasible paths:

### Path A — no-scene fast path only

When `scene_config == nullptr`, skip the four chroma unpack buffers and call a native-chroma blend function for U and V. When `scene_config != nullptr`, keep the existing scalar unpack/scene/blend/reset path.

This is the smallest safe patch and should reclaim the chroma-unpack leaf only when scene detection is disabled. It is a **conditional partial** win.

### Path B — full native chroma path including scene support

Broaden the fusion scope to include:

- native-chroma scene-change detection, with the same Tier-1 native validation policy;
- native/current-source reset handling, such as direct staging/passthrough from current-source U/V native planes on scene reset, or direct scalar output construction without pre-unpacking all four chroma planes;
- native recursive blend.

This could eliminate all four chroma pre-unpack buffers in all modes, but it is no longer a blend-only patch. It touches scene-change and reset semantics and requires a larger proof gate: P.6A/P.8A/P.11B/P.11C, not just the recursive blend path.

### Path C — do not pursue

If the team does not want a conditional no-scene fast path and does not want a broader native scene/reset proof, stop the fusion work here. The current optimisation arc is already around -79%, and the remaining leaf is comparatively small.

## Eliminable buffer count

Under **Path A** (`scene_config == nullptr` fast path):

- eliminable chroma unpack calls: all four (`current_u`, `current_v`, `previous_u`, `previous_v`);
- eliminable scalar chroma input buffers for blend: all four;
- output scalar buffers still remain unless a further direct-native-output/staging fusion is scoped separately.

Under **Path B** (native scene/reset included):

- eliminable chroma unpack calls: all four in all modes;
- eliminable scalar chroma input buffers: all four in all modes;
- output scalar buffers may still remain depending on whether the patch also writes native/staged output directly.

Under **Path C**:

- eliminable buffer count: zero.

## Recommended verdict

**PARTIAL.**

Q1-Q5 are clean for native chroma consumption by the blend. The expected Q4 blocker is not present. The blocker is instead the source-discovered scene-change/reset consumers of the same scalar chroma planes. Therefore, a blend-private native read path is safe only as either:

1. a no-scene-detection fast path; or
2. part of a broader native chroma scene/reset/blend scope.

## Recommended next step

Do not scope a simple “replace blend inputs with native planes and remove the four chroma buffers” patch. That would be incomplete.

The cleanest next scope, if the coordinator wants to pursue this, is:

```text
Lever Tier2 Chroma-Unpack Fusion A:
  conditional no-scene native recursive blend fast path

When scene_config == nullptr:
  - allocate only downsampled luma and output scalar planes;
  - skip current_u/current_v/previous_u/previous_v scalar allocations and unpack;
  - native-validate current_source_u/v and previous_filtered_u/v before publish;
  - call a native-chroma blend function for U and V;
  - stage output U/V normally.

When scene_config != nullptr:
  - keep the existing scalar path unchanged.
```

That patch would be narrow, value-preserving, and easy to gate against P.8A/P.11B while avoiding the scene-path proof expansion.

If the project wants full all-mode elimination, scope a larger follow-on after that:

```text
Lever Tier2 Chroma-Unpack Fusion B:
  native scene-change + native reset path + native recursive blend
```

That is feasible but materially larger and should not be bundled into the first fusion patch.
