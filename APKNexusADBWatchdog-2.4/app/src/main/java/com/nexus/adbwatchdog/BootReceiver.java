package com.nexus.adbwatchdog;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Start Watchdog Service after boot (Android 5.1.1).
 */
public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }
        String action = intent.getAction();
        if (Intent.ACTION_BOOT_COMPLETED.equals(action)
                || "android.intent.action.QUICKBOOT_POWERON".equals(action)) {
            Log.i(NexusADBWatchdogService.TAG, "BOOT_COMPLETED -> start Watchdog Service");
            NexusADBWatchdogService.start(context.getApplicationContext());
        }
    }
}
