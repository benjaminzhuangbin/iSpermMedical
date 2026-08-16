package com.nexus.adbwatchdog;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.concurrent.TimeUnit;

/**
 * Root shell helper for RK3288 Android 5.1.1.
 * Watchdog must use su for getprop/setprop/ctl.start/ctl.stop and /proc access.
 */
public final class RootShell {

    public static final class Result {
        public final int exitCode;
        public final String stdout;
        public final String stderr;
        public final boolean timedOut;

        public Result(int exitCode, String stdout, String stderr, boolean timedOut) {
            this.exitCode = exitCode;
            this.stdout = stdout != null ? stdout : "";
            this.stderr = stderr != null ? stderr : "";
            this.timedOut = timedOut;
        }

        public boolean ok() {
            return !timedOut && exitCode == 0;
        }
    }

    private RootShell() {
    }

    /** Non-root probe: try {@code su -c id}. */
    public static boolean hasRoot() {
        Result r = execSu("id", 5);
        return r.ok() && r.stdout.contains("uid=0");
    }

    public static Result execSu(String command) {
        return execSu(command, 15);
    }

    public static Result execSu(String command, int timeoutSec) {
        Process process = null;
        try {
            process = Runtime.getRuntime().exec(new String[]{"su", "-c", command});
            final Process p = process;
            final StringBuilder out = new StringBuilder();
            final StringBuilder err = new StringBuilder();

            Thread tOut = new Thread(new Runnable() {
                @Override
                public void run() {
                    readFully(p.getInputStream(), out);
                }
            });
            Thread tErr = new Thread(new Runnable() {
                @Override
                public void run() {
                    readFully(p.getErrorStream(), err);
                }
            });
            tOut.start();
            tErr.start();

            boolean finished = waitFor(process, timeoutSec);
            if (!finished) {
                process.destroy();
                return new Result(-1, out.toString(), err.toString(), true);
            }
            tOut.join(1000);
            tErr.join(1000);
            return new Result(process.exitValue(), out.toString().trim(), err.toString().trim(), false);
        } catch (Exception e) {
            return new Result(-1, "", e.getMessage() != null ? e.getMessage() : "exec failed", false);
        } finally {
            if (process != null) {
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
            }
        }
    }

    /** API 22-safe wait (Process.waitFor(timeout) is API 26+). */
    private static boolean waitFor(Process process, int timeoutSec) throws InterruptedException {
        long deadline = System.currentTimeMillis() + TimeUnit.SECONDS.toMillis(timeoutSec);
        while (System.currentTimeMillis() < deadline) {
            try {
                process.exitValue();
                return true;
            } catch (IllegalThreadStateException e) {
                Thread.sleep(50);
            }
        }
        return false;
    }

    private static void readFully(java.io.InputStream in, StringBuilder sb) {
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
}
