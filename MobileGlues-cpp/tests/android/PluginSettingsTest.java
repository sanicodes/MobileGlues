// SPDX-License-Identifier: LGPL-2.1-only
package com.sanicodes.mobileglues.tests;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import com.fcl.plugin.mobileglues.mc26test.SettingsStore;
import org.json.JSONObject;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicReference;

/** Runs against the installed release APK, using its actual settings classes. */
public final class PluginSettingsTest extends Instrumentation {
    private Bundle arguments;
    private int checks;
    private Activity activity;
    private File sharedDirectory;
    private byte[] originalConfig;
    private boolean hadConfig;
    private boolean touchedShared;

    @Override public void onCreate(Bundle arguments) { this.arguments = arguments; start(); }

    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            if ("permission".equals(arguments.getString("mode"))) {
                activity = openActivity();
                waitFor(() -> tagged("storageAccess") != null);
                require(tagged("enableANGLE") == null, "Settings must be gated before access is granted.");
            } else {
                storeTests();
                activityTests();
            }
            result.putString("result", "PASS");
            result.putInt("checks", checks);
        } catch (Throwable error) {
            result.putString("result", "FAIL");
            result.putString("error", android.util.Log.getStackTraceString(error));
        } finally {
            if (activity != null) runOnMainSync(() -> activity.finish());
            if (touchedShared) {
                try {
                    File config = new File(sharedDirectory, "config.json");
                    if (hadConfig) Files.write(config.toPath(), originalConfig);
                    else Files.deleteIfExists(config.toPath());
                } catch (Exception error) { result.putString("restoreError", error.toString()); }
            }
        }
        finish(Activity.RESULT_OK, result);
    }

    private void storeTests() throws Exception {
        File root = new File(getTargetContext().getCacheDir(), "settings-test-" + System.nanoTime());
        require(root.mkdirs(), "Create isolated test folder.");
        SettingsStore store = new SettingsStore(root);
        JSONObject initial = store.loadOrCreate();
        require(initial.getInt("enableANGLE") == 1, "Fresh install prefers ANGLE.");
        require(initial.getInt("enableNoError") == 0, "Fresh install reports errors.");
        require(initial.get("enableExtTimerQuery") instanceof Number, "Native flags are numeric.");
        initial.put("custom-provider", new JSONObject().put("retained", true));
        initial.put("multidrawOrderElements", "indirect,unroll");
        initial.put("maxGlslCacheSize", 81);
        store.save(initial);
        JSONObject loaded = store.loadOrCreate();
        require(loaded.getJSONObject("custom-provider").getBoolean("retained"), "Preserve unknown settings.");
        require(loaded.getString("multidrawOrderElements").equals("indirect,unroll"), "Preserve draw orders.");
        require(loaded.getInt("maxGlslCacheSize") == 81, "Read persisted cache setting.");
        File file = new File(root, "config.json");
        byte[] validBytes = Files.readAllBytes(file.toPath());
        loaded.put("enableANGLE", 99);
        try { store.save(loaded); throw new AssertionError("Invalid ANGLE setting accepted."); }
        catch (org.json.JSONException expected) { checks++; }
        require(java.util.Arrays.equals(validBytes, Files.readAllBytes(file.toPath())), "Failed save preserves original.");
        Files.write(file.toPath(), "invalid-json".getBytes(StandardCharsets.UTF_8));
        try { store.loadOrCreate(); throw new AssertionError("Corrupt settings accepted."); }
        catch (org.json.JSONException expected) { checks++; }
        require(new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8).equals("invalid-json"),
                "Corrupt settings are not overwritten automatically.");
        store.reset();
        require(new String(Files.readAllBytes(new File(root, "config.before-reset.json").toPath()),
                StandardCharsets.UTF_8).equals("invalid-json"), "Reset backs up the original.");
        File cache = new File(root, "glsl_cache.tmp");
        File pending = new File(root, "glsl_cache.tmp.new");
        File log = new File(root, "latest.log");
        Files.write(cache.toPath(), new byte[100]); Files.write(pending.toPath(), new byte[30]);
        Files.write(log.toPath(), "keep".getBytes(StandardCharsets.UTF_8));
        require(store.cacheBytes() == 100, "Read cache size.");
        store.clearShaderCache();
        require(!cache.exists() && !pending.exists(), "Clear both cache files.");
        require(log.isFile() && file.isFile(), "Cache clearing preserves logs and settings.");
        File badParent = new File(root, "not-a-directory");
        Files.write(badParent.toPath(), new byte[]{1});
        try { new SettingsStore(badParent).save(SettingsStore.defaults());
            throw new AssertionError("Unwritable settings destination accepted."); }
        catch (java.io.IOException expected) { checks++; }
    }

    private void activityTests() throws Exception {
        sharedDirectory = new File(Environment.getExternalStorageDirectory(), "MG");
        File file = new File(sharedDirectory, "config.json");
        hadConfig = file.isFile();
        if (hadConfig) originalConfig = Files.readAllBytes(file.toPath());
        touchedShared = true;
        SettingsStore store = new SettingsStore(sharedDirectory);
        JSONObject fixture = SettingsStore.defaults().put("enableANGLE", 3).put("maxGlslCacheSize", 81)
                .put("custom-provider", new JSONObject().put("retained", true))
                .put("multidrawOrderElements", "indirect,unroll");
        store.save(fixture);
        activity = openActivity();
        waitFor(() -> tagged("enableANGLE") != null);
        waitForIdleSync();
        runOnMainSync(() -> {
            try {
                View root = activity.getWindow().getDecorView();
                android.graphics.Bitmap image = android.graphics.Bitmap.createBitmap(root.getWidth(), root.getHeight(),
                        android.graphics.Bitmap.Config.ARGB_8888);
                root.draw(new android.graphics.Canvas(image));
                File imageFile = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                        "mobileglues-settings-check.png");
                try (java.io.FileOutputStream output = new java.io.FileOutputStream(imageFile)) {
                    image.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, output);
                }
                image.recycle();
            } catch (Exception error) { throw new AssertionError(error); }
        });
        runOnMainSync(() -> {
            require(((Spinner) tagged("enableANGLE")).getSelectedItemPosition() == 3, "Load existing ANGLE setting.");
            require(((EditText) tagged("maxGlslCacheSize")).getText().toString().equals("81"), "Load existing cache setting.");
            ((Spinner) tagged("enableANGLE")).setSelection(1);
            ((EditText) tagged("maxGlslCacheSize")).setText("64");
            ((CheckBox) tagged("enableExtDirectStateAccess")).setChecked(true);
            tagged("save").performClick();
        });
        waitFor(() -> store.loadOrCreate().getInt("maxGlslCacheSize") == 64);
        JSONObject written = store.loadOrCreate();
        require(written.getInt("enableANGLE") == 1, "Save ANGLE setting from actual screen.");
        require(written.getInt("enableExtDirectStateAccess") == 1, "Save advanced flag in native format.");
        require(written.getJSONObject("custom-provider").getBoolean("retained"), "Screen save preserves unknown settings.");
        require(written.getString("multidrawOrderElements").equals("indirect,unroll"), "Screen save preserves draw orders.");
        waitFor(() -> tagged("save").isEnabled());
        runOnMainSync(() -> {
            ((EditText) tagged("maxGlslCacheSize")).setText("1025");
            tagged("save").performClick();
            require(((EditText) tagged("maxGlslCacheSize")).getError() != null, "Reject oversized cache on screen.");
        });
        require(store.loadOrCreate().getInt("maxGlslCacheSize") == 64, "Invalid screen input does not overwrite settings.");
        ActivityMonitor monitor = addMonitor(activity.getClass().getName(), null, false);
        runOnMainSync(() -> {
            ((EditText) tagged("maxGlslCacheSize")).setText("57");
            ((Spinner) tagged("enableANGLE")).setSelection(2);
            activity.recreate();
        });
        activity = waitForMonitorWithTimeout(monitor, 5000);
        removeMonitor(monitor);
        require(activity != null, "Activity recreated.");
        waitFor(() -> tagged("enableANGLE") != null);
        runOnMainSync(() -> {
            require(((EditText) tagged("maxGlslCacheSize")).getText().toString().equals("57"), "Keep unsaved cache input across recreation.");
            require(((Spinner) tagged("enableANGLE")).getSelectedItemPosition() == 2, "Keep unsaved ANGLE choice across recreation.");
            tagged("save").performClick();
        });
        waitFor(() -> store.loadOrCreate().getInt("maxGlslCacheSize") == 57);
        require(store.loadOrCreate().getInt("enableANGLE") == 2, "Recreated screen saves correctly.");
    }

    private Activity openActivity() {
        Intent intent = new Intent(Intent.ACTION_MAIN).setComponent(new ComponentName(
                "com.fcl.plugin.mobileglues.mc26test", "com.fcl.plugin.mobileglues.mc26test.MainActivity"));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return startActivitySync(intent);
    }

    private View tagged(String tag) {
        AtomicReference<View> found = new AtomicReference<>();
        if (android.os.Looper.myLooper() == android.os.Looper.getMainLooper())
            return activity.getWindow().getDecorView().findViewWithTag(tag);
        runOnMainSync(() -> found.set(activity.getWindow().getDecorView().findViewWithTag(tag)));
        return found.get();
    }
    private interface Condition { boolean ready() throws Exception; }
    private void waitFor(Condition condition) throws Exception {
        long limit = System.currentTimeMillis() + 5000;
        while (System.currentTimeMillis() < limit) {
            if (condition.ready()) return;
            Thread.sleep(40);
        }
        throw new AssertionError("Timed out waiting for settings state.");
    }
    private void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
        checks++;
    }
}
