// SPDX-License-Identifier: LGPL-2.1-only
package com.fcl.plugin.mobileglues.mc26test;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Color;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.json.JSONException;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Edits backend preference orders without exposing raw configuration keys. */
final class OrderEditor {
    static final String[] KEYS = {"multidrawOrder", "multidrawOrderArrays", "multidrawOrderElements",
            "multidrawOrderElementsBaseVertex", "multidrawOrderArraysIndirect", "multidrawOrderElementsIndirect"};
    static final String[] TITLES = {"Global draw order", "Array draws", "Indexed draws",
            "Indexed draws with base vertex", "Indirect array draws", "Indirect indexed draws"};
    static final String[][] DEFAULTS = {
            {"native", "multiindirect", "multibasevertex", "multiarrays", "indirect", "basevertex", "unroll", "compute"},
            {"multiarrays", "multiindirect", "unroll"},
            {"multiarrays", "multiindirect", "multibasevertex", "indirect", "unroll"},
            {"multibasevertex", "multiindirect", "indirect", "basevertex", "unroll", "compute"},
            {"multiindirect", "indirect"}, {"multiindirect", "indirect"}
    };

    static void show(Activity activity, JSONObject config, int entry, Runnable changed) {
        List<String> allowed = Arrays.asList(DEFAULTS[entry]);
        ArrayList<String> order = new ArrayList<>();
        for (String raw : config.optString(KEYS[entry], "").split("[,;]")) {
            String item = raw.replaceAll("[\\s_-]", "").toLowerCase(java.util.Locale.ROOT);
            if (allowed.contains(item) && !order.contains(item)) order.add(item);
        }
        for (String item : allowed) if (!order.contains(item)) order.add(item);
        LinearLayout rows = new LinearLayout(activity);
        rows.setOrientation(LinearLayout.VERTICAL);
        final float density = activity.getResources().getDisplayMetrics().density;
        rows.setPadding(Math.round(16 * density), Math.round(8 * density), Math.round(16 * density), Math.round(8 * density));
        ScrollView scroll = new ScrollView(activity);
        scroll.addView(rows);
        Runnable[] render = new Runnable[1];
        render[0] = () -> {
            rows.removeAllViews();
            for (int i = 0; i < order.size(); ++i) {
                final int index = i;
                String item = order.get(i);
                LinearLayout row = new LinearLayout(activity);
                row.setGravity(android.view.Gravity.CENTER_VERTICAL);
                TextView label = new TextView(activity);
                label.setText(name(item));
                label.setTextColor(Color.rgb(28, 43, 35));
                label.setTextSize(16);
                row.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
                for (int direction : new int[]{-1, 1}) {
                    Button move = new Button(activity);
                    move.setText(direction < 0 ? "↑" : "↓");
                    move.setContentDescription("Move " + name(item) + (direction < 0 ? " up" : " down"));
                    move.setEnabled(index + direction >= 0 && index + direction < order.size());
                    move.setOnClickListener(view -> {
                        java.util.Collections.swap(order, index, index + direction);
                        render[0].run();
                    });
                    row.addView(move, new LinearLayout.LayoutParams(Math.round(56 * density), Math.round(48 * density)));
                }
                rows.addView(row);
            }
        };
        render[0].run();
        new AlertDialog.Builder(activity).setTitle(TITLES[entry])
                .setMessage("Try backends from top to bottom. Unsupported options are skipped.")
                .setView(scroll).setNegativeButton("Cancel", null)
                .setNeutralButton(entry == 0 ? "Use default" : "Use global order", (dialog, which) -> {
                    config.remove(KEYS[entry]);
                    changed.run();
                }).setPositiveButton("Apply", (dialog, which) -> {
                    try { config.put(KEYS[entry], String.join(",", order)); }
                    catch (JSONException error) { throw new IllegalStateException(error); }
                    changed.run();
                }).show();
    }

    private static String name(String key) {
        switch (key) {
            case "native": return "Native";
            case "multiindirect": return "Multi-indirect";
            case "multibasevertex": return "Multi-base-vertex";
            case "multiarrays": return "Multi-array";
            case "indirect": return "Indirect";
            case "basevertex": return "Base vertex";
            case "unroll": return "Unrolled draws";
            default: return "Compute";
        }
    }
}
