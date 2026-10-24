# MobileGlues (Sanicodes) 2.0.0-sanicodes.1

An ARM64 release of the Sanicodes fork with the Minecraft 26.3 shader fixes.
The optimized native library uses CMake Release, ThinLTO and 16 KiB ELF page
alignment; the APK contains a stripped library and release DEX, with debugging
disabled. The launcher activity has a cube icon and shows the installed version
and renderer name.

The user reports that startup and the follow-up rendering checks worked on the
Xiaomi 2311DRK48G, Mali-G615, Android 16, Zalith 2.7.2 with ANGLE. This is a
fork release; compatibility beyond that setup has not been established.

## Install or update

Install `build/releases/MobileGlues-2.0.0-sanicodes.1-arm64.apk` directly over
the earlier test plugin. No uninstall is required. Restart Zalith and select
**MobileGlues (Sanicodes, OpenGL 4.0)**. Keep the official MobileGlues app for
its ANGLE and other shared settings.

The release preserves the earlier plugin's Android application ID and signing
certificate so Android accepts the update. The app label, renderer label and
version use the release name. The official plugin remains a separate app.
The native driver version is `2.0.0-sanicodes.1`.

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
