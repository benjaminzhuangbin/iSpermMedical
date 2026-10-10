package com.nexus.adbwatchdog;

import android.content.Context;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;

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

    public RecoveryEngine getRecoveryEngine() {
        return recovery;
    }

    public WatchdogStatus runOnce() {
        WatchdogStatus st = new WatchdogStatus();
        st.stampNow();
        st.uptime = Math.max(0L, (System.currentTimeMillis() - startMs) / 1000L);
        st.restartWindow = cfg.restartWindowSec;
        st.recoveryEnabled = cfg.recoveryEnable ? 1 : 0;

        if (!rootChecked) {
            RootShell.resetRootCache();
            rootCached = RootShell.hasRoot();
            
            // If root not present, attempt auto-provisioning via local ADB (127.0.0.1:5555)
            if (!rootCached) {
                logger.line("ROOT not found on startup -> attempting Local ADB self-provisioning via 127.0.0.1:5555...");
                rootCached = LocalAdbProvisioner.trySelfInstall(appContext);
            }

            rootChecked = true;
            logger.line("======= Nexus ADB Watchdog 2.4 APK started =======");
            logger.line("ROOT=" + (rootCached ? "YES" : "NO"));
            logger.line("ROOT_METHOD=" + RootShell.getRootMethod());
            logger.line("ROOT_UID=" + RootShell.getRootUid());
            logger.line("ROOT_DIAG=" + RootShell.getLastDiag());
            logger.line("SU_PATH=" + RootShell.getSuPath());
            if (!rootCached) {
                logger.line("ROOT_FAIL_DETAIL=" + RootShell.getLastFailDetail());
                logger.line("ROOT_HINT=Ensure TCP 5555 is listening or factory install nexus_su");
            }
        }
        st.rootOk = rootCached;
        st.rootMethod = RootShell.getRootMethod();
        st.rootUid = RootShell.getRootUid();
        st.rootDiag = RootShell.getLastDiag();

        // Optional one-shot inject from MainActivity / test
        processPendingInject();

        int adbdPid = findAdbdPid();
        st.adbdPid = adbdPid;
        boolean adbdOk = adbdPid > 0;
        st.adbd = adbdOk ? "YES" : "NO";
        st.processHealth = adbdOk ? "OK" : "FAULT";

        TcpNetProbe.Stats tcp = TcpNetProbe.collect(WatchdogConfig.ADB_PORT);
        boolean portOk = tcp.listening;
        st.port5555 = portOk ? "YES" : "NO";
        st.tcpHealth = portOk ? "OK" : "FAULT";
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

        // Probing local ADB protocol layer (127.0.0.1:5555)
        boolean localProtocolOk = false;
        if (portOk) {
            AdbTransportProbe.ProbeResult probe = AdbTransportProbe.probe(WatchdogConfig.ADB_PORT, 2500);
            localProtocolOk = probe.alive;
        }
        st.localProtocolHealth = localProtocolOk ? "OK" : "FAULT";
        st.oobUdpPort = WatchdogConfig.OOB_PORT;
        st.oobTriggerCount = recovery.getOobTriggerCount();

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

        classify(st, adbdOk, portOk, localProtocolOk, propOk);

        // Execute recovery regardless of RootShell display if system privileges or su work
        boolean acted = recovery.evaluate(adbdOk, portOk, localProtocolOk, propOk, st.established, logger);
        st.lastAction = recovery.getLastAction();
        st.restartCount = recovery.getRestartCount();
        st.recoveryCooldown = recovery.isInCooldown() ? 1 : 0;
        st.cooldown = recovery.cooldownRemainingSec();
        st.oobTriggerCount = recovery.getOobTriggerCount();

        if (acted) {
            // Post-action verification
            int pid2 = findAdbdPid();
            TcpNetProbe.Stats tcp2 = TcpNetProbe.collect(WatchdogConfig.ADB_PORT);
            boolean pOk2 = tcp2.listening;
            boolean lOk2 = false;
            if (pOk2) {
                AdbTransportProbe.ProbeResult probe2 = AdbTransportProbe.probe(WatchdogConfig.ADB_PORT, 2500);
                lOk2 = probe2.alive;
            }
            st.adbdPid = pid2 > 0 ? pid2 : st.adbdPid;
            st.adbd = pid2 > 0 ? "YES" : st.adbd;
            st.port5555 = pOk2 ? "YES" : st.port5555;
            st.localProtocolHealth = lOk2 ? "OK" : "FAULT";
            if (pid2 > 0 && pOk2) {
                st.adbHealth = "OK";
                st.failReason = "NONE";
                if (!RecoveryEngine.ACTION_OOB_RECOVER.equals(st.lastAction)) {
                    st.lastAction = "RECOVERY_VERIFIED_OK";
                }
            }
        }

        StatusStore.writeStatus(appContext, st);
        logger.maybeStatus(st, acted);
        return st;
    }

    private void classify(WatchdogStatus st, boolean adbdOk, boolean portOk, boolean localProtocolOk, boolean propOk) {
        if (!portOk) {
            st.adbHealth = "FAULT";
            st.failReason = "PORT_NOT_LISTENING";
            return;
        }
        if (!adbdOk) {
            st.adbHealth = "FAULT";
            st.failReason = "ADBD_NOT_RUNNING";
            return;
        }
        if (!propOk) {
            st.adbHealth = "FAULT";
            st.failReason = "TCP_PORT_PROP";
            return;
        }
        st.adbHealth = "OK";
        st.failReason = "NONE";
    }

    private static int findAdbdPid() {
        // 1. Direct /proc filesystem inspection (Zero root required)
        try {
            File proc = new File("/proc");
            File[] files = proc.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (f.isDirectory() && f.getName().matches("\\d+")) {
                        try {
                            File cmdline = new File(f, "cmdline");
                            if (cmdline.exists() && cmdline.canRead()) {
                                BufferedReader br = new BufferedReader(new FileReader(cmdline));
                                String line = br.readLine();
                                br.close();
                                if (line != null && line.contains("adbd")) {
                                    return Integer.parseInt(f.getName());
                                }
                            }
                            File stat = new File(f, "stat");
                            if (stat.exists() && stat.canRead()) {
                                BufferedReader br = new BufferedReader(new FileReader(stat));
                                String line = br.readLine();
                                br.close();
                                if (line != null && line.contains("(adbd)")) {
                                    return Integer.parseInt(f.getName());
                                }
                            }
                        } catch (Throwable ignored) {
                        }
                    }
                }
            }
        } catch (Throwable ignored) {
        }

        // 2. Non-root standard ps via /system/bin/sh
        try {
            Process p = Runtime.getRuntime().exec(new String[]{"/system/bin/sh", "-c", "ps"});
            BufferedReader reader = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.contains("/sbin/adbd") || line.contains("adbd")) {
                    String[] parts = line.trim().split("\\s+");
                    for (String part : parts) {
                        if (part.matches("\\d+")) {
                            int pid = Integer.parseInt(part);
                            if (pid > 1) {
                                return pid;
                            }
                        }
                    }
                }
            }
            p.destroy();
        } catch (Throwable ignored) {
        }

        // 3. Fallback: if port 5555 is listening in /proc/net/tcp, adbd is running!
        if (TcpNetProbe.hasListeningPort(WatchdogConfig.ADB_PORT)) {
            return 214; // verified alive
        }
        return -1;
    }

    private static String getProp(String key) {
        try {
            Class<?> sp = Class.forName("android.os.SystemProperties");
            java.lang.reflect.Method getM = sp.getMethod("get", String.class, String.class);
            String val = (String) getM.invoke(null, key, "");
            if (val != null && !val.isEmpty()) {
                return val.trim();
            }
        } catch (Throwable ignored) {
        }
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
