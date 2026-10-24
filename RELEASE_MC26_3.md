# MobileGlues (Sanicodes) 2.0.0-sanicodes.7

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The app manages its renderer settings directly, including ANGLE,
error reporting, shader cache, extensions, OpenGL version, FSR 1, depth-clear
workarounds, renderer identity and multi-draw preference orders.

Version 7 is a recovery update after version 6 caused black rendering on the
user's Mali/ANGLE device. The sampler-reuse optimization is removed. Rendering
sources, native version and startup code exactly match the version 5 commit
`c7b21f6`; the APK ships the **byte-identical native library extracted from the
signed version 5 APK**. The independent settings app and Fabulous/raw-depth fix
from version 5 remain.

The APK version is `2.0.0-sanicodes.7`, versionCode `200007`, so it installs over
version 6 with the existing certificate. The native renderer intentionally
reports `2.0.0-sanicodes.5` because the known working binary is reused unchanged.
The user reported improved rendering and about 48 FPS with that renderer in
Fabulous at 100% resolution. Recovery on the current phone session still needs
to be confirmed. No new performance optimization is included.

Version 6 is withdrawn. Its host tests and microbenchmark gains did not predict
the device regression. [PERFORMANCE_MC26_3.md](PERFORMANCE_MC26_3.md) preserves the
failed experiment's measurements and device result for investigation.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.7-arm64.apk` directly over
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
The native driver version is intentionally `2.0.0-sanicodes.5`; `MG/latest.log`
starts with `[MobileGlues] Build: 2.0.0-sanicodes.5`. This is expected for the
version 7 recovery APK. The first affected draw logs
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
  emulator-5580 build/releases/MobileGlues-2.0.0-sanicodes.7-arm64.apk
```

Use a disposable emulator. The runner refuses physical-device serials and
restores the shared configuration after its screen checks. Its instrumentation
APK uses the local release signing identity and is not included in the release.
