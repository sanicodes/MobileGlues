# Minecraft 26.3 shader compatibility test

This branch addresses the two shader errors in the Xiaomi 2311DRK48G / Mali-G615
launch log, using upstream commit `97558a69157b033b890958b1e49258c5b51a80c6`.
Device validation is still pending.

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

The APK is `build/test-plugin/MobileGlues-mc26.3-test-arm64.apk`. It has package
name `com.fcl.plugin.mobileglues.mc26test` and a local test signature. It installs
alongside the official MobileGlues plugin and contains only the ARM64 renderer
library and plugin metadata; it has no settings screen or launcher activity.

1. Install the test APK and fully restart Zalith Launcher.
2. In the isolated vanilla 26.3 instance, select
   **MobileGlues MC26.3 shader fix (test)** as the renderer.
3. Keep the existing ANGLE setting enabled and shader/program error ignoring
   disabled. Settings are read from the existing shared `/sdcard/MG` directory;
   use the official plugin's settings app if a setting needs changing.
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

Zalith's renderer discovery reads the `fclPlugin`, `renderer`, `des` and
`pojavEnv` metadata from installed applications; it does not require the
official package name. The test APK uses that same metadata protocol.

Related upstream report:
https://github.com/MobileGL-Dev/MobileGlues-release/issues/474
