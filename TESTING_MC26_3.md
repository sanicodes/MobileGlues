# Minecraft 26.3 shader compatibility test

This branch addresses the two shader errors in the Xiaomi 2311DRK48G / Mali-G615
launch log, using upstream commit `97558a69157b033b890958b1e49258c5b51a80c6`.
The user reports that startup and the follow-up rendering checks passed on
that Mali/ANGLE setup. The packaged release is documented in
[RELEASE_MC26_3.md](RELEASE_MC26_3.md).

Validated locally: the unpatched translator fails five regression cases; this
branch passes those cases, all 81 selected Minecraft fragment shader variants,
and the existing pixel/framebuffer tests. A surfaceless GLES render check on an
NVIDIA GTX 1050 Ti verified the two output locations and read back the expected
pixels after normal completion, an early return and discard (zero mismatches,
no GL error). This desktop check does not replace testing on Mali/ANGLE.

## Changes

- Match the complete `uniform` keyword during initializer cleanup. Generated
  instance names such as `_uniform` and `_uniform_0` must survive comparisons.
- Translate fragment output arrays through private arrays. SPIRV-Cross's entry
  function return hooks copy each element back to its original output using a
  literal index, preserving names, locations, early returns and discard.
  Vertex-stage output arrays are left to the normal compiler path.
- Include a translator revision in the disk cache key so the patched build
  does not load previously generated invalid ESSL.

The C++ SPIRV-Cross backend replaces its C wrapper to allow the output-array
adaptation without modifying the pinned dependency. Compiler options are still
ESSL, with a minimum target of version 300. Ownership remains scoped to each
translation, including exception paths.

## Manual test on the phone

The APK is `build/test-plugin/MobileGlues-mc26.3-test2-arm64.apk`. It has package
name `com.fcl.plugin.mobileglues.mc26test` and a local test signature. It installs
alongside the official MobileGlues plugin and contains the ARM64 renderer
library, plugin metadata and a small informational launcher activity.
Test 2 updates test 1 in place using the same package and signing key.
The original test 1 APK lacked a MAIN activity and was invisible to Zalith's
plugin discovery; use test 2 instead.
An Android 16 emulator reproduced that difference using the same
`queryIntentActivities(ACTION_MAIN, GET_META_DATA | GET_SHARED_LIBRARY_FILES)`
call as Zalith: test 1 returned zero matching plugins, while updating it to
test 2 returned one, with the expected renderer metadata and the native
library extracted successfully. The update used the existing signing key.

1. Install the test APK and fully restart Zalith Launcher.
2. In the isolated vanilla 26.3 instance, select
   **MobileGlues MC26.3 shader fix (test)** as the renderer.
3. Keep the existing ANGLE setting enabled and shader/program error ignoring
   disabled. Settings are read from the existing shared `/sdcard/MG` directory.
   Sanicodes release 2 provides its own settings screen; the older test APKs
   used the official plugin's settings app.
4. Launch the game. Confirm the launcher log's renderer library path contains
   `com.fcl.plugin.mobileglues.mc26test` so the correct build is being tested.
5. Check the main menu and create a disposable world. Check terrain, text,
   entities, clouds, water and glass, then restart the game to exercise caching.
6. Report whether startup succeeds and whether those objects render correctly.
   If it fails, provide the complete launcher log and `/sdcard/MG/latest.log`.

Rollback: select the original MobileGlues renderer and remove only the test
plugin. The test APK does not replace Zalith or the official plugin.

## Reproduce the host tests (Linux)

Initialize the four build dependencies:

```sh
git submodule update --init --depth 1 \
  MobileGlues-cpp/3rdparty/glslang MobileGlues-cpp/3rdparty/SPIRV-Cross \
  MobileGlues-cpp/3rdparty/xxhash MobileGlues-cpp/include/ska
cmake -S MobileGlues-cpp -B build/host -DCMAKE_BUILD_TYPE=Debug \
  -DMOBILEGLUES_BUILD_SHADER_TESTS=ON
cmake --build build/host --target shader_translation_test -j 4
ctest --test-dir build/host -R '^shader_translation$' --output-on-failure
sh MobileGlues-cpp/tests/run.sh
python3 MobileGlues-cpp/tests/minecraft_shaders.py \
  /path/to/26.3.jar build/host/shader_translation_test
```

The Minecraft check expands includes directly from the supplied client JAR,
without storing or redistributing game assets. It validates 81 selected core
fragment shader variants: ordinary shaders, transparency transmittance,
accumulation, depth bounds, multidraw terrain and terrain cutout. These are
compiler checks and do not establish that rendering works on the phone.

## Build the ARM64 library and test APK

Set `ANDROID_SDK_ROOT` and `ANDROID_NDK_HOME` to installed SDK and NDK paths.
This test build used NDK 27.1.12297006, CMake 3.22.1 and build-tools 36.0.0.
The upstream Gradle configuration specifies NDK 27.3.13750724; Gradle is not
needed for the library or this small test APK.

```sh
cmake -S MobileGlues-cpp -B build/arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_ANDROID_STL_TYPE=c++_static
cmake --build build/arm64 --target mobileglues -j 4
bash MobileGlues-cpp/scripts/package-test-plugin.sh build/arm64/libmobileglues.so
```

Packaging strips a copy of the library when `ANDROID_NDK_HOME` is set, includes
license notices, aligns and signs the APK, and verifies its signature and
manifest. The locally generated signing key stays under the ignored `build/`
directory. It is a development key with the conventional password `android`.
No official signing key is used.

Zalith's plugin loader first calls `queryIntentActivities(Intent(ACTION_MAIN))`.
It then reads the `fclPlugin`, `renderer`, `des` and `pojavEnv` metadata from
those applications; it does not require the official package name. The test
APK provides both a MAIN activity and the renderer metadata. Packaging checks
that the final APK exposes the launchable activity.

Related upstream report:
https://github.com/MobileGL-Dev/MobileGlues-release/issues/474


## Fabulous raw-depth sampling (Sanicodes 5)

The optional real-driver test links `texture.cpp`, `drawing.cpp`, native
forwards, pixel transfer and the pure depth predicates. Shader compilation and
framebuffer setup use the real GLES driver, bypassing unrelated shader/FBO
translation. Sampler state is inspected inside the production draw forward.

```sh
MG_GLES_TEST_ANGLE_DIR="$ANDROID_SDK_ROOT/emulator/lib64/gles_angle" \
VK_ICD_FILENAMES=/path/to/vk_swiftshader_icd.json \
sh MobileGlues-cpp/tests/run-depth-gles.sh
```

With no ANGLE directory the test uses surfaceless system EGL. For Mesa software
rendering set `LIBGL_ALWAYS_SOFTWARE=1` and select the Mesa EGL vendor if needed.
The old emulator ANGLE build `5fa2e61da947` does not update sampler-array mappings
correctly after a draw, including a direct driver draw with legal filters; that
case is reported explicitly. The guard must match the legal driver reference,
and Mesa must produce both expected numeric layer-composite results. No failure
from a different ANGLE revision is suppressed.

Device acceptance: confirm native build `2.0.0-sanicodes.5`, start in Fancy, then
check Fabulous with clouds/water behind solid geometry. Check terrain, GUI/item
textures, glass, particles and FPS. Host test success is not a phone test result.
