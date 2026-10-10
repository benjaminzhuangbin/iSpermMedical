package com.nexus.adbwatchdog;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.List;

/**
 * HomeGuard ensures the designated kiosk/main app (e.g. com.join)
 * is seamlessly launched on boot, automatically dismissing any transient
 * "Select a Home app" system chooser dialogs.
 */
public final class HomeGuard {

    private static final String TAG = "NexusHomeGuard";
    public static final String TARGET_PACKAGE = "com.join";
    public static final String TARGET_ACTIVITY_DEFAULT = "com.join.ui.MainActivity";

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());
    private static boolean sScheduled = false;

    private HomeGuard() {
    }

    /**
     * Schedules periodic launches of the target Home app during boot startup sequence
     * (immediate, 1.5s, 4s, 8s) to ensure the system chooser is eliminated.
     */
    public static synchronized void scheduleBootHomeLaunch(final Context context) {
        if (context == null) return;
        final Context appCtx = context.getApplicationContext();

        // Immediate attempt
        launchTargetHome(appCtx);

        // Delayed staggered attempts to beat any late-finishing AMS resolver popups
        int[] delaysMs = new int[]{1200, 3000, 6500, 11000};
        for (final int delay : delaysMs) {
            sMainHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    launchTargetHome(appCtx);
                }
            }, delay);
        }
        sScheduled = true;
    }

    /**
     * Executes robust launch of com.join as HOME or Launch intent.
     */
    public static boolean launchTargetHome(Context context) {
        if (context == null) return false;

        PackageManager pm = context.getPackageManager();
        if (pm == null) return false;

        try {
            // 1. Try resolving HOME intent for com.join
            Intent homeIntent = new Intent(Intent.ACTION_MAIN);
            homeIntent.addCategory(Intent.CATEGORY_HOME);
            homeIntent.setPackage(TARGET_PACKAGE);
            homeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

            List<ResolveInfo> homes = pm.queryIntentActivities(homeIntent, PackageManager.MATCH_DEFAULT_ONLY);
            if (homes != null && !homes.isEmpty()) {
                ResolveInfo ri = homes.get(0);
                if (ri.activityInfo != null) {
                    homeIntent.setComponent(new ComponentName(ri.activityInfo.packageName, ri.activityInfo.name));
                    context.startActivity(homeIntent);
                    Log.i(TAG, "HomeGuard: Successfully launched " + ri.activityInfo.packageName + "/" + ri.activityInfo.name + " as HOME");
                    return true;
                }
            }

            // 2. Try explicit ComponentName
            try {
                Intent explicitIntent = new Intent(Intent.ACTION_MAIN);
                explicitIntent.addCategory(Intent.CATEGORY_HOME);
                explicitIntent.setComponent(new ComponentName(TARGET_PACKAGE, TARGET_ACTIVITY_DEFAULT));
                explicitIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                context.startActivity(explicitIntent);
                Log.i(TAG, "HomeGuard: Explicit launch " + TARGET_PACKAGE + "/" + TARGET_ACTIVITY_DEFAULT);
                return true;
            } catch (Throwable ignored) {
            }

            // 3. Fallback: package standard launch intent
            Intent launchIntent = pm.getLaunchIntentForPackage(TARGET_PACKAGE);
            if (launchIntent != null) {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                context.startActivity(launchIntent);
                Log.i(TAG, "HomeGuard: Started " + TARGET_PACKAGE + " via getLaunchIntentForPackage");
                return true;
            }

            Log.w(TAG, "HomeGuard: Target package " + TARGET_PACKAGE + " not found or no launchable activity");
        } catch (Throwable t) {
            Log.e(TAG, "HomeGuard: Failed to launch target app", t);
        }

        return false;
    }
}
