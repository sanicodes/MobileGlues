// SPDX-License-Identifier: LGPL-2.1-only
package com.fcl.plugin.mobileglues.mc26test;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.InputType;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import org.json.JSONObject;
import java.io.File;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Configures the renderer directly; no other MobileGlues APK is required. */
public final class MainActivity extends Activity {
    private static final int TEXT = Color.rgb(28, 43, 35);
    private static final int MUTED = Color.rgb(78, 98, 85);
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Map<String, Spinner> choices = new LinkedHashMap<>();
    private final Map<String, int[]> choiceValues = new LinkedHashMap<>();
    private final Map<String, CheckBox> toggles = new LinkedHashMap<>();
    private SettingsStore store;
    private LinearLayout content;
    private LinearLayout form;
    private TextView status;
    private TextView cacheInfo;
    private EditText cacheLimit;
    private JSONObject config;
    private Bundle draft;
    private boolean loaded;
    private boolean busy;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        draft = state;
        store = new SettingsStore(new File(Environment.getExternalStorageDirectory(), "MG"));
        ScrollView scroll = new ScrollView(this);
        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(20), dp(24), dp(20), dp(24));
        content.setOnApplyWindowInsetsListener((view, insets) -> {
            content.setPadding(dp(20) + insets.getSystemWindowInsetLeft(),
                    dp(24) + insets.getSystemWindowInsetTop(), dp(20) + insets.getSystemWindowInsetRight(),
                    dp(24) + insets.getSystemWindowInsetBottom());
            return insets;
        });
        scroll.addView(content);
        setContentView(scroll);
    }

    @Override public void onResume() {
        super.onResume();
        if (!hasStorageAccess()) {
            if (loaded) captureDraft();
            loaded = false;
            permissionScreen();
        } else if (!loaded && !busy) {
            load(false);
        }
    }

    private boolean hasStorageAccess() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestStorageAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } catch (ActivityNotFoundException error) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE}, 10);
        }
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (hasStorageAccess() && !busy && !loaded) load(false);
    }

    private void header() {
        content.removeAllViews();
        choices.clear(); choiceValues.clear(); toggles.clear();
        text(content, "SANICODES", 12, MUTED, true);
        text(content, "MobileGlues", 30, TEXT, true);
        String version = "";
        try { version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName; }
        catch (PackageManager.NameNotFoundException ignored) { }
        text(content, version, 14, MUTED, false);
        text(content, "Tap Save settings after making changes. Restart Minecraft to apply them.", 16, MUTED, false);
        status = text(content, "", 14, MUTED, false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
    }

    private void permissionScreen() {
        header();
        LinearLayout card = card(content);
        text(card, "Allow settings access", 20, TEXT, true);
        text(card, "MobileGlues saves settings and shader cache in shared storage so Zalith can read them. "
                + "Your existing settings will be kept.", 16, MUTED, false);
        button(card, "Allow access", view -> requestStorageAccess()).setTag("storageAccess");
    }

    private void load(boolean reset) {
        busy = true;
        header();
        status.setText("Loading settings…");
        io.execute(() -> {
            try {
                JSONObject value = reset ? store.reset() : store.loadOrCreate();
                long bytes = store.cacheBytes();
                runOnUiThread(() -> {
                    if (isDestroyed()) return;
                    busy = false;
                    if (!hasStorageAccess()) { permissionScreen(); return; }
                    if (!reset && draft != null && draft.containsKey("draft")) {
                        try { valueFromDraft(value); }
                        catch (Exception ignored) { config = value; }
                    } else config = value;
                    loaded = true;
                    settingsScreen(bytes);
                    if (!reset && draft != null && draft.containsKey("cacheText"))
                        cacheLimit.setText(draft.getString("cacheText"));
                    draft = null;
                    status.setText(reset ? "Defaults restored. Restart Minecraft to apply." : "Settings ready.");
                });
            } catch (Exception error) {
                runOnUiThread(() -> {
                    if (isDestroyed()) return;
                    busy = false; loaded = false;
                    header();
                    status.setText("Could not read settings: " + error.getMessage());
                    LinearLayout card = card(content);
                    text(card, "Your existing file has been kept.", 16, TEXT, false);
                    button(card, "Try again", view -> load(false));
                    button(card, "Restore defaults", view -> confirmReset());
                });
            }
        });
    }

    private void valueFromDraft(JSONObject fallback) throws Exception {
        config = new JSONObject(draft.getString("draft", fallback.toString()));
    }

    private void settingsScreen(long cacheBytes) {
        header();
        form = new LinearLayout(this);
        form.setOrientation(LinearLayout.VERTICAL);
        content.addView(form);
        LinearLayout renderer = card(form);
        text(renderer, "Rendering", 20, TEXT, true);
        choice(renderer, "ANGLE", "enableANGLE", new int[]{0, 1, 2, 3},
                new String[]{"Prefer native GLES", "Prefer ANGLE", "Force native GLES", "Force ANGLE"}, 0);
        text(renderer, "ANGLE is supplied by Zalith. No separate MobileGlues app is needed.", 14, MUTED, false);
        choice(renderer, "Error reporting", "enableNoError", new int[]{0, 1, 2, 3},
                new String[]{"Automatic", "Report all errors", "Ignore shader / program errors",
                        "Ignore shader / program / framebuffer errors"}, 0);

        LinearLayout cache = card(form);
        text(cache, "Shader cache", 20, TEXT, true);
        TextView cacheLabel = text(cache, "Cache limit in MiB · 0 disables caching", 15, TEXT, false);
        cacheLimit = new EditText(this);
        cacheLimit.setId(1001);
        cacheLabel.setLabelFor(cacheLimit.getId());
        cacheLimit.setTag("maxGlslCacheSize");
        cacheLimit.setInputType(InputType.TYPE_CLASS_NUMBER);
        cacheLimit.setSingleLine(true);
        cacheLimit.setText(String.valueOf(Math.max(0, SettingsStore.number(config, "maxGlslCacheSize", 0))));
        cache.addView(cacheLimit);
        cacheInfo = text(cache, cacheDescription(cacheBytes), 14, MUTED, false);
        button(cache, "Clear shader cache", view -> new AlertDialog.Builder(this)
                .setTitle("Clear shader cache?")
                .setMessage("Close Minecraft first. Shaders will be rebuilt on the next launch.")
                .setNegativeButton("Cancel", null).setPositiveButton("Clear", (dialog, which) -> clearCache()).show());

        LinearLayout advanced = card(form);
        LinearLayout advancedFields = new LinearLayout(this);
        advancedFields.setOrientation(LinearLayout.VERTICAL);
        Button expand = button(advanced, "Advanced settings ▾", view -> {
            boolean open = advancedFields.getVisibility() == View.VISIBLE;
            advancedFields.setVisibility(open ? View.GONE : View.VISIBLE);
            ((Button) view).setText(open ? "Advanced settings ▾" : "Advanced settings ▴");
        });
        expand.setTag("advanced");
        advanced.addView(advancedFields);
        choice(advancedFields, "OpenGL version", "customGLVersion", new int[]{0, 32, 33, 40, 41, 42, 43, 44, 45, 46},
                new String[]{"Default (4.0)", "3.2", "3.3", "4.0", "4.1", "4.2", "4.3", "4.4", "4.5", "4.6"}, 0);
        choice(advancedFields, "ANGLE depth-clear workaround", "angleDepthClearFixMode", new int[]{0, 1, 2},
                new String[]{"Disabled", "Mode 1", "Mode 2"}, 0);
        choice(advancedFields, "FSR 1 upscaling", "fsr1Setting", new int[]{0, 1, 2, 3, 4},
                new String[]{"Disabled", "Ultra quality", "Quality", "Balanced", "Performance"}, 0);
        toggle(advancedFields, "GPU timer queries", "enableExtTimerQuery");
        toggle(advancedFields, "Compute-shader extension", "enableExtComputeShader");
        toggle(advancedFields, "Direct-state-access extension", "enableExtDirectStateAccess");
        toggle(advancedFields, "Hide renderer identity", "hideMGEnvLevel");
        text(advancedFields, "Multi-draw preferences", 17, TEXT, true);
        for (int i = 0; i < OrderEditor.KEYS.length; ++i) {
            final int entry = i;
            Button order = button(advancedFields, OrderEditor.TITLES[i], view -> OrderEditor.show(this, config, entry,
                    () -> status.setText("Draw order changed. Tap Save settings to apply.")));
            order.setTag(OrderEditor.KEYS[i]);
        }
        advancedFields.setVisibility(View.GONE);

        Button save = button(form, "Save settings", view -> save(false));
        save.setTag("save");
        Button launch = button(form, "Save and open Zalith", view -> save(true));
        launch.setTag("launch");
        button(form, "Restore defaults", view -> confirmReset());
        text(form, "Settings and logs: /sdcard/MG\nSelect MobileGlues (Sanicodes, OpenGL 4.0) in Zalith.", 13, MUTED, false);
    }

    private void choice(LinearLayout parent, String title, String key, int[] values, String[] labels, int fallback) {
        TextView label = text(parent, title, 15, TEXT, false);
        Spinner spinner = new Spinner(this);
        spinner.setId(1100 + choices.size());
        spinner.setTag(key);
        label.setLabelFor(spinner.getId());
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, labels);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        int current = SettingsStore.number(config, key, fallback);
        if (key.equals("customGLVersion")) {
            if (current > 46) current = 46;
            else if (current > 33 && current < 40) current = 33;
            else if (current > 0 && current < 32) current = 32;
        }
        for (int i = 0; i < values.length; ++i) if (values[i] == current) spinner.setSelection(i);
        parent.addView(spinner, new LinearLayout.LayoutParams(-1, dp(52)));
        choices.put(key, spinner); choiceValues.put(key, values);
    }

    private void toggle(LinearLayout parent, String label, String key) {
        CheckBox control = new CheckBox(this);
        control.setText(label); control.setTextColor(TEXT); control.setMinHeight(dp(48));
        control.setTag(key);
        control.setChecked(SettingsStore.number(config, key, 0) > 0);
        parent.addView(control);
        toggles.put(key, control);
    }

    private JSONObject snapshot() throws Exception {
        JSONObject next = new JSONObject(config.toString());
        for (Map.Entry<String, Spinner> entry : choices.entrySet())
            next.put(entry.getKey(), choiceValues.get(entry.getKey())[entry.getValue().getSelectedItemPosition()]);
        for (Map.Entry<String, CheckBox> entry : toggles.entrySet())
            next.put(entry.getKey(), entry.getValue().isChecked() ? 1 : 0);
        return next;
    }

    private void save(boolean openLauncher) {
        if (busy || !loaded) return;
        final JSONObject next;
        try {
            next = snapshot();
            int limit = Integer.parseInt(cacheLimit.getText().toString().trim());
            if (limit < 0 || limit > 1024) throw new NumberFormatException();
            next.put("maxGlslCacheSize", limit);
        } catch (Exception error) {
            cacheLimit.setError("Enter a whole number from 0 to 1024.");
            cacheLimit.requestFocus();
            return;
        }
        busy = true; setEnabled(form, false); status.setText("Saving…");
        io.execute(() -> {
            try {
                store.save(next);
                runOnUiThread(() -> {
                    if (isDestroyed()) return;
                    config = next; busy = false; setEnabled(form, true);
                    status.setText("Saved. Restart Minecraft to apply.");
                    if (openLauncher) openZalith();
                });
            } catch (Exception error) { reportFailure(error); }
        });
    }

    private void clearCache() {
        if (busy) return;
        busy = true; setEnabled(form, false);
        io.execute(() -> {
            try {
                store.clearShaderCache();
                runOnUiThread(() -> {
                    if (isDestroyed()) return;
                    busy = false; setEnabled(form, true);
                    cacheInfo.setText(cacheDescription(0)); status.setText("Shader cache cleared.");
                });
            } catch (Exception error) { reportFailure(error); }
        });
    }

    private void reportFailure(Exception error) {
        runOnUiThread(() -> {
            if (isDestroyed()) return;
            busy = false;
            if (form != null) setEnabled(form, true);
            status.setText("Could not apply changes: " + error.getMessage());
        });
    }

    private void confirmReset() {
        new AlertDialog.Builder(this).setTitle("Restore defaults?")
                .setMessage("This replaces renderer settings. A copy is kept as config.before-reset.json.")
                .setNegativeButton("Cancel", null).setPositiveButton("Restore", (dialog, which) -> {
                    draft = null; loaded = false; load(true);
                }).show();
    }

    private void openZalith() {
        for (String name : new String[]{"com.movtery.zalithlauncher.v2", "com.movtery.zalithlauncher.v2.debug"}) {
            Intent intent = getPackageManager().getLaunchIntentForPackage(name);
            if (intent != null) { startActivity(intent); return; }
        }
        status.setText("Saved. Open your Minecraft launcher and select the Sanicodes renderer.");
    }

    private void captureDraft() {
        if (!loaded || config == null || cacheLimit == null) return;
        try {
            draft = new Bundle();
            draft.putString("draft", snapshot().toString());
            draft.putString("cacheText", cacheLimit.getText().toString());
        } catch (Exception ignored) { }
    }

    @Override public void onSaveInstanceState(Bundle state) {
        if (loaded) captureDraft();
        if (draft != null) state.putAll(draft);
        super.onSaveInstanceState(state);
    }

    @Override public void onDestroy() { io.shutdown(); super.onDestroy(); }

    private void setEnabled(View view, boolean enabled) {
        view.setEnabled(enabled);
        if (view instanceof android.view.ViewGroup) {
            android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) setEnabled(group.getChildAt(i), enabled);
        }
    }

    private LinearLayout card(LinearLayout parent) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(14), dp(16), dp(14));
        GradientDrawable background = new GradientDrawable();
        background.setColor(Color.WHITE); background.setCornerRadius(dp(18));
        card.setBackground(background);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.setMargins(0, dp(8), 0, dp(8));
        parent.addView(card, params);
        return card;
    }

    private TextView text(LinearLayout parent, String value, int size, int color, boolean bold) {
        TextView text = new TextView(this);
        text.setText(value); text.setTextSize(size); text.setTextColor(color);
        text.setPadding(0, dp(5), 0, dp(7));
        if (bold) text.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        parent.addView(text);
        return text;
    }

    private Button button(LinearLayout parent, String label, View.OnClickListener action) {
        Button button = new Button(this);
        button.setText(label); button.setAllCaps(false); button.setMinHeight(dp(48));
        button.setOnClickListener(action);
        parent.addView(button, new LinearLayout.LayoutParams(-1, -2));
        return button;
    }

    private String cacheDescription(long bytes) {
        return String.format(Locale.getDefault(), "Currently %.1f MiB on disk", bytes / 1048576.0);
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
