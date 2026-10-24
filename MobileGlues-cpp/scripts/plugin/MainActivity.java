// SPDX-License-Identifier: LGPL-2.1-only
package com.fcl.plugin.mobileglues.mc26test;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.TextView;

// Zalith discovers APK plugins through queryIntentActivities(ACTION_MAIN).
public final class MainActivity extends Activity {
    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        TextView message = new TextView(this);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        message.setPadding(padding, padding, padding, padding);
        message.setTextSize(18);
        String title = getApplicationInfo().loadLabel(getPackageManager()).toString();
        String renderer = title;
        try {
            ApplicationInfo info = getPackageManager().getApplicationInfo(
                    getPackageName(), PackageManager.GET_META_DATA);
            renderer = info.metaData.getString("des", title);
            title += "\n" + getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException ignored) {
        }
        message.setText(title + "\n\n"
                + "Restart Zalith Launcher, then select:\n\n"
                + renderer + "\n\n"
                + "Configure ANGLE using the official MobileGlues app.");
        setContentView(message);
    }
}
