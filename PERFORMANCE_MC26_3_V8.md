# Version 8: fewer fresh-sampler setup writes

The v5 renderer remains the baseline. This change skips a sampler parameter
write only when a fresh sampler already has the requested value. Source state
is queried on each draw. Required non-default values are copied and each
sampler is deleted when the draw returns. No sampler survives across draws and
no context lifecycle/cache logic from withdrawn v6 is present.

Initial values are specified by the [Khronos sampler reference](https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glSamplerParameter.xhtml)
and the [OpenGL ES 3.2 specification](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf).
The border test compares bytes so signed zero is preserved. Correct depth
minification/magnification filters continue to be written explicitly.

## Validation

The production texture/draw wrappers link to real GLES drivers. Tests inspect
private sampler state inside the draw forward and compare it with the effective
application state. Default/non-default/default transitions, non-default and
signed-zero border data, fresh sampler deletion, raw depth/colour readback,
texture-only sampling, shared colour samplers and shadow comparison are covered.
ANGLE passes 97 checks and Mesa passes 104, with no GL errors. The existing old
ANGLE sampler-array remapping limitation also reproduces in the direct-driver
control; Mesa verifies the changed composite numerically.

## Instrumented host microbenchmark

The baseline uses c7b21f6 texture.cpp. Both builds use the same new harness,
headers and other production sources with GCC -O2 on the development machine.
The fixture uses Minecraft-style nearest min/mag filtering, clamp-to-edge wrap,
default comparison function and min LOD, and max LOD zero.
Three processes per version/backend were run in alternating order. Each process
reports the median of five 10,000-cycle guard batches and five 1,000-draw batches.
Draws render 4x4 pixels and finish the GPU per batch. The table reports medians
across processes in microseconds per operation. It does not measure Mali or
Minecraft frame rate.

| Backend | Measurement | V5 baseline | V8 candidate | Time reduction |
| --- | --- | ---: | ---: | ---: |
| ANGLE / Vulkan SwiftShader | Guard CPU | 1.500 | 1.309 | 12.7% |
| ANGLE / Vulkan SwiftShader | 4x4 draw + batch finish | 15.500 | 14.994 | 3.3% |
| Mesa / llvmpipe | Guard CPU | 0.801 | 0.704 | 12.1% |
| Mesa / llvmpipe | 4x4 draw + batch finish | 3.928 | 3.777 | 3.8% |

Over 50,000 guard cycles, both versions generate and delete 50,000 samplers.
The baseline makes 450,000 setup writes under ANGLE and 500,000 under Mesa;
the candidate makes 250,000 under either backend. Object lifetime is unchanged.
The improvement is modest and could be invisible in whole-game FPS.

## Reproduce

Run `tests/run-depth-gles.sh` with the EGL/driver environment described in
TESTING_MC26_3.md. Optional `MG_DEPTH_BENCH_ITERATIONS=10000` measures the guard;
`MG_DEPTH_BENCH_DRAW_ITERATIONS=1000` adds timed draws. `MG_DEPTH_BENCH_ONLY=1`
exits after timing and initial depth readback, without certifying the full suite.
For an identical-harness baseline, extract c7b21f6's texture.cpp with git show,
substitute it for gl/texture.cpp and add -Igl when compiling so relative includes
resolve. Timed tests still require one allocation/deletion per affected draw.

## Device status

The user reports around 48 FPS in-world on v7, at 100% resolution. VSync-off and
a quick 75% resolution comparison reportedly did not improve it; the main menu
runs faster. Actual frame-time traces and the applied lower-resolution window
size were not collected. These observations leave the bottleneck unresolved.
Version 8 device rendering and FPS comparison are pending. Use the same world,
view, render distance and resolution when comparing, after chunks settle.
