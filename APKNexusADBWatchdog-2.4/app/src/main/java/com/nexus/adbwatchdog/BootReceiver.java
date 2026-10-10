package com.nexus.adbwatchdog;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.util.Log;

/**
 * Start Watchdog Service after boot and network link state changes (Android 5.1.1).
 */
public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }
        String action = intent.getAction();
        Log.i(NexusADBWatchdogService.TAG, "Trigger intent received: " + action);
        
        if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            HomeGuard.scheduleBootHomeLaunch(context);
        }
        
        NexusADBWatchdogService.start(context.getApplicationContext());
    }
}

