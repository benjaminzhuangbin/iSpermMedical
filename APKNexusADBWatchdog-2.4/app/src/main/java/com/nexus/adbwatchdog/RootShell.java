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
 * Root execution for RK3288 / Android 5.1.1.
 * <p>
 * <b>Root cause of prior failure:</b> stock {@code /system/xbin/su} is setuid
 * but enforces a caller-UID allowlist. Shell (uid 2000) is allowed;
 * app uid (e.g. 10053) is rejected with {@code su: uid N not allowed to su}.
 * Wrapping via {@code sh -c "su -c ..."} does <em>not</em> bypass that check.
 * <p>
 * <b>Product solution:</b> factory-install a dedicated setuid helper
 * {@code /system/xbin/nexus_su} (shipped in this APK's assets + factory/).
 * That binary does not implement an app-UID deny list; kernel setuid elevates
 * any caller, then the helper runs {@code sh -c <cmd>} as uid 0.
 * <p>
 * Does not change Watchdog 2.4 recovery policy.
 */
public final class RootShell {

    public static final String TAG = "NexusRootShell";

    /** Preferred factory-installed setuid helper (no UID allowlist). */
    public static final String[] NEXUS_SU_PATHS = new String[]{
            "/system/xbin/nexus_su",
            "/system/bin/nexus_su",
            "/sbin/nexus_su",
            "/data/local/tmp/nexus_su"
    };

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
    private static volatile String sSuPath = "";
    private static volatile String sRootMethod = "NONE";
    private static volatile String sRootUid = "-1";
    private static volatile String sLastDiag = "";
    private static volatile String sLastFailDetail = "";

    private static final String[] SH_CANDIDATES = new String[]{
            "/system/bin/sh",
            "/system/xbin/sh",
            "sh"
    };

    private static final String[] STOCK_SU_PROBE = new String[]{
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
     * Prove APK execution path can obtain uid=0(root).
     * Prefer factory {@code nexus_su}; stock {@code su} is probed only to
     * document the UID deny (expected fail for 3rd-party app UID).
     */
    public static synchronized boolean hasRoot() {
        if (sHasRoot != null && sHasRoot.booleanValue()) {
            return true;
        }

        List<String> attempts = new ArrayList<String>();
        Result bestFail = new Result(-1, "", "no attempt", false, "NONE");

        // 0) Already root? (system / sharedUserId edge cases)
        Result self = execArgv(new String[]{"/system/bin/sh", "-c", "id"}, 5, "SELF_ID", true);
        attempts.add(formatAttempt(self));
        if (outputLooksLikeRoot(self)) {
            return markRootSuccess(self, "SELF", "ALREADY_UID0");
        }

        // 1) Factory setuid helper — this is the supported product path
        for (int i = 0; i < NEXUS_SU_PATHS.length; i++) {
            String path = NEXUS_SU_PATHS[i];
            if (!fileExists(path)) {
                attempts.add("missing:" + path);
                continue;
            }
            Result r = execDirectSuC(path, "id", 8);
            attempts.add(formatAttempt(r));
            if (outputLooksLikeRoot(r)) {
                return markRootSuccess(r, path, "NEXUS_SU:" + path);
            }
            // Also try via sh in case of odd exec wrappers
            Result r2 = execShSuC("/system/bin/sh", path, "id", 8);
            attempts.add(formatAttempt(r2));
            if (outputLooksLikeRoot(r2)) {
                return markRootSuccess(r2, path, "SH_NEXUS_SU:" + path);
            }
            bestFail = r2.timedOut ? r2 : r;
        }

        // 2) Stock su — expected to fail with "uid N not allowed to su" for app UIDs
        String[] stock = listExisting(STOCK_SU_PROBE);
        for (int i = 0; i < stock.length; i++) {
            Result r = execDirectSuC(stock[i], "id", 8);
            attempts.add(formatAttempt(r));
            if (outputLooksLikeRoot(r)) {
                return markRootSuccess(r, stock[i], "STOCK_SU:" + stock[i]);
            }
            Result r2 = execShSuC("/system/bin/sh", stock[i], "id", 8);
            attempts.add(formatAttempt(r2));
            if (outputLooksLikeRoot(r2)) {
                return markRootSuccess(r2, stock[i], "SH_STOCK_SU:" + stock[i]);
            }
            bestFail = r2;
            if (isUidNotAllowed(r) || isUidNotAllowed(r2)) {
                attempts.add("STOCK_SU_UID_DENIED:" + stock[i]
                        + " (expected on this firmware for app UID — use factory nexus_su)");
            }
        }

        // 3) Bare "su" via PATH (last resort; same UID deny)
        Result bare = execShSuC("/system/bin/sh", "su", "id", 8);
        attempts.add(formatAttempt(bare));
        if (outputLooksLikeRoot(bare)) {
            return markRootSuccess(bare, "su", "SH_SU_C:su");
        }
        bestFail = bare;

        sHasRoot = false;
        sRootMethod = "NONE";
        sRootUid = "-1";
        sSuPath = "";
        sLastFailDetail = "NEED_FACTORY_NEXUS_SU; stock su denies app UID; attempts="
                + attempts.toString()
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
     * Run a command as root via the helper that succeeded for {@code id}.
     */
    public static Result execSu(String command, int timeoutSec) {
        if (command == null || command.trim().length() == 0) {
            return new Result(-1, "", "empty command", false, "NONE");
        }
        if (sHasRoot == null) {
            hasRoot();
        }
        if (sHasRoot == null || !sHasRoot.booleanValue()) {
            return new Result(-1, "", "NO_ROOT: " + sLastFailDetail, false, "NONE");
        }

        // Already uid 0 in this process — run directly
        if ("ALREADY_UID0".equals(sRootMethod) || sRootMethod.startsWith("ALREADY_")) {
            return execArgv(new String[]{"/system/bin/sh", "-c", command}, timeoutSec,
                    "SELF_SH", true);
        }

        String helper = (sSuPath != null && sSuPath.length() > 0) ? sSuPath : null;
        if (helper != null && fileExists(helper)) {
            Result r = execDirectSuC(helper, command, timeoutSec);
            if (isCommandSuccess(r, command)) {
                return r;
            }
            Result r2 = execShSuC("/system/bin/sh", helper, command, timeoutSec);
            if (isCommandSuccess(r2, command)) {
                return r2;
            }
        }

        // Re-probe helpers if cached path vanished after OTA
        for (int i = 0; i < NEXUS_SU_PATHS.length; i++) {
            if (!fileExists(NEXUS_SU_PATHS[i])) {
                continue;
            }
            Result r = execDirectSuC(NEXUS_SU_PATHS[i], command, timeoutSec);
            if (isCommandSuccess(r, command)) {
                sSuPath = NEXUS_SU_PATHS[i];
                return r;
            }
        }

        return new Result(-1, "", "root helper exec failed for: " + command, false, "NONE");
    }

    private static boolean markRootSuccess(Result r, String suPath, String method) {
        sHasRoot = true;
        sSuPath = suPath;
        sRootMethod = method != null ? method : r.method;
        sRootUid = "0";
        sLastDiag = "ok method=" + sRootMethod + " helper=" + suPath
                + " out=" + trimOneLine(r.combined());
        sLastFailDetail = "";
        Log.i(TAG, "ROOT=YES " + sLastDiag);
        return true;
    }

    private static boolean isUidNotAllowed(Result r) {
        if (r == null) {
            return false;
        }
        String all = r.combined().toLowerCase();
        return all.contains("not allowed to su")
                || (all.contains("uid ") && all.contains("not allowed"));
    }

    private static Result execShSuC(String shBin, String suBin, String command, int timeoutSec) {
        String inner = suBin + " -c " + shellSingleQuote(command);
        String method = "SH_SU_C:" + shBin + "+" + suBin;
        return execArgv(new String[]{shBin, "-c", inner}, timeoutSec, method, true);
    }

    private static Result execDirectSuC(String suBin, String command, int timeoutSec) {
        String method = "SU_C:" + suBin;
        return execArgv(new String[]{suBin, "-c", command}, timeoutSec, method, true);
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

    private static String[] listExisting(String[] paths) {
        List<String> list = new ArrayList<String>();
        for (int i = 0; i < paths.length; i++) {
            if (fileExists(paths[i])) {
                list.add(paths[i]);
            }
        }
        return list.toArray(new String[list.size()]);
    }

    private static boolean fileExists(String path) {
        if (path == null || path.length() == 0) {
            return false;
        }
        try {
            return new File(path).exists();
        } catch (Exception e) {
            return false;
        }
    }

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
                + ",err=" + trimOneLine(r.stderr) + ",out=" + trimOneLine(r.stdout) + "}";
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
