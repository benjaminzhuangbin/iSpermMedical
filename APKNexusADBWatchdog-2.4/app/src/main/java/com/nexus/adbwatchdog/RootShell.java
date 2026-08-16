package com.nexus.adbwatchdog;

import android.util.Log;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Root shell for RK3288 / Android 5.1.1.
 * <p>
 * Device confirmed working from adb:
 * {@code adb shell "su -c id"} → uid=0
 * {@code adb shell "su -c setprop persist.adb.tcp.port 5555"}
 * <p>
 * App-side must:
 * <ul>
 *   <li>Locate real su binary ({@code /system/xbin/su}, {@code /system/bin/su}, …)</li>
 *   <li>Pass command as ONE {@code -c} argument (not as executable path)</li>
 *   <li>Also support stdin shell mode ({@code su} then write cmd + exit)</li>
 *   <li>Close stdin promptly; drain stdout/stderr; check uid=0 in either stream</li>
 * </ul>
 * Does not change Watchdog 2.4 recovery policy.
 */
public final class RootShell {

    public static final String TAG = "NexusRootShell";

    public static final class Result {
        public final int exitCode;
        public final String stdout;
        public final String stderr;
        public final boolean timedOut;
        public final String method; // e.g. SU_C:/system/xbin/su or SU_STDIN:/system/xbin/su

        public Result(int exitCode, String stdout, String stderr, boolean timedOut, String method) {
            this.exitCode = exitCode;
            this.stdout = stdout != null ? stdout : "";
            this.stderr = stderr != null ? stderr : "";
            this.timedOut = timedOut;
            this.method = method != null ? method : "";
        }

        public boolean ok() {
            return !timedOut && exitCode == 0;
        }

        public String combined() {
            if (stdout.isEmpty()) {
                return stderr;
            }
            if (stderr.isEmpty()) {
                return stdout;
            }
            return stdout + "\n" + stderr;
        }
    }

    /** Cached after first successful probe. */
    private static volatile Boolean sHasRoot;
    private static volatile String sSuPath = "su";
    private static volatile String sRootMethod = "NONE";
    private static volatile String sRootUid = "-1";
    private static volatile String sLastDiag = "";

    private static final String[] SU_CANDIDATES = new String[]{
            "/system/xbin/su",
            "/system/bin/su",
            "/sbin/su",
            "/su/bin/su",
            "/vendor/bin/su",
            "su"
    };

    private RootShell() {
    }

    public static String getRootMethod() {
        return sRootMethod;
    }

    public static String getRootUid() {
        return sRootUid;
    }

    public static String getLastDiag() {
        return sLastDiag;
    }

    public static String getSuPath() {
        return sSuPath;
    }

    /**
     * Probe root by actually running {@code id} via su.
     * Success = output contains {@code uid=0}.
     */
    public static synchronized boolean hasRoot() {
        if (sHasRoot != null && sHasRoot.booleanValue()) {
            return true;
        }
        Result r = probeRoot();
        boolean ok = outputLooksLikeRoot(r);
        sHasRoot = ok;
        if (ok) {
            sRootMethod = r.method.isEmpty() ? "SU" : r.method;
            sRootUid = "0";
            sLastDiag = "ok method=" + sRootMethod + " out=" + trimOneLine(r.combined());
            Log.i(TAG, "ROOT=YES " + sLastDiag);
        } else {
            sRootMethod = "NONE";
            sRootUid = "-1";
            sLastDiag = "fail exit=" + r.exitCode
                    + " timedOut=" + r.timedOut
                    + " method=" + r.method
                    + " out=" + trimOneLine(r.combined());
            Log.w(TAG, "ROOT=NO " + sLastDiag);
        }
        return ok;
    }

    /** Force re-probe (e.g. after user grants SuperSU). */
    public static synchronized void resetRootCache() {
        sHasRoot = null;
        sRootMethod = "NONE";
        sRootUid = "-1";
    }

    public static Result execSu(String command) {
        return execSu(command, 20);
    }

    /**
     * Execute {@code command} as root.
     * Example: {@code setprop persist.adb.tcp.port 5555} — whole string is ONE -c argument.
     */
    public static Result execSu(String command, int timeoutSec) {
        if (command == null || command.trim().isEmpty()) {
            return new Result(-1, "", "empty command", false, "NONE");
        }
        // Ensure we know a working su binary when possible
        if (sHasRoot == null) {
            hasRoot();
        }

        List<String> tried = new ArrayList<String>();

        // Prefer discovered path first
        String[] paths = buildSuSearchOrder();
        for (int i = 0; i < paths.length; i++) {
            String su = paths[i];
            Result r = execSuDashC(su, command, timeoutSec);
            tried.add("C:" + su + ":exit=" + r.exitCode + ":to=" + r.timedOut);
            if (isUsefulSuccess(r, command)) {
                sSuPath = su;
                return r;
            }
            Result r2 = execSuStdin(su, command, timeoutSec);
            tried.add("IN:" + su + ":exit=" + r2.exitCode + ":to=" + r2.timedOut);
            if (isUsefulSuccess(r2, command)) {
                sSuPath = su;
                return r2;
            }
        }

        Result last = new Result(-1, "", "all su methods failed: " + tried.toString(), false, "NONE");
        Log.w(TAG, "execSu failed cmd=[" + command + "] " + tried);
        return last;
    }

    private static Result probeRoot() {
        String[] paths = buildSuSearchOrder();
        Result last = new Result(-1, "", "no su binary", false, "NONE");
        for (int i = 0; i < paths.length; i++) {
            String su = paths[i];
            Result r = execSuDashC(su, "id", 8);
            last = r;
            if (outputLooksLikeRoot(r)) {
                sSuPath = su;
                return r;
            }
            Result r2 = execSuStdin(su, "id", 8);
            last = r2;
            if (outputLooksLikeRoot(r2)) {
                sSuPath = su;
                return r2;
            }
        }
        return last;
    }

    private static String[] buildSuSearchOrder() {
        List<String> list = new ArrayList<String>();
        if (sSuPath != null && sSuPath.length() > 0) {
            list.add(sSuPath);
        }
        for (int i = 0; i < SU_CANDIDATES.length; i++) {
            if (!list.contains(SU_CANDIDATES[i])) {
                list.add(SU_CANDIDATES[i]);
            }
        }
        return list.toArray(new String[list.size()]);
    }

    /**
     * {@code <su> -c <command>}
     * Critical: command with spaces must be a single argv element after -c.
     */
    private static Result execSuDashC(String suBin, String command, int timeoutSec) {
        Process process = null;
        String method = "SU_C:" + suBin;
        try {
            // Explicit argv — NOT Runtime.exec(String) which tokenizes badly.
            process = Runtime.getRuntime().exec(new String[]{suBin, "-c", command});
            // Close stdin immediately so su -c does not wait for more input.
            try {
                process.getOutputStream().close();
            } catch (Exception ignored) {
            }

            StreamGobbler outG = new StreamGobbler(process.getInputStream());
            StreamGobbler errG = new StreamGobbler(process.getErrorStream());
            outG.start();
            errG.start();

            boolean finished = waitFor(process, timeoutSec);
            if (!finished) {
                destroyProcess(process);
                outG.joinQuiet(500);
                errG.joinQuiet(500);
                return new Result(-1, outG.text(), errG.text(), true, method);
            }
            outG.joinQuiet(2000);
            errG.joinQuiet(2000);
            int code;
            try {
                code = process.exitValue();
            } catch (Exception e) {
                code = -1;
            }
            return new Result(code, outG.text().trim(), errG.text().trim(), false, method);
        } catch (Exception e) {
            return new Result(-1, "",
                    e.getMessage() != null ? e.getMessage() : "exec failed",
                    false, method);
        } finally {
            destroyProcess(process);
        }
    }

    /**
     * Interactive style used by many Android 5.x SuperSU builds:
     * start {@code su}, write command + exit to stdin.
     */
    private static Result execSuStdin(String suBin, String command, int timeoutSec) {
        Process process = null;
        String method = "SU_STDIN:" + suBin;
        try {
            process = Runtime.getRuntime().exec(new String[]{suBin});
            StreamGobbler outG = new StreamGobbler(process.getInputStream());
            StreamGobbler errG = new StreamGobbler(process.getErrorStream());
            outG.start();
            errG.start();

            DataOutputStream os = new DataOutputStream(process.getOutputStream());
            // Single shell line; keep setprop args intact.
            os.writeBytes(command);
            os.writeBytes("\n");
            os.writeBytes("exit\n");
            os.flush();
            try {
                os.close();
            } catch (Exception ignored) {
            }

            boolean finished = waitFor(process, timeoutSec);
            if (!finished) {
                destroyProcess(process);
                outG.joinQuiet(500);
                errG.joinQuiet(500);
                return new Result(-1, outG.text(), errG.text(), true, method);
            }
            outG.joinQuiet(2000);
            errG.joinQuiet(2000);
            int code;
            try {
                code = process.exitValue();
            } catch (Exception e) {
                code = -1;
            }
            return new Result(code, outG.text().trim(), errG.text().trim(), false, method);
        } catch (Exception e) {
            return new Result(-1, "",
                    e.getMessage() != null ? e.getMessage() : "exec failed",
                    false, method);
        } finally {
            destroyProcess(process);
        }
    }

    private static boolean outputLooksLikeRoot(Result r) {
        if (r == null) {
            return false;
        }
        String all = r.combined();
        // Accept uid=0 even if exit code non-zero (some su wrappers do that).
        return all.contains("uid=0");
    }

    private static boolean isUsefulSuccess(Result r, String command) {
        if (r == null || r.timedOut) {
            return false;
        }
        if ("id".equals(command.trim())) {
            return outputLooksLikeRoot(r);
        }
        // Prefer exit 0; also accept empty success for setprop/ctl.
        if (r.exitCode == 0) {
            return true;
        }
        // getprop may print value with exit 0 only — if non-zero, fail
        return false;
    }

    private static boolean waitFor(Process process, int timeoutSec) throws InterruptedException {
        long deadline = System.currentTimeMillis() + TimeUnit.SECONDS.toMillis(timeoutSec);
        while (System.currentTimeMillis() < deadline) {
            try {
                process.exitValue();
                return true;
            } catch (IllegalThreadStateException e) {
                Thread.sleep(40);
            }
        }
        return false;
    }

    private static void destroyProcess(Process process) {
        if (process == null) {
            return;
        }
        try {
            process.getInputStream().close();
        } catch (Exception ignored) {
        }
        try {
            process.getErrorStream().close();
        } catch (Exception ignored) {
        }
        try {
            process.getOutputStream().close();
        } catch (Exception ignored) {
        }
        try {
            process.destroy();
        } catch (Exception ignored) {
        }
    }

    private static String trimOneLine(String s) {
        if (s == null) {
            return "";
        }
        String t = s.replace('\n', ' ').replace('\r', ' ').trim();
        if (t.length() > 180) {
            return t.substring(0, 180) + "...";
        }
        return t;
    }

    private static final class StreamGobbler extends Thread {
        private final InputStream in;
        private final StringBuilder sb = new StringBuilder();

        StreamGobbler(InputStream in) {
            this.in = in;
            setDaemon(true);
        }

        @Override
        public void run() {
            try {
                BufferedReader br = new BufferedReader(new InputStreamReader(in));
                String line;
                while ((line = br.readLine()) != null) {
                    if (sb.length() > 0) {
                        sb.append('\n');
                    }
                    sb.append(line);
                }
            } catch (Exception ignored) {
            }
        }

        String text() {
            return sb.toString();
        }

        void joinQuiet(long ms) {
            try {
                join(ms);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}
