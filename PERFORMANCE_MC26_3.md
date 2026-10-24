# Sanicodes 6 performance change

Version 5 fixed raw-depth sampling with a fresh private sampler on each affected
draw. Version 6 retains a private sampler per texture unit and tracked context.
It still reads every effective setting at draw time. An identical state reuses
the sampler; changed wrap, LOD, filters or border settings update the copy.
No application texture/sampler parameters or graphics-quality settings change.
Nested borrowers and untracked contexts use temporary copies. Context retirement
queues deletion for the correct share group instead of issuing GL calls with an
unrelated context current.

## Host measurement

Baseline: `c7b21f6` (Sanicodes 5) `texture.cpp`. Candidate: Sanicodes 6.
Both use the same current harness, headers and other production translation
units, compiled with GCC `-O2` on the development machine. This isolates the
texture-path change; it is not a benchmark of the Android APKs or Mali GPU.
The software backends are ANGLE `5fa2e61da947` / Vulkan SwiftShader and Mesa
GLES 3.2 / llvmpipe. A D32F 4x4 image is sampled with the same invalid desktop
filter pairing, corrected by the production draw/texture wrappers.

Each process measures five batches of 10,000 guard-only cycles and five batches
of 1,000 4x4 draws. Draw timing includes a GPU finish after each batch. The table
reports the median of three process measurements per version/backend, with
baseline/candidate order alternated. Counters are enabled identically for both.
All times are microseconds per operation; lower is better.

| Backend | Measurement | Version 5 | Version 6 | Time reduction |
| --- | --- | ---: | ---: | ---: |
| ANGLE / Vulkan SwiftShader | Guard CPU time | 1.592 | 0.831 | 47.8% |
| ANGLE / Vulkan SwiftShader | 4x4 draw + batch finish | 15.911 | 14.378 | 9.6% |
| Mesa / llvmpipe | Guard CPU time | 0.823 | 0.401 | 51.3% |
| Mesa / llvmpipe | 4x4 draw + batch finish | 4.002 | 3.167 | 20.9% |

In each 50,000-cycle warmed guard measurement, version 5 generated and deleted
50,000 samplers and made 450,000 sampler parameter writes under ANGLE (500,000
under Mesa, including border colour). Version 6 generated/deleted zero and made
zero sampler parameter writes. State reads and original-binding restoration
remain enabled. This removes repeated allocation/configuration rather than
assuming the game's sampler state never changes.

Full regression runs pass with 78 ANGLE checks and 79 Mesa checks, no GL errors.
They include state changes, context isolation/retirement, nested borrowers,
untracked fallback, depth/colour readback, shadow comparisons and restoration.
The existing old-ANGLE sampler-array remapping limitation is reported separately;
it also reproduces with legal filters in the direct-driver control. Mesa verifies
the updated composite numerically. The optional benchmark-only mode exits before
the full regression suite and must not be presented as its validation.

## Reproduce

From the repository root, extract the baseline source:

```sh
git show c7b21f6:MobileGlues-cpp/gl/texture.cpp > /tmp/mg-v5-texture.cpp
```

Compile `tests/depth_sampling_gles_test.cpp` with the sources and flags in
`tests/run-depth-gles.sh`. For the baseline replace `gl/texture.cpp` with the
extracted file and add `-Igl` so its relative includes resolve. Link the candidate
with the current `gl/texture.cpp`. Run both with the same environment:

```sh
MG_DEPTH_BENCH_ONLY=1 MG_DEPTH_BENCH_ITERATIONS=10000 \
MG_DEPTH_BENCH_DRAW_ITERATIONS=1000 \
MG_GLES_TEST_ANGLE_DIR="$ANDROID_SDK_ROOT/emulator/lib64/gles_angle" \
VK_ICD_FILENAMES=/path/to/vk_swiftshader_icd.json /path/to/benchmark
```

Without `MG_DEPTH_BENCH_ONLY` the candidate also runs the full regression suite.
For Mesa, omit the ANGLE directory, set `LIBGL_ALWAYS_SOFTWARE=1` and select the
Mesa EGL vendor if the host has several implementations.

## Phone acceptance

The user reports about 48 FPS with version 5, Fabulous and 100% launcher
resolution scale. Render distance was not supplied. Compare version 6 in the
same world/view with the same distance, resolution, graphics mode and device
conditions after chunks settle. Verify clouds/water behind geometry and GUI
textures as well as FPS. `MG/latest.log` identifies build `2.0.0-sanicodes.6` and
prints `Raw depth sampler cache: reusing unchanged state` on the first cache hit.

The host results do not imply a corresponding Minecraft FPS increase. Full
resolution Fabulous may be limited by GPU pixel/shader work that this change
does not reduce. Device performance confirmation remains pending.
