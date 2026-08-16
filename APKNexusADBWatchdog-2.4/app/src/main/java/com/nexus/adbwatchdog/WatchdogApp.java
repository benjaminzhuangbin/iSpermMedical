package com.nexus.adbwatchdog;

import android.app.Application;
import android.util.Log;

/**
 * Starts Watchdog Service as soon as the app process is created.
 * Service is intended to run permanently (boot + sticky + no UI stop).
 */
public class WatchdogApp extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        StatusStore.ensurePublicFolder(this);
        Log.i(NexusADBWatchdogService.TAG, "Application onCreate -> auto START Service");
        NexusADBWatchdogService.start(this);
    }
}
