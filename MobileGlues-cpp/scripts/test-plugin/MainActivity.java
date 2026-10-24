// SPDX-License-Identifier: LGPL-2.1-only
package com.fcl.plugin.mobileglues.mc26test;

import android.app.Activity;
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
        message.setText("MobileGlues MC26.3 test\n\n"
                + "Restart Zalith Launcher, then select:\n\n"
                + "MobileGlues MC26.3 shader fix (test)\n\n"
                + "Configure ANGLE using the official MobileGlues app.");
        setContentView(message);
    }
}
