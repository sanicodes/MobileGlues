# MobileGlues (Sanicodes) 2.0.0-sanicodes.5

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The app manages its renderer settings directly, including ANGLE,
error reporting, shader cache, extensions, OpenGL version, FSR 1, depth-clear
workarounds, renderer identity and multi-draw preference orders.

Version 5 adds a replacement for the Fabulous depth-sampling fix. It inspects
only active `sampler2D` inputs of the current program. A sized depth image with
comparison disabled and filtering that makes it incomplete on GLES receives a
private nearest-filtered sampler for that draw. The copy retains wrap, LOD,
comparison-function and supported border settings. Original texture and sampler
parameters stay unchanged; original bindings are restored and private objects
are deleted when the draw returns. Colour, integer and shadow samplers are left
on their existing paths. Program reflection is invalidated on creation, link,
binary load and deletion; sampler unit assignments are read at each draw.

This does not restore version 3's global texture-filter rewrites, sampler
unbinding fallback or new buffer-texture preparation on previously direct draw
forwards. Version 3 was withdrawn after black terrain and GUI textures on the
user's Mali/ANGLE device. Version 4 restored Fancy/Fast and remains the known
working fallback. The startup shader fixes and independent settings app remain.

Version 5 is a signed ARM64 Release build for manual device validation. Host
checks cover real production texture/draw wrappers with ANGLE Vulkan/SwiftShader
and Mesa GLES, depth readback, shared colour samplers, LOD/wrap preservation,
texture-only sampling, multiple depth layers, active-unit/binding restoration,
private-object cleanup, program relinking and shadow comparisons. The old host
ANGLE build has a separate sampler-array remapping limitation that reproduces
without MobileGlues; the test reports it explicitly and compares with the
legal direct-driver reference. Mesa validates the updated mapping numerically.
These checks do not establish Minecraft compatibility on Mali. Fabulous cloud
and water occlusion, ordinary textures and frame rate still need the phone test.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.5-arm64.apk` directly over
the earlier test plugin or Sanicodes release. No uninstall is required.

1. Open **MobileGlues (Sanicodes)** and tap **Allow access**. On Android 11+
   enable its all-files access in the system settings screen, then return.
   Android 8–10 use the normal storage permission prompt instead.
2. Existing `/sdcard/MG/config.json` settings are read without overwriting them.
   A fresh install creates defaults: prefer ANGLE, report errors, 32 MiB shader
   cache and OpenGL 4.0. Unknown settings and existing draw orders survive saves.
3. Make any changes and tap **Save settings**, or **Save and open Zalith**.
4. Force-stop and reopen Zalith so its Minecraft process loads the new library.
   Use **MobileGlues (Sanicodes, OpenGL 4.0)**. Check Fancy first, then Fabulous:
   clouds and water behind solid walls/roofs, stained glass, particles and item
   icons. Reopen Minecraft after changing graphics modes if a result is unclear.

The original MobileGlues app can now be uninstalled. ANGLE is supplied by the
launcher, not the original plugin app. Shared settings stay in `/sdcard/MG`,
so uninstalling the original app does not require moving or recreating them.
The settings permission is needed because both the app and the launcher must
access that shared directory; it is not used to scan unrelated files.

The release preserves the earlier plugin's Android application ID and signing
certificate so Android accepts the update. The app label, renderer label and
version use the release name. The official plugin remains a separate app.
The native driver version is `2.0.0-sanicodes.5`. The new `MG/latest.log` starts
with `[MobileGlues] Build: 2.0.0-sanicodes.5`. The first affected draw logs
`Raw depth sampling: using a private nearest-filtered sampler`.

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

The unchanged settings UI passed 29 checks in release 2 on an Android 16
emulator without the original MobileGlues app installed: the permission gate,
defaults, persistence, native numeric values, preservation of unknown keys and
draw orders, invalid input, corrupt-file recovery, failed writes, cache
clearing, actual screen save/load and unsaved edits across activity recreation.
The release also updates the previous signed APK in place.

```sh
ANDROID_SDK_ROOT=/path/to/sdk bash MobileGlues-cpp/tests/android/run.sh \
  emulator-5580 build/releases/MobileGlues-2.0.0-sanicodes.5-arm64.apk
```

Use a disposable emulator. The runner refuses physical-device serials and
restores the shared configuration after its screen checks. Its instrumentation
APK uses the local release signing identity and is not included in the release.
