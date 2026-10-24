#!/usr/bin/env bash
# Package an already-built arm64 library as the Sanicodes Zalith/FCL plugin.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
sdk=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}
: "${sdk:?Set ANDROID_SDK_ROOT to your Android SDK}"
lib=${1:?Usage: package-plugin.sh /path/to/libmobileglues.so [output.apk]}
lib=$(realpath "$lib")
if [[ ${MOBILEGLUES_CHANNEL:-release} == test ]]; then
    app_name='MobileGlues MC26.3 test'
    description='MobileGlues MC26.3 shader fix (test)'
    version_name='2.0.0-mc26.3-test2'
    version_code=2
    default_output="$root/build/test-plugin/MobileGlues-mc26.3-test2-arm64.apk"
else
    app_name='MobileGlues (Sanicodes)'
    description='MobileGlues (Sanicodes, OpenGL 4.0)'
    version_name='2.0.0-sanicodes.8'
    version_code=200008
    default_output="$root/build/releases/MobileGlues-2.0.0-sanicodes.8-arm64.apk"
fi
output=${2:-$default_output}
mkdir -p "$(dirname "$output")"
output=$(realpath "$output")
tools=${ANDROID_BUILD_TOOLS:-$sdk/build-tools/36.0.0}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat > "$work/AndroidManifest.xml" <<XML
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.fcl.plugin.mobileglues.mc26test"
    android:versionCode="$version_code" android:versionName="$version_name">
    <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36" />
    <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" android:maxSdkVersion="29" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" android:maxSdkVersion="29" />
    <queries>
        <package android:name="com.movtery.zalithlauncher.v2" />
        <package android:name="com.movtery.zalithlauncher.v2.debug" />
    </queries>
    <application android:label="$app_name" android:icon="@drawable/ic_launcher"
        android:theme="@style/PluginTheme" android:requestLegacyExternalStorage="true"
        android:debuggable="false" android:extractNativeLibs="true">
        <activity android:name="com.fcl.plugin.mobileglues.mc26test.MainActivity"
            android:windowSoftInputMode="adjustResize|stateAlwaysHidden"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        <meta-data android:name="fclPlugin" android:value="true" />
        <meta-data android:name="des" android:value="$description" />
        <meta-data android:name="renderer" android:value="MobileGlues:libmobileglues.so:libmobileglues.so" />
        <meta-data android:name="pojavEnv" android:value="LIBGL_ES=3:POJAV_RENDERER=opengles3:POJAVEXEC_EGL=libmobileglues.so:LIBGL_EGL=libmobileglues.so:MG_COUNT_LAUNCH=1" />
        <meta-data android:name="boatEnv" android:value="LIBGL_ES=3:MG_COUNT_LAUNCH=1" />
        <meta-data android:name="minMCVer" android:value="1.17" />
    </application>
</manifest>
XML
mkdir -p "$work/classes"
javac -source 8 -target 8 -Xlint:-options -classpath "$sdk/platforms/android-36/android.jar" \
    -d "$work/classes" "$root/MobileGlues-cpp/scripts/plugin/"*.java
"$tools/d8" --release --min-api 26 --lib "$sdk/platforms/android-36/android.jar" \
    --output "$work" "$work/classes/com/fcl/plugin/mobileglues/mc26test/"*.class
"$tools/aapt2" compile --dir "$root/MobileGlues-cpp/scripts/plugin/res" -o "$work/resources.zip"
"$tools/aapt2" link --manifest "$work/AndroidManifest.xml" \
    -I "$sdk/platforms/android-36/android.jar" -o "$work/unsigned.apk" "$work/resources.zip"
cp "$lib" "$work/libmobileglues.so"
if [[ -n ${ANDROID_NDK_HOME:-} ]]; then
    "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" \
        --strip-unneeded "$work/libmobileglues.so"
fi
python3 - "$work/unsigned.apk" "$work/libmobileglues.so" "$root" "$work/classes.dex" "$version_name" <<'PY'
import hashlib, json, pathlib, subprocess, sys, zipfile
root = pathlib.Path(sys.argv[3])
with zipfile.ZipFile(sys.argv[1], 'a', compression=zipfile.ZIP_DEFLATED) as apk:
    apk.write(sys.argv[2], 'lib/arm64-v8a/libmobileglues.so')
    apk.write(sys.argv[4], 'classes.dex')
    apk.writestr('assets/build-info.json', json.dumps({
        'version': sys.argv[5],
        'source': 'https://github.com/sanicodes/MobileGlues',
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'librarySha256': hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest(),
    }, indent=2) + '\n')
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
if [[ ${MOBILEGLUES_CHANNEL:-release} == test ]]; then
    key="$root/build/test-plugin/local-test.jks"
    if [[ ! -f $key ]]; then
        mkdir -p "$(dirname "$key")"
        keytool -genkeypair -keystore "$key" -storepass android -keypass android \
            -alias local-test -dname 'CN=MobileGlues local test' -keyalg RSA \
            -keysize 2048 -validity 3650 >/dev/null 2>&1
    fi
    "$tools/apksigner" sign --ks "$key" --ks-pass pass:android --key-pass pass:android \
        --ks-key-alias local-test --out "$output" "$work/aligned.apk"
else
    signing_dir=${MOBILEGLUES_SIGNING_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/mobileglues-signing}
    key="$signing_dir/release.p12"
    password_file="$signing_dir/password"
    if [[ ! -f $key ]]; then
        (umask 077; mkdir -p "$signing_dir")
        python3 - "$password_file" <<'PY'
import os, secrets, sys
with open(os.open(sys.argv[1], os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'w') as f:
    f.write(secrets.token_urlsafe(48) + '\n')
PY
        if [[ -f $root/build/test-plugin/local-test.jks ]]; then
            # Preserve the installed certificate so users can update in place.
            keytool -importkeystore -noprompt \
                -srckeystore "$root/build/test-plugin/local-test.jks" \
                -srcstorepass android -srckeypass android -srcalias local-test \
                -destkeystore "$key" -deststoretype PKCS12 -destalias mobileglues \
                -deststorepass:file "$password_file" -destkeypass:file "$password_file"
        else
            keytool -genkeypair -keystore "$key" -storetype PKCS12 \
                -storepass:file "$password_file" -keypass:file "$password_file" \
                -alias mobileglues -dname 'CN=MobileGlues Sanicodes' -keyalg RSA \
                -keysize 2048 -validity 10000
        fi
        chmod 600 "$key"
    fi
    "$tools/apksigner" sign --ks "$key" \
        --ks-pass "file:$password_file" \
        --ks-key-alias mobileglues --out "$output" "$work/aligned.apk"
fi
"$tools/apksigner" verify "$output"
"$tools/aapt2" dump badging "$output"
"$tools/aapt2" dump badging "$output" | \
    grep -F "launchable-activity: name='com.fcl.plugin.mobileglues.mc26test.MainActivity'" >/dev/null
sha256sum "$output"
