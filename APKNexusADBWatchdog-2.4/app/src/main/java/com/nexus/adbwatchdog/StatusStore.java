package com.nexus.adbwatchdog;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

/**
 * App-private status + log under {@code getFilesDir()} (not /data/local/watchdog).
 */
public final class StatusStore {

    public static final String STATUS_NAME = "watchdog.status";
    public static final String LOG_NAME = "watchdog.log";

    private StatusStore() {
    }

    public static File statusFile(Context ctx) {
        return new File(ctx.getFilesDir(), STATUS_NAME);
    }

    public static File logFile(Context ctx) {
        return new File(ctx.getFilesDir(), LOG_NAME);
    }

    public static void writeStatus(Context ctx, WatchdogStatus status) {
        writeText(statusFile(ctx), status.toStatusFile(), false);
    }

    public static synchronized void appendLog(Context ctx, String text) {
        File f = logFile(ctx);
        // Simple size cap ~2MB
        if (f.exists() && f.length() > 2L * 1024L * 1024L) {
            // Rotate by truncate (API 22 friendly)
            writeText(f, "", false);
        }
        writeText(f, text.endsWith("\n") ? text : text + "\n", true);
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

    private static void writeText(File file, String text, boolean append) {
        try {
            FileOutputStream fos = new FileOutputStream(file, append);
            OutputStreamWriter w = new OutputStreamWriter(fos, Charset.forName("UTF-8"));
            w.write(text);
            w.flush();
            w.close();
            fos.close();
        } catch (Exception ignored) {
        }
    }

    private static String readText(File file) {
        if (file == null || !file.exists()) {
            return "";
        }
        try {
            java.io.FileInputStream fis = new java.io.FileInputStream(file);
            java.io.InputStreamReader r = new java.io.InputStreamReader(fis, Charset.forName("UTF-8"));
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
            return "";
        }
    }
}
