package com.nexus.adbwatchdog;

import android.content.Context;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;

/**
 * Status + log storage.
 * <p>
 * Primary location (internal shared storage / user-visible folder):
 * {@code /sdcard/NexusADBWatchdog/}
 * <ul>
 *   <li>{@code /sdcard/NexusADBWatchdog/watchdog.status}</li>
 *   <li>{@code /sdcard/NexusADBWatchdog/watchdog.log}</li>
 * </ul>
 * Fallback: app private {@code getFilesDir()} if public dir is unavailable.
 */
public final class StatusStore {

    public static final String TAG = "NexusADBWatchdog";
    public static final String FOLDER_NAME = "NexusADBWatchdog";
    public static final String STATUS_NAME = "watchdog.status";
    public static final String LOG_NAME = "watchdog.log";

    /** Canonical path users should open on the device. */
    public static final String PUBLIC_DIR_PATH = "/sdcard/" + FOLDER_NAME;

    private static boolean dirReady;

    private StatusStore() {
    }

    public static String publicDirPath() {
        return PUBLIC_DIR_PATH;
    }

    public static File publicDir(Context ctx) {
        // Prefer classic /sdcard path (RK3288 / Android 5.1.1)
        File sd = new File(PUBLIC_DIR_PATH);
        if (ensureDir(sd)) {
            return sd;
        }
        // Fallback Environment.getExternalStorageDirectory()
        try {
            File ext = Environment.getExternalStorageDirectory();
            if (ext != null) {
                File alt = new File(ext, FOLDER_NAME);
                if (ensureDir(alt)) {
                    return alt;
                }
            }
        } catch (Exception ignored) {
        }
        // Last resort: app files
        return ctx.getFilesDir();
    }

    private static boolean ensureDir(File dir) {
        try {
            if (dir.exists() || dir.mkdirs()) {
                return dir.isDirectory() && dir.canWrite();
            }
        } catch (Exception ignored) {
        }
        // Root mkdir for industrial rooted RK3288
        RootShell.Result r = RootShell.execSu(
                "mkdir -p " + dir.getAbsolutePath()
                        + " && chmod 777 " + dir.getAbsolutePath(), 5);
        if (r.ok() || dir.isDirectory()) {
            try {
                return dir.isDirectory();
            } catch (Exception ignored) {
            }
        }
        return false;
    }

    public static void ensurePublicFolder(Context ctx) {
        File dir = publicDir(ctx);
        dirReady = dir != null && dir.isDirectory();
        Log.i(TAG, "Log folder: " + (dir != null ? dir.getAbsolutePath() : "null")
                + " ready=" + dirReady);
        if (dirReady) {
            // Marker file so users can find the folder easily
            File readme = new File(dir, "README.txt");
            if (!readme.exists()) {
                writeText(readme,
                        "Nexus ADB Watchdog 2.4 log folder\n"
                                + "watchdog.status = live status\n"
                                + "watchdog.log = event log\n",
                        false);
            }
        }
    }

    public static File statusFile(Context ctx) {
        return new File(publicDir(ctx), STATUS_NAME);
    }

    public static File logFile(Context ctx) {
        return new File(publicDir(ctx), LOG_NAME);
    }

    public static void writeStatus(Context ctx, WatchdogStatus status) {
        ensurePublicFolder(ctx);
        File f = statusFile(ctx);
        if (!writeText(f, status.toStatusFile(), false)) {
            // Root fallback write via temp in app files then cp
            writeViaRootCopy(ctx, STATUS_NAME, status.toStatusFile(), false);
        }
    }

    public static synchronized void appendLog(Context ctx, String text) {
        ensurePublicFolder(ctx);
        File f = logFile(ctx);
        String line = text.endsWith("\n") ? text : text + "\n";
        if (f.exists() && f.length() > 2L * 1024L * 1024L) {
            writeText(f, "", false);
        }
        if (!writeText(f, line, true)) {
            writeViaRootCopy(ctx, LOG_NAME, line, true);
        }
    }

    public static String readStatus(Context ctx) {
        return readText(statusFile(ctx));
    }

    public static String readLogTail(Context ctx, int maxLines) {
        String all = readText(logFile(ctx));
        if (all.isEmpty()) {
            return "";
        }
        String[] lines = all.split("\n");
        int start = Math.max(0, lines.length - maxLines);
        StringBuilder sb = new StringBuilder();
        for (int i = start; i < lines.length; i++) {
            if (sb.length() > 0) sb.append('\n');
            sb.append(lines[i]);
        }
        return sb.toString();
    }

    private static void writeViaRootCopy(Context ctx, String name, String text, boolean append) {
        try {
            File tmp = new File(ctx.getCacheDir(), "wd_tmp_" + name);
            writeText(tmp, text, false);
            String dest = PUBLIC_DIR_PATH + "/" + name;
            RootShell.execSu("mkdir -p " + PUBLIC_DIR_PATH + " && chmod 777 " + PUBLIC_DIR_PATH, 5);
            if (append) {
                RootShell.execSu("cat " + tmp.getAbsolutePath() + " >> " + dest
                        + " && chmod 666 " + dest, 8);
            } else {
                RootShell.execSu("cp " + tmp.getAbsolutePath() + " " + dest
                        + " && chmod 666 " + dest, 8);
            }
        } catch (Exception e) {
            Log.w(TAG, "root write failed", e);
        }
    }

    private static boolean writeText(File file, String text, boolean append) {
        try {
            File parent = file.getParentFile();
            if (parent != null && !parent.exists()) {
                //noinspection ResultOfMethodCallIgnored
                parent.mkdirs();
            }
            FileOutputStream fos = new FileOutputStream(file, append);
            OutputStreamWriter w = new OutputStreamWriter(fos, Charset.forName("UTF-8"));
            w.write(text);
            w.flush();
            w.close();
            fos.close();
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private static String readText(File file) {
        if (file == null || !file.exists()) {
            // Try root cat
            if (file != null) {
                RootShell.Result r = RootShell.execSu("cat " + file.getAbsolutePath(), 5);
                if (r.ok() && r.stdout != null) {
                    return r.stdout;
                }
            }
            return "";
        }
        try {
            FileInputStream fis = new FileInputStream(file);
            InputStreamReader r = new InputStreamReader(fis, Charset.forName("UTF-8"));
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[2048];
            int n;
            while ((n = r.read(buf)) > 0) {
                sb.append(buf, 0, n);
            }
            r.close();
            fis.close();
            return sb.toString();
        } catch (Exception e) {
            RootShell.Result rr = RootShell.execSu("cat " + file.getAbsolutePath(), 5);
            return rr.stdout != null ? rr.stdout : "";
        }
    }
}
