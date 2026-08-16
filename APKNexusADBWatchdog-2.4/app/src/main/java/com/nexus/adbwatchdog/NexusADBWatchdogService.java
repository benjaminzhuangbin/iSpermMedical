package com.nexus.adbwatchdog;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

/**
 * Long-running watchdog Service for Android 5.1.1.
 * Main APP should start this Service explicitly.
 */
public class NexusADBWatchdogService extends Service {

    public static final String TAG = "NexusADBWatchdog";

    private static final int NOTIF_ID = 2401;

    private HandlerThread workerThread;
    private Handler workerHandler;
    private WatchdogEngine engine;
    private WatchdogConfig config;
    private PowerManager.WakeLock wakeLock;
    private boolean running;

    private final Runnable tickRunnable = new Runnable() {
        @Override
        public void run() {
            if (!running) {
                return;
            }
            try {
                if (engine != null) {
                    engine.runOnce();
                }
            } catch (Throwable t) {
                Log.e(TAG, "watchdog tick failed", t);
            }
            if (running && workerHandler != null) {
                workerHandler.postDelayed(this, Math.max(1, config.intervalSec) * 1000L);
            }
        }
    };

    public static void start(Context ctx) {
        Intent i = new Intent(ctx, NexusADBWatchdogService.class);
        i.setAction(WatchdogConfig.ACTION_START);
        ctx.startService(i);
    }

    public static void stop(Context ctx) {
        Intent i = new Intent(ctx, NexusADBWatchdogService.class);
        i.setAction(WatchdogConfig.ACTION_STOP);
        ctx.startService(i);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        config = new WatchdogConfig();
        engine = new WatchdogEngine(this, config);
        workerThread = new HandlerThread("nexus-adb-watchdog");
        workerThread.start();
        workerHandler = new Handler(workerThread.getLooper());

        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm != null) {
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nexusadbwatchdog:svc");
            wakeLock.setReferenceCounted(false);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent != null ? intent.getAction() : WatchdogConfig.ACTION_START;

        /*
         * Product mode: permanent run. ACTION_STOP is ignored so the Service
         * is not terminated from the UI / casual intents. Use force-stop app
         * only for development if absolutely required.
         */
        if (WatchdogConfig.ACTION_STOP.equals(action)) {
            Log.w(TAG, "STOP ignored — Watchdog runs permanently");
            startWatchdog();
            return START_STICKY;
        }

        if (WatchdogConfig.ACTION_INJECT.equals(action) && intent != null && engine != null) {
            String cmd = intent.getStringExtra(WatchdogConfig.EXTRA_INJECT);
            if (cmd != null) {
                engine.requestInject(cmd);
            }
        }

        startWatchdog();
        // Sticky: restart after kill on Android 5.1.1
        return START_STICKY;
    }

    private void startWatchdog() {
        if (running) {
            return;
        }
        running = true;
        if (wakeLock != null && !wakeLock.isHeld()) {
            wakeLock.acquire();
        }
        startForegroundCompat();
        workerHandler.removeCallbacks(tickRunnable);
        workerHandler.post(tickRunnable);
        Log.i(TAG, "Watchdog Service started");
    }

    private void stopWatchdog() {
        running = false;
        if (workerHandler != null) {
            workerHandler.removeCallbacks(tickRunnable);
        }
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        stopForeground(true);
        Log.i(TAG, "Watchdog Service stopped");
    }

    private void startForegroundCompat() {
        Intent open = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open, 0);
        Notification.Builder b = new Notification.Builder(this)
                .setContentTitle("Nexus ADB Watchdog 2.4")
                .setContentText(getString(R.string.service_running))
                .setSmallIcon(android.R.drawable.ic_lock_idle_lock)
                .setContentIntent(pi)
                .setOngoing(true);
        // Notification.Builder(Context) is valid on API 22; channel APIs are API 26+.
        startForeground(NOTIF_ID, b.getNotification());
    }

    @Override
    public void onDestroy() {
        stopWatchdog();
        if (workerThread != null) {
            if (Build.VERSION.SDK_INT >= 18) {
                workerThread.quitSafely();
            } else {
                workerThread.quit();
            }
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
