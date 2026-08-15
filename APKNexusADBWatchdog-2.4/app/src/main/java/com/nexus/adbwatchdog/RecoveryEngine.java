package com.nexus.adbwatchdog;

/**
 * Recovery aligned with native Watchdog 2.4 product rules (APK / root shell).
 *
 * HARD RULES:
 * - Fault recovery only — never periodic restart.
 * - ESTABLISHED &gt; 0 (live QtScrcpy/PC) =&gt; NEVER restart adbd.
 * - ESTABLISHED = 0 is NOT a fault.
 * - No localhost CNXN probe.
 * - CLOSE_WAIT is diagnostic only — NOT a recovery trigger in APK 2.4.
 * - Cooldown is post-fault gate; if healthy, clear recovery state immediately.
 * - Rate limit window — never permanent disable.
 */
public final class RecoveryEngine {

    public static final String ACTION_NONE = "NONE";
    public static final String ACTION_START_ADBD = "START_ADBD";
    public static final String ACTION_RESTART_ADBD = "RESTART_ADBD";
    public static final String ACTION_FIX_TCP_PORT = "FIX_TCP_PORT";
    public static final String ACTION_COOLDOWN = "RECOVERY_COOLDOWN";
    public static final String ACTION_RATE_LIMIT = "RATE_LIMIT";
    public static final String ACTION_SUCCESS = "RECOVERY_SUCCESS";

    private final WatchdogConfig cfg;
    private int restartCount;
    private long windowStartMs;
    private long cooldownUntilMs;
    private boolean inCooldown;
    private boolean pendingVerify;
    private String lastAction = ACTION_NONE;

    public RecoveryEngine(WatchdogConfig cfg) {
        this.cfg = cfg;
        this.windowStartMs = System.currentTimeMillis();
    }

    public String getLastAction() {
        return lastAction;
    }

    public int getRestartCount() {
        return restartCount;
    }

    public boolean isInCooldown() {
        return inCooldown;
    }

    public long cooldownRemainingSec() {
        long now = System.currentTimeMillis();
        if (cooldownUntilMs <= now) {
            return 0;
        }
        return (cooldownUntilMs - now) / 1000L;
    }

    /**
     * @return true if a recovery action was executed this cycle
     */
    public boolean evaluate(boolean adbdOk, boolean portOk, boolean propOk, int established,
                            WatchdogLogger log) {
        tickWindow(log);
        tickCooldown();

        if (!cfg.recoveryEnable) {
            if (!inCooldown && !ACTION_RATE_LIMIT.equals(lastAction)) {
                lastAction = ACTION_NONE;
            }
            return false;
        }

        boolean coreUp = adbdOk && portOk;

        // HARD: live PC/QtScrcpy session — never restart adbd
        if (coreUp && established > 0) {
            if (!propOk) {
                setTcpPortProps(); // soft fix only
            }
            noteHealthy(log);
            return false;
        }

        // Healthy idle (including ESTABLISHED=0): do nothing
        if (coreUp && propOk) {
            noteHealthy(log);
            return false;
        }

        // Core up but prop wrong, no live client: may restart once (rate-limited)
        if (coreUp && !propOk) {
            if (inCooldown) {
                lastAction = ACTION_COOLDOWN;
                return false;
            }
            if (!rateAllow(log)) {
                return false;
            }
            log.event("ADB FAULT DETECTED", "FAIL_REASON=TCP_PORT_PROP",
                    "why=persist.adb.tcp.port != 5555; action=FIX_TCP_PORT");
            setTcpPortProps();
            restartAdbd();
            lastAction = ACTION_FIX_TCP_PORT;
            log.event("RECOVERY ACTION", "FIX_TCP_PORT / adbd restarted", "");
            noteAttempt(log);
            return true;
        }

        if (inCooldown) {
            lastAction = ACTION_COOLDOWN;
            return false;
        }

        // CASE A
        if (!adbdOk) {
            if (!rateAllow(log)) {
                return false;
            }
            log.event("ADB FAULT DETECTED", "FAIL_REASON=ADBD_NOT_RUNNING",
                    "why=adbd process missing; action=START_ADBD");
            setTcpPortProps();
            startAdbd();
            lastAction = ACTION_START_ADBD;
            log.event("RECOVERY ACTION", "START_ADBD requested", "");
            noteAttempt(log);
            return true;
        }

        // CASE B
        if (!portOk) {
            if (!rateAllow(log)) {
                return false;
            }
            log.event("ADB FAULT DETECTED", "FAIL_REASON=PORT_NOT_LISTENING",
                    "why=TCP 5555 not LISTEN; action=RESTART_ADBD");
            restartAdbd();
            lastAction = ACTION_RESTART_ADBD;
            log.event("RECOVERY ACTION", "RESTART_ADBD requested", "");
            noteAttempt(log);
            return true;
        }

        return false;
    }

    private void noteHealthy(WatchdogLogger log) {
        inCooldown = false;
        cooldownUntilMs = 0;
        if (pendingVerify) {
            lastAction = ACTION_SUCCESS;
            log.event("RECOVERY SUCCESS",
                    "ADBD=YES PORT5555=YES Android-side TCP ADB OK",
                    "recovery state cleared; will NOT restart again");
            pendingVerify = false;
        } else if (!ACTION_RATE_LIMIT.equals(lastAction) && !ACTION_SUCCESS.equals(lastAction)) {
            lastAction = ACTION_NONE;
        }
    }

    private void noteAttempt(WatchdogLogger log) {
        restartCount++;
        pendingVerify = true;
        inCooldown = true;
        cooldownUntilMs = System.currentTimeMillis() + cfg.cooldownSec * 1000L;
        log.event("COOLDOWN ARMED",
                "post-recovery wait before next attempt",
                "restart_count=" + restartCount + " cooldown_sec=" + cfg.cooldownSec);
    }

    private void tickWindow(WatchdogLogger log) {
        long now = System.currentTimeMillis();
        long windowMs = cfg.restartWindowSec * 1000L;
        if (now - windowStartMs >= windowMs) {
            if (restartCount > 0) {
                log.event("RESTART WINDOW RESET",
                        "restart_count cleared; recovery still enabled",
                        "window=" + cfg.restartWindowSec + "s old_count=" + restartCount);
            }
            windowStartMs = now;
            restartCount = 0;
            if (ACTION_RATE_LIMIT.equals(lastAction)) {
                lastAction = ACTION_NONE;
                cooldownUntilMs = 0;
            }
        }
    }

    private void tickCooldown() {
        if (!inCooldown) {
            return;
        }
        if (System.currentTimeMillis() >= cooldownUntilMs) {
            inCooldown = false;
            cooldownUntilMs = 0;
            if (ACTION_COOLDOWN.equals(lastAction)) {
                lastAction = ACTION_NONE;
            }
        }
    }

    private boolean rateAllow(WatchdogLogger log) {
        tickWindow(log);
        tickCooldown();
        if (inCooldown) {
            lastAction = ACTION_COOLDOWN;
            return false;
        }
        if (restartCount >= cfg.maxRestart) {
            long now = System.currentTimeMillis();
            long remain = Math.max(0L, (windowStartMs + cfg.restartWindowSec * 1000L) - now) / 1000L;
            cooldownUntilMs = now + remain * 1000L;
            boolean was = ACTION_RATE_LIMIT.equals(lastAction);
            lastAction = ACTION_RATE_LIMIT;
            if (!was) {
                log.event("RATE LIMIT",
                        "waiting for restart_window to expire",
                        "restart_count=" + restartCount + " max=" + cfg.maxRestart
                                + " window_remain=" + remain + "s (no permanent disable)");
            }
            return false;
        }
        return true;
    }

    public static void setTcpPortProps() {
        RootShell.execSu("setprop persist.adb.tcp.port " + WatchdogConfig.ADB_PORT_STR);
        RootShell.execSu("setprop service.adb.tcp.port " + WatchdogConfig.ADB_PORT_STR);
    }

    public static void startAdbd() {
        RootShell.execSu("setprop ctl.start adbd");
    }

    public void restartAdbd() {
        setTcpPortProps();
        RootShell.execSu("setprop ctl.stop adbd");
        try {
            Thread.sleep(cfg.adbdSleepSec * 1000L);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        RootShell.execSu("setprop ctl.start adbd");
        try {
            Thread.sleep(cfg.adbdSleepSec * 1000L);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
