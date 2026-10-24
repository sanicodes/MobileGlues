# MobileGlues (Sanicodes) 2.0.0-sanicodes.8

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The app manages its renderer settings directly, including ANGLE,
error reporting, shader cache, extensions, OpenGL version, FSR 1, depth-clear
workarounds, renderer identity and multi-draw preference orders.

Version 8 makes one narrow change to the version 5 renderer restored in v7:
it skips parameter writes when a newly generated private sampler already has
the required default value. All source state is still read on every affected
draw. Non-default wrap, LOD, comparison-function and border values are copied;
border comparison preserves signed zero. The two depth-legal filters are still
set explicitly. Samplers are created and deleted within each draw as in v5.
There is no cross-draw sampler reuse, cache or context-lifetime change.

Real-driver tests compare the complete relevant private sampler state with the
effective source settings, including default/non-default/default transitions,
signed-zero border data, depth readback, shared colour samplers, texture-only
sampling and shadow comparisons. ANGLE passes 97 checks; Mesa passes 104.
The old host ANGLE sampler-array mapping limitation is still reported separately
and reproduced in the direct-driver control.

An identical-harness host benchmark reduces setup writes from 9 (ANGLE) / 10
(Mesa) to 5 for the Minecraft-style fixture. Sampler creation/deletion counts
remain unchanged. Guard CPU time is about 12–13% lower and tiny draw time about
3–4% lower in this run. These are host microbenchmarks, not Minecraft FPS gains;
phone rendering and performance remain unconfirmed. See
[PERFORMANCE_MC26_3_V8.md](PERFORMANCE_MC26_3_V8.md).

The user reports about 48 FPS in-world with v7 at 100% resolution, unchanged in
quick lower-resolution and VSync-off tests; main-menu FPS is higher. Those
observations do not establish the exact bottleneck. Version 6 remains withdrawn
for black rendering. Version 7 is the recovery package with the byte-identical
v5 native renderer.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.8-arm64.apk` directly over
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
The native driver version is `2.0.0-sanicodes.8`; `MG/latest.log` starts with
`[MobileGlues] Build: 2.0.0-sanicodes.8`. The startup renderer line states that
fresh per-draw samplers are used and redundant default writes are skipped.

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
  emulator-5580 build/releases/MobileGlues-2.0.0-sanicodes.8-arm64.apk
```

Use a disposable emulator. The runner refuses physical-device serials and
restores the shared configuration after its screen checks. Its instrumentation
APK uses the local release signing identity and is not included in the release.
