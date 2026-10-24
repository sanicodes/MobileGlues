#!/usr/bin/env bash
# Package an already-built arm64 library as a separate Zalith/FCL test plugin.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
sdk=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}
: "${sdk:?Set ANDROID_SDK_ROOT to your Android SDK}"
lib=${1:?Usage: package-test-plugin.sh /path/to/libmobileglues.so [output.apk]}
lib=$(realpath "$lib")
output=${2:-$root/build/test-plugin/MobileGlues-mc26.3-test2-arm64.apk}
mkdir -p "$(dirname "$output")"
output=$(realpath "$output")
tools=${ANDROID_BUILD_TOOLS:-$sdk/build-tools/36.0.0}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat > "$work/AndroidManifest.xml" <<'XML'
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.fcl.plugin.mobileglues.mc26test"
    android:versionCode="2" android:versionName="2.0.0-mc26.3-test2">
    <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36" />
    <application android:label="MobileGlues MC26.3 test"
        android:extractNativeLibs="true">
        <activity android:name="com.fcl.plugin.mobileglues.mc26test.MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        <meta-data android:name="fclPlugin" android:value="true" />
        <meta-data android:name="des" android:value="MobileGlues MC26.3 shader fix (test)" />
        <meta-data android:name="renderer" android:value="MobileGlues:libmobileglues.so:libmobileglues.so" />
        <meta-data android:name="pojavEnv" android:value="LIBGL_ES=3:POJAV_RENDERER=opengles3:POJAVEXEC_EGL=libmobileglues.so:LIBGL_EGL=libmobileglues.so:MG_COUNT_LAUNCH=1" />
        <meta-data android:name="boatEnv" android:value="LIBGL_ES=3:MG_COUNT_LAUNCH=1" />
        <meta-data android:name="minMCVer" android:value="1.17" />
    </application>
</manifest>
XML
mkdir -p "$work/classes"
javac -source 8 -target 8 -Xlint:-options -classpath "$sdk/platforms/android-36/android.jar" \
    -d "$work/classes" "$root/MobileGlues-cpp/scripts/test-plugin/MainActivity.java"
"$tools/d8" --min-api 26 --lib "$sdk/platforms/android-36/android.jar" \
    --output "$work" "$work/classes/com/fcl/plugin/mobileglues/mc26test/MainActivity.class"
"$tools/aapt2" link --manifest "$work/AndroidManifest.xml" \
    -I "$sdk/platforms/android-36/android.jar" -o "$work/unsigned.apk"
cp "$lib" "$work/libmobileglues.so"
if [[ -n ${ANDROID_NDK_HOME:-} ]]; then
    "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" \
        --strip-unneeded "$work/libmobileglues.so"
fi
python3 - "$work/unsigned.apk" "$work/libmobileglues.so" "$root" "$work/classes.dex" <<'PY'
import pathlib, sys, zipfile
root = pathlib.Path(sys.argv[3])
with zipfile.ZipFile(sys.argv[1], 'a', compression=zipfile.ZIP_DEFLATED) as apk:
    apk.write(sys.argv[2], 'lib/arm64-v8a/libmobileglues.so')
    apk.write(sys.argv[4], 'classes.dex')
    apk.write(root / 'LICENSE', 'assets/licenses/MobileGlues-LGPL-2.1.txt')
    for dep in ['glslang', 'SPIRV-Cross', 'xxhash']:
        directory = root / 'MobileGlues-cpp/3rdparty' / dep
        for path in directory.rglob('*'):
            if path.is_file() and (path.name.startswith(('LICENSE', 'COPYING')) or
                                   path.name == 'copyright'):
                apk.write(path, 'assets/licenses/' + dep + '/' + str(path.relative_to(directory)))
    apk.write(root / 'MobileGlues-cpp/include/ska/flat_hash_map.hpp',
              'assets/licenses/flat_hash_map.hpp')
PY
"$tools/zipalign" -f 4 "$work/unsigned.apk" "$work/aligned.apk"
key="$root/build/test-plugin/local-test.jks"
if [[ ! -f $key ]]; then
    mkdir -p "$(dirname "$key")"
    keytool -genkeypair -keystore "$key" -storepass android -keypass android \
        -alias local-test -dname 'CN=MobileGlues local test' -keyalg RSA \
        -keysize 2048 -validity 3650 >/dev/null 2>&1
fi
"$tools/apksigner" sign --ks "$key" --ks-pass pass:android --key-pass pass:android \
    --ks-key-alias local-test --out "$output" "$work/aligned.apk"
"$tools/apksigner" verify "$output"
"$tools/aapt2" dump badging "$output"
"$tools/aapt2" dump badging "$output" | \
    grep -F "launchable-activity: name='com.fcl.plugin.mobileglues.mc26test.MainActivity'" >/dev/null
sha256sum "$output"
