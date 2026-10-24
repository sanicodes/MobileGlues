#!/usr/bin/env bash
# Run on a disposable Android emulator, never a connected phone.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
serial=${1:?Usage: run.sh emulator-SERIAL /path/to/release.apk}
apk=${2:?Usage: run.sh emulator-SERIAL /path/to/release.apk}
[[ $serial == emulator-* ]] || { echo 'Use a disposable emulator for these settings tests.' >&2; exit 1; }
sdk=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}
: "${sdk:?Set ANDROID_SDK_ROOT}"
tools=${ANDROID_BUILD_TOOLS:-$sdk/build-tools/36.0.0}
signing=${MOBILEGLUES_SIGNING_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/mobileglues-signing}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/app-classes" "$work/test-classes"
javac -source 8 -target 8 -Xlint:-options -classpath "$sdk/platforms/android-36/android.jar" \
    -d "$work/app-classes" "$root/MobileGlues-cpp/scripts/plugin/"*.java
javac -source 8 -target 8 -Xlint:-options -classpath "$sdk/platforms/android-36/android.jar:$work/app-classes" \
    -d "$work/test-classes" "$root/MobileGlues-cpp/tests/android/PluginSettingsTest.java"
"$tools/d8" --min-api 26 --lib "$sdk/platforms/android-36/android.jar" --classpath "$work/app-classes" \
    --output "$work" "$work/test-classes/com/sanicodes/mobileglues/tests/"*.class
cat > "$work/AndroidManifest.xml" <<'XML'
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.sanicodes.mobileglues.tests">
    <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36" />
    <application android:testOnly="true" android:label="MobileGlues settings checks" />
    <instrumentation android:name="com.sanicodes.mobileglues.tests.PluginSettingsTest"
        android:targetPackage="com.fcl.plugin.mobileglues.mc26test" />
</manifest>
XML
"$tools/aapt2" link --manifest "$work/AndroidManifest.xml" \
    -I "$sdk/platforms/android-36/android.jar" -o "$work/checks.apk"
python3 - "$work/checks.apk" "$work/classes.dex" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], 'a') as apk:
    apk.write(sys.argv[2], 'classes.dex')
PY
"$tools/apksigner" sign --ks "$signing/release.p12" --ks-pass "file:$signing/password" \
    --ks-key-alias mobileglues "$work/checks.apk"
adb -s "$serial" install -r "$apk"
adb -s "$serial" install -r -t "$work/checks.apk"
for mode in permission settings; do
    access=default
    [[ $mode == settings ]] && access=allow
    adb -s "$serial" shell appops set --uid com.fcl.plugin.mobileglues.mc26test MANAGE_EXTERNAL_STORAGE "$access"
    adb -s "$serial" shell am instrument -w -e mode "$mode" \
        com.sanicodes.mobileglues.tests/com.sanicodes.mobileglues.tests.PluginSettingsTest > "$work/$mode.log"
    cat "$work/$mode.log"
    python3 - "$work/$mode.log" <<'PY'
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
if 'INSTRUMENTATION_RESULT: result=PASS' not in text or 'restoreError=' in text:
    raise SystemExit('Settings checks failed.')
PY
done
mkdir -p "$root/build/android-settings-checks"
adb -s "$serial" pull /sdcard/Download/mobileglues-settings-check.png "$root/build/android-settings-checks/settings.png"
