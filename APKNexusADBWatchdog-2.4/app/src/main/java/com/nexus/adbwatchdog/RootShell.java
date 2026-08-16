package com.nexus.adbwatchdog;

import android.util.Log;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Root shell for RK3288 / Android 5.1.1 (API 22).
 * <p>
 * Field fact: {@code adb shell "su -c id"} works and returns {@code uid=0(root)}.
 * That means the working path is: <b>sh runs su</b>, not necessarily
 * {@code Runtime.exec(["/vendor/bin/su", ...])} from the app process.
 * <p>
 * Primary method (matches adb):
 * <pre>
 *   /system/bin/sh -c "su -c 'id'"
 *   /system/bin/sh -c "su -c 'setprop persist.adb.tcp.port 5555'"
 * </pre>
 * Direct {@code Runtime.exec([suBin, "-c", cmd])} is secondary only.
 * <p>
 * Does not change Watchdog 2.4 recovery policy.
 */
public final class RootShell {

    public static final String TAG = "NexusRootShell";

    public static final class Result {
        public final int exitCode;
        public final String stdout;
        public final String stderr;
        public final boolean timedOut;
        public final String method;

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
            if (stdout.length() == 0) {
                return stderr;
            }
            if (stderr.length() == 0) {
                return stdout;
            }
            return stdout + "\n" + stderr;
        }
    }

    private static volatile Boolean sHasRoot;
    private static volatile String sSuPath = "su"; // path or bare "su" for sh -c
    private static volatile String sRootMethod = "NONE";
    private static volatile String sRootUid = "-1";
    private static volatile String sLastDiag = "";
    private static volatile String sLastFailDetail = "";

    /** Shells that exist on Android 5.1.1. */
    private static final String[] SH_CANDIDATES = new String[]{
            "/system/bin/sh",
            "/system/xbin/sh",
            "sh"
    };

    /** Only used after existence check — never assume /vendor/bin/su. */
    private static final String[] SU_PATH_PROBE = new String[]{
            "/system/xbin/su",
            "/system/bin/su",
            "/sbin/su",
            "/su/bin/su"
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

    public static String getLastFailDetail() {
        return sLastFailDetail;
    }

    public static String getSuPath() {
        return sSuPath;
    }

    public static synchronized void resetRootCache() {
        sHasRoot = null;
        sRootMethod = "NONE";
        sRootUid = "-1";
    }

    /**
     * Probe by actually running {@code id} through the same style as adb shell.
     */
    public static synchronized boolean hasRoot() {
        if (sHasRoot != null && sHasRoot.booleanValue()) {
            return true;
        }

        List<String> attempts = new ArrayList<String>();
        Result bestFail = new Result(-1, "", "no attempt", false, "NONE");

        // 1) Exact adb equivalent: sh -c "su -c 'id'"
        for (int i = 0; i < SH_CANDIDATES.length; i++) {
            String sh = SH_CANDIDATES[i];
            if (!canExec(sh) && !("sh".equals(sh))) {
                attempts.add("skip-missing-sh:" + sh);
                continue;
            }
            Result r = execShSuC(sh, "su", "id", 10);
            attempts.add(formatAttempt(r));
            if (outputLooksLikeRoot(r)) {
                return markRootSuccess(r, "su");
            }
            bestFail = r;
        }

        // 2) Discover real su file via sh, then sh -c "<su> -c 'id'"
        String discovered = discoverSuViaSh(attempts);
        if (discovered != null) {
            for (int i = 0; i < SH_CANDIDATES.length; i++) {
                String sh = SH_CANDIDATES[i];
                if (!canExec(sh) && !("sh".equals(sh))) {
                    continue;
                }
                Result r = execShSuC(sh, discovered, "id", 10);
                attempts.add(formatAttempt(r));
                if (outputLooksLikeRoot(r)) {
                    return markRootSuccess(r, discovered);
                }
                bestFail = r;
            }
        }

        // 3) Direct Runtime.exec([su,"-c","id"]) only if file exists
        String[] existingSu = listExistingSuBins();
        for (int i = 0; i < existingSu.length; i++) {
            Result r = execDirectSuC(existingSu[i], "id", 8);
            attempts.add(formatAttempt(r));
            if (outputLooksLikeRoot(r)) {
                return markRootSuccess(r, existingSu[i]);
            }
            bestFail = r;

            Result r2 = execSuStdin(existingSu[i], "id", 8);
            attempts.add(formatAttempt(r2));
            if (outputLooksLikeRoot(r2)) {
                return markRootSuccess(r2, existingSu[i]);
            }
            bestFail = r2;
        }

        sHasRoot = false;
        sRootMethod = "NONE";
        sRootUid = "-1";
        sLastFailDetail = "attempts=" + attempts.toString()
                + " last=" + formatAttempt(bestFail)
                + " out=" + trimOneLine(bestFail.combined());
        sLastDiag = "fail " + sLastFailDetail;
        Log.w(TAG, "ROOT=NO " + sLastDiag);
        return false;
    }

    public static Result execSu(String command) {
        return execSu(command, 20);
    }

    /**
     * Run a root command. Example command string:
     * {@code setprop persist.adb.tcp.port 5555}
     * (entire string is passed to {@code su -c}, not split into executable path).
     */
    public static Result execSu(String command, int timeoutSec) {
        if (command == null || command.trim().length() == 0) {
            return new Result(-1, "", "empty command", false, "NONE");
        }
        if (sHasRoot == null) {
            hasRoot();
        }

        List<String> attempts = new ArrayList<String>();

        // Prefer the method that succeeded for id
        String su = (sSuPath != null && sSuPath.length() > 0) ? sSuPath : "su";

        for (int i = 0; i < SH_CANDIDATES.length; i++) {
            String sh = SH_CANDIDATES[i];
            if (!canExec(sh) && !("sh".equals(sh))) {
                continue;
            }
            Result r = execShSuC(sh, su, command, timeoutSec);
            attempts.add(formatAttempt(r));
            if (isCommandSuccess(r, command)) {
                return r;
            }
        }

        // Try discovered absolute su through sh
        String[] existing = listExistingSuBins();
        for (int e = 0; e < existing.length; e++) {
            for (int i = 0; i < SH_CANDIDATES.length; i++) {
                String sh = SH_CANDIDATES[i];
                if (!canExec(sh) && !("sh".equals(sh))) {
                    continue;
                }
                Result r = execShSuC(sh, existing[e], command, timeoutSec);
                attempts.add(formatAttempt(r));
                if (isCommandSuccess(r, command)) {
                    sSuPath = existing[e];
                    return r;
                }
            }
            Result d = execDirectSuC(existing[e], command, timeoutSec);
            attempts.add(formatAttempt(d));
            if (isCommandSuccess(d, command)) {
                sSuPath = existing[e];
                return d;
            }
        }

        Result fail = new Result(-1, "", "all methods failed: " + attempts.toString(), false, "NONE");
        sLastFailDetail = fail.stderr;
        Log.w(TAG, "execSu FAILED cmd=[" + command + "] " + attempts);
        return fail;
    }

    private static boolean markRootSuccess(Result r, String suPath) {
        sHasRoot = true;
        sSuPath = suPath;
        sRootMethod = r.method;
        sRootUid = "0";
        sLastDiag = "ok method=" + sRootMethod + " su=" + suPath
                + " out=" + trimOneLine(r.combined());
        sLastFailDetail = "";
        Log.i(TAG, "ROOT=YES " + sLastDiag);
        return true;
    }

    /**
     * {@code <sh> -c "<su> -c '<command>'"} — same shape as adb shell "su -c …".
     */
    private static Result execShSuC(String shBin, String suBin, String command, int timeoutSec) {
        // su -c 'setprop persist.adb.tcp.port 5555'
        String inner = suBin + " -c " + shellSingleQuote(command);
        String method = "SH_SU_C:" + shBin + "+" + suBin;
        return execArgv(new String[]{shBin, "-c", inner}, timeoutSec, method, true);
    }

    private static Result execDirectSuC(String suBin, String command, int timeoutSec) {
        String method = "SU_C:" + suBin;
        return execArgv(new String[]{suBin, "-c", command}, timeoutSec, method, true);
    }

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
            return new Result(safeExit(process), outG.text().trim(), errG.text().trim(), false, method);
        } catch (Exception e) {
            return new Result(-1, "", exMsg(e), false, method);
        } finally {
            destroyProcess(process);
        }
    }

    private static Result execArgv(String[] argv, int timeoutSec, String method, boolean closeStdin) {
        Process process = null;
        try {
            process = Runtime.getRuntime().exec(argv);
            if (closeStdin) {
                try {
                    process.getOutputStream().close();
                } catch (Exception ignored) {
                }
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
            return new Result(safeExit(process), outG.text().trim(), errG.text().trim(), false, method);
        } catch (Exception e) {
            return new Result(-1, "", exMsg(e), false, method);
        } finally {
            destroyProcess(process);
        }
    }

    private static String discoverSuViaSh(List<String> attempts) {
        // Prefer listing known locations; do not invent /vendor/bin/su.
        StringBuilder script = new StringBuilder();
        script.append("for f in");
        for (int i = 0; i < SU_PATH_PROBE.length; i++) {
            script.append(' ').append(SU_PATH_PROBE[i]);
        }
        script.append("; do if [ -x \"$f\" ] || [ -e \"$f\" ]; then echo FOUND:$f; fi; done; ");
        script.append("command -v su 2>/dev/null; which su 2>/dev/null; type su 2>/dev/null");

        for (int i = 0; i < SH_CANDIDATES.length; i++) {
            String sh = SH_CANDIDATES[i];
            Result r = execArgv(new String[]{sh, "-c", script.toString()}, 8,
                    "DISCOVER:" + sh, true);
            attempts.add(formatAttempt(r));
            String all = r.combined();
            // FOUND:/system/xbin/su
            String[] lines = all.split("\n");
            for (int L = 0; L < lines.length; L++) {
                String line = lines[L].trim();
                if (line.startsWith("FOUND:")) {
                    return line.substring(6).trim();
                }
                if (line.startsWith("/") && line.contains("su") && !line.contains(" ")) {
                    return line;
                }
            }
        }
        // Fall back to bare "su" (works under sh PATH, as with adb)
        return "su";
    }

    private static String[] listExistingSuBins() {
        List<String> list = new ArrayList<String>();
        for (int i = 0; i < SU_PATH_PROBE.length; i++) {
            if (canExec(SU_PATH_PROBE[i])) {
                list.add(SU_PATH_PROBE[i]);
            }
        }
        return list.toArray(new String[list.size()]);
    }

    private static boolean canExec(String path) {
        if (path == null || path.length() == 0 || "sh".equals(path) || "su".equals(path)) {
            return true; // let Runtime try PATH names
        }
        try {
            File f = new File(path);
            return f.exists();
        } catch (Exception e) {
            return false;
        }
    }

    /** POSIX single-quote escape for embedding in sh -c. */
    private static String shellSingleQuote(String s) {
        if (s == null) {
            return "''";
        }
        return "'" + s.replace("'", "'\\''") + "'";
    }

    private static boolean outputLooksLikeRoot(Result r) {
        if (r == null) {
            return false;
        }
        String all = r.combined();
        return all.contains("uid=0(root)") || all.contains("uid=0");
    }

    private static boolean isCommandSuccess(Result r, String command) {
        if (r == null || r.timedOut) {
            return false;
        }
        if ("id".equals(command.trim())) {
            return outputLooksLikeRoot(r);
        }
        return r.exitCode == 0;
    }

    private static String formatAttempt(Result r) {
        if (r == null) {
            return "null";
        }
        return r.method + "{exit=" + r.exitCode + ",to=" + r.timedOut
                + ",err=" + trimOneLine(r.stderr) + "}";
    }

    private static int safeExit(Process p) {
        try {
            return p.exitValue();
        } catch (Exception e) {
            return -1;
        }
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

    private static String exMsg(Exception e) {
        return e.getClass().getSimpleName() + ": " + (e.getMessage() != null ? e.getMessage() : "");
    }

    private static String trimOneLine(String s) {
        if (s == null) {
            return "";
        }
        String t = s.replace('\n', ' ').replace('\r', ' ').trim();
        if (t.length() > 220) {
            return t.substring(0, 220) + "...";
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
