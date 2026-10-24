# MobileGlues (Sanicodes) 2.0.0-sanicodes.6

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The app manages its renderer settings directly, including ANGLE,
error reporting, shader cache, extensions, OpenGL version, FSR 1, depth-clear
workarounds, renderer identity and multi-draw preference orders.

Version 6 optimizes the raw-depth sampling fix from version 5. Private samplers
are retained per texture unit and tracked context. An unchanged effective state
reuses the existing sampler without creating, deleting or configuring another
object. Effective texture/sampler settings are still read each draw, so native,
DSA and texture-only parameter changes remain visible. A changed state updates
the retained copy. Only active raw-depth inputs get overrides; original sampler
bindings and application texture/sampler parameters remain untouched.

Each tracked context reserves 128 CPU cache slots; private GL sampler objects
are allocated on demand. Shared contexts own separate copies. Retired names are deleted when another context in
the same share group is selected; destroying the final GL share-group context
also frees its GL resources. Nested borrowers and contexts without a dependable
identity use temporary copies with the version 5 lifetime.

The user reports improved rendering on version 5, with about 48 FPS in Fabulous
at 100% launcher resolution scale. Version 6 keeps graphics quality and the
existing settings unchanged. Host microbenchmarks measure this sampler path,
not Minecraft FPS; the device comparison is still required. See
[PERFORMANCE_MC26_3.md](PERFORMANCE_MC26_3.md) for methodology and measurements.
Version 3 remains withdrawn; its global texture-filter experiment is absent.

The real-driver checks cover depth readback, shared colour samplers, LOD/wrap
preservation, texture-only sampling, multiple depth layers, binding restoration,
program relinking and shadow comparisons. New checks verify warm reuse without
sampler allocation/configuration, native parameter changes, shared-context
isolation, deferred cleanup, nested borrowing and the untracked-context fallback.
The old host ANGLE build has a separate sampler-array remapping limitation that
also reproduces without MobileGlues; the test reports it explicitly. Mesa
validates the updated mapping numerically. Host success does not certify Mali
performance or full-game compatibility.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.6-arm64.apk` directly over
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
The native driver version is `2.0.0-sanicodes.6`. The new `MG/latest.log` starts
with `[MobileGlues] Build: 2.0.0-sanicodes.6`. The first affected draw logs
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
  emulator-5580 build/releases/MobileGlues-2.0.0-sanicodes.6-arm64.apk
```

Use a disposable emulator. The runner refuses physical-device serials and
restores the shared configuration after its screen checks. Its instrumentation
APK uses the local release signing identity and is not included in the release.
