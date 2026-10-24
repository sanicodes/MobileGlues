# MobileGlues (Sanicodes) 2.0.0-sanicodes.3

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The app manages its renderer settings directly, including ANGLE,
error reporting, shader cache, extensions, OpenGL version, FSR 1, depth-clear
workarounds, renderer identity and multi-draw preference orders.

The user reports that startup and the follow-up rendering checks worked on the
Xiaomi 2311DRK48G, Mali-G615, Android 16, Zalith 2.7.2 with ANGLE. This is a
fork release; compatibility beyond that setup has not been established.

Version 3 addresses depth sampling in Fabulous / improved transparency mode,
where water and clouds could appear over solid terrain. The depth-texture
filter correction comes from HEBEI77's proposed upstream
[PR #61](https://github.com/MobileGL-Dev/MobileGlues/pull/61), retained with its
original commit authorship. Additional fixes keep nested draw guards from
restoring sampler bindings too early, restore the correct active texture unit,
and avoid recording sampler bindings the driver rejected. Unknown texture
targets are also handled safely by the newly intercepted parameter queries.

The correction applies to sized 2D depth/depth-stencil textures. Colour units
sharing the same sampler keep their filtering. Shadow comparison filtering is
preserved. Depth arrays and cube-map sampling are outside this correction.

Validation for this change: 31 filter-decision checks, 20 tests of the actual
texture/sampler wrappers with a driver model, the existing pixel/framebuffer
tests, 81 Minecraft fragment shader variants, and a Release ARM64 build pass.
On ANGLE/Vulkan/SwiftShader, the GLES test reproduced a stored depth of 0.75
sampling as zero through the original filter pairing; the corrected pairing
returned 191/255 with no GL error. Fabulous rendering on the user's Mali phone
still needs a visual retest of the version 3 APK.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.3-arm64.apk` directly over
the earlier test plugin or Sanicodes release. No uninstall is required.

1. Open **MobileGlues (Sanicodes)** and tap **Allow access**. On Android 11+
   enable its all-files access in the system settings screen, then return.
   Android 8–10 use the normal storage permission prompt instead.
2. Existing `/sdcard/MG/config.json` settings are read without overwriting them.
   A fresh install creates defaults: prefer ANGLE, report errors, 32 MiB shader
   cache and OpenGL 4.0. Unknown settings and existing draw orders survive saves.
3. Make any changes and tap **Save settings**, or **Save and open Zalith**.
4. Restart Minecraft using **MobileGlues (Sanicodes, OpenGL 4.0)** in Zalith.

The original MobileGlues app can now be uninstalled. ANGLE is supplied by the
launcher, not the original plugin app. Shared settings stay in `/sdcard/MG`,
so uninstalling the original app does not require moving or recreating them.
The settings permission is needed because both the app and the launcher must
access that shared directory; it is not used to scan unrelated files.

The release preserves the earlier plugin's Android application ID and signing
certificate so Android accepts the update. The app label, renderer label and
version use the release name. The official plugin remains a separate app.
The native driver version is `2.0.0-sanicodes.3`.

Cache clearing removes only the shader cache and its pending write, and asks
that Minecraft be closed first. Restoring defaults keeps the previous settings
as `config.before-reset.json`. Invalid JSON is reported without replacing the
file automatically. Changes are saved atomically and disk errors are shown.

## Build

Initialize the dependencies as described in [TESTING_MC26_3.md](TESTING_MC26_3.md)
and set `ANDROID_SDK_ROOT` and `ANDROID_NDK_HOME` to the installed SDK and NDK.

```sh
cmake -S MobileGlues-cpp -B build/arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_ANDROID_STL_TYPE=c++_static
cmake --build build/arm64 --target mobileglues -j 4
bash MobileGlues-cpp/scripts/package-plugin.sh build/arm64/libmobileglues.so
```

Packaging uses SDK build-tools 36.0.0 by default, or `ANDROID_BUILD_TOOLS` when
set. The APK includes license notices and `assets/build-info.json`, recording
the source commit, fork URL, version and packaged library SHA-256.

## Signing continuity

Release signing material is kept outside the repository at
`${XDG_DATA_HOME:-$HOME/.local/share}/mobileglues-signing`, or
`MOBILEGLUES_SIGNING_DIR` when set. The directory holds `release.p12` and its
randomly generated password file, with private filesystem permissions.
Back up both files to retain the ability to sign future updates.

On this machine, the earlier test signing identity was imported into that
password-protected release store. Its certificate is unchanged for Android
update compatibility. A fresh checkout with no earlier identity generates a
new one; independently generated identities cannot update existing installs.
No signing key or password is included in the APK or committed to Git.

`package-test-plugin.sh` remains as a compatibility entry point for the earlier
test packaging instructions. Normal releases use `package-plugin.sh`.

## Android settings checks

On an Android 16 emulator without the original MobileGlues app installed,
29 checks passed against the installed release APK: the permission gate,
defaults, persistence, native numeric values, preservation of unknown keys and
draw orders, invalid input, corrupt-file recovery, failed writes, cache
clearing, actual screen save/load and unsaved edits across activity recreation.
The release also updates the previous signed APK in place.

```sh
ANDROID_SDK_ROOT=/path/to/sdk bash MobileGlues-cpp/tests/android/run.sh \
  emulator-5580 build/releases/MobileGlues-2.0.0-sanicodes.3-arm64.apk
```

Use a disposable emulator. The runner refuses physical-device serials and
restores the shared configuration after its screen checks. Its instrumentation
APK uses the local release signing identity and is not included in the release.

## Optional depth-sampling GLES check (Linux)

```sh
g++ -std=gnu++20 -I MobileGlues-cpp -I MobileGlues-cpp/include \
  MobileGlues-cpp/tests/depth_filter_gles_test.cpp \
  MobileGlues-cpp/gl/depth_filter.cpp -ldl -o /tmp/mg_depth_filter_gles_test
MG_GLES_TEST_ANGLE_DIR="$ANDROID_SDK_ROOT/emulator/lib64/gles_angle" \
  VK_ICD_FILENAMES=/path/to/vk_swiftshader_icd.json /tmp/mg_depth_filter_gles_test
```

Without `MG_GLES_TEST_ANGLE_DIR`, it uses the system surfaceless EGL/GLES driver.
Some desktop drivers permit the original filter pairing, so only the explicit
ANGLE mode requires the original sample to be zero. Both modes require the
corrected sample to match the stored depth and the draw to return no GL error.
