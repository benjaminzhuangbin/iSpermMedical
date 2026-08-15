package com.nexus.adbwatchdog;

import android.content.Context;
import android.text.TextUtils;

/**
 * One monitoring cycle — mirrors native Watchdog 2.4 classification,
 * without CNXN probe and without CLOSE_WAIT recovery.
 */
public final class WatchdogEngine {

    private final Context appContext;
    private final WatchdogConfig cfg;
    private final RecoveryEngine recovery;
    private final WatchdogLogger logger;
    private final long startMs;
    private int totalConnections;
    private int prevEstablished = -1;
    private boolean rootCached;
    private boolean rootChecked;

    public WatchdogEngine(Context ctx, WatchdogConfig cfg) {
        this.appContext = ctx.getApplicationContext();
        this.cfg = cfg;
        this.recovery = new RecoveryEngine(cfg);
        this.logger = new WatchdogLogger(appContext, cfg);
        this.startMs = System.currentTimeMillis();
    }

    public WatchdogLogger getLogger() {
        return logger;
    }

    public WatchdogStatus runOnce() {
        WatchdogStatus st = new WatchdogStatus();
        st.stampNow();
        st.uptime = Math.max(0L, (System.currentTimeMillis() - startMs) / 1000L);
        st.restartWindow = cfg.restartWindowSec;
        st.recoveryEnabled = cfg.recoveryEnable ? 1 : 0;

        if (!rootChecked) {
            rootCached = RootShell.hasRoot();
            rootChecked = true;
            logger.line("======= Nexus ADB Watchdog 2.4 APK started =======");
            logger.line("ROOT=" + (rootCached ? "YES" : "NO"));
        }
        st.rootOk = rootCached;
        if (!st.rootOk) {
            // Re-probe occasionally
            st.rootOk = RootShell.hasRoot();
            rootCached = st.rootOk;
        }

        if (!st.rootOk) {
            st.adbd = "NO";
            st.port5555 = "NO";
            st.tcpHealth = "FAULT";
            st.adbHealth = "FAULT";
            st.failReason = "NO_ROOT";
            st.lastAction = "NONE";
            st.clientState = "NO_CLIENT";
            StatusStore.writeStatus(appContext, st);
            logger.maybeStatus(st, false);
            return st;
        }

        // Optional one-shot inject from MainActivity / test
        processPendingInject();

        int adbdPid = findAdbdPid();
        st.adbdPid = adbdPid;
        boolean adbdOk = adbdPid > 0;
        st.adbd = adbdOk ? "YES" : "NO";

        TcpNetProbe.Stats tcp = TcpNetProbe.collect(WatchdogConfig.ADB_PORT);
        boolean portOk = tcp.listening;
        st.port5555 = portOk ? "YES" : "NO";
        st.established = tcp.established;
        st.closeWait = tcp.closeWait;
        st.timeWait = tcp.timeWait;
        st.synRecv = tcp.synRecv;
        st.clients = TcpNetProbe.joinClients(tcp.clientIps);
        if (!tcp.clientIps.isEmpty()) {
            st.client = tcp.clientIps.get(0);
        } else {
            st.client = "";
        }
        st.clientState = st.established > 0 ? "CONNECTED" : "NO_CLIENT";

        if (prevEstablished >= 0 && st.established > prevEstablished) {
            totalConnections += (st.established - prevEstablished);
        } else if (prevEstablished < 0) {
            totalConnections += st.established;
        }
        prevEstablished = st.established;
        st.totalConnections = totalConnections;

        st.propTcp = getProp("persist.adb.tcp.port");
        boolean propOk = WatchdogConfig.ADB_PORT_STR.equals(st.propTcp.trim());
        // Also accept service.adb.tcp.port if persist empty on some builds
        if (!propOk) {
            String svc = getProp("service.adb.tcp.port");
            if (WatchdogConfig.ADB_PORT_STR.equals(svc.trim())) {
                propOk = true;
                if (TextUtils.isEmpty(st.propTcp)) {
                    st.propTcp = svc;
                }
            }
        }
        st.propOk = propOk ? 1 : 0;

        classify(st, adbdOk, portOk, propOk);

        boolean acted = recovery.evaluate(adbdOk, portOk, propOk, st.established, logger);
        st.lastAction = recovery.getLastAction();
        st.restartCount = recovery.getRestartCount();
        st.recoveryCooldown = recovery.isInCooldown() ? 1 : 0;
        st.cooldown = recovery.cooldownRemainingSec();

        // Re-classify after recovery so status reflects post-action reality next cycle;
        // this cycle keeps pre-action FAIL_REASON for clarity unless success cleared.
        if (acted) {
            // Refresh port/adbd quickly for status file
            int pid2 = findAdbdPid();
            TcpNetProbe.Stats tcp2 = TcpNetProbe.collect(WatchdogConfig.ADB_PORT);
            st.adbdPid = pid2 > 0 ? pid2 : st.adbdPid;
            st.adbd = pid2 > 0 ? "YES" : st.adbd;
            st.port5555 = tcp2.listening ? "YES" : st.port5555;
        }

        StatusStore.writeStatus(appContext, st);
        logger.maybeStatus(st, acted);
        return st;
    }

    private void classify(WatchdogStatus st, boolean adbdOk, boolean portOk, boolean propOk) {
        if (!adbdOk) {
            st.tcpHealth = "FAULT";
            st.adbHealth = "FAULT";
            st.failReason = "ADBD_NOT_RUNNING";
            return;
        }
        if (!portOk) {
            st.tcpHealth = "FAULT";
            st.adbHealth = "FAULT";
            st.failReason = "PORT_NOT_LISTENING";
            return;
        }
        if (!propOk) {
            st.tcpHealth = "FAULT";
            st.adbHealth = "OK";
            st.failReason = "TCP_PORT_PROP";
            return;
        }
        // CLOSE_WAIT / TIME_WAIT / SYN_RECV are reported but NEVER recovery triggers.
        st.tcpHealth = "OK";
        st.adbHealth = "OK";
        st.failReason = "NONE";
    }

    private static int findAdbdPid() {
        RootShell.Result r = RootShell.execSu("pidof adbd", 5);
        if (r.ok() && !TextUtils.isEmpty(r.stdout)) {
            String first = r.stdout.trim().split("\\s+")[0];
            try {
                return Integer.parseInt(first);
            } catch (NumberFormatException ignored) {
            }
        }
        // Fallback: ps parse
        RootShell.Result ps = RootShell.execSu("ps", 8);
        if (!ps.stdout.isEmpty()) {
            String[] lines = ps.stdout.split("\n");
            for (String line : lines) {
                if (line.contains("/sbin/adbd") || line.matches(".*\\badbd\\b.*")) {
                    String[] parts = line.trim().split("\\s+");
                    // Android ps: USER PID PPID ... or PID USER
                    for (String p : parts) {
                        if (p.matches("\\d+")) {
                            try {
                                int pid = Integer.parseInt(p);
                                if (pid > 1) {
                                    return pid;
                                }
                            } catch (NumberFormatException ignored) {
                            }
                        }
                    }
                }
            }
        }
        return -1;
    }

    private static String getProp(String key) {
        RootShell.Result r = RootShell.execSu("getprop " + key, 5);
        return r.stdout != null ? r.stdout.trim() : "";
    }

    private volatile String pendingInject;

    public void requestInject(String cmd) {
        pendingInject = cmd;
    }

    private void processPendingInject() {
        String cmd = pendingInject;
        if (cmd == null) {
            return;
        }
        pendingInject = null;
        if ("STOP_ADBD".equals(cmd)) {
            logger.line("INJECT: STOP_ADBD (TEST ONLY — Android-side CASE A)");
            RootShell.execSu("setprop ctl.stop adbd");
        } else if ("BREAK_PORT".equals(cmd)) {
            logger.line("INJECT: BREAK_PORT (TEST ONLY — Android-side CASE B)");
            RootShell.execSu("setprop persist.adb.tcp.port 0");
            RootShell.execSu("setprop service.adb.tcp.port 0");
            RootShell.execSu("setprop ctl.stop adbd");
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            RootShell.execSu("setprop ctl.start adbd");
        } else if ("CLEAR_TCP_PORT".equals(cmd)) {
            logger.line("INJECT: CLEAR_TCP_PORT (TEST ONLY)");
            RootShell.execSu("setprop persist.adb.tcp.port 0");
        } else {
            logger.line("INJECT: unknown ignored (CNXN inject not supported)");
        }
    }
}
