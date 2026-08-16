package com.nexus.adbwatchdog;

/**
 * Defaults aligned with native Nexus ADB Watchdog 2.4.
 */
public final class WatchdogConfig {
    public static final String VERSION_STR = "Nexus ADB Watchdog 2.4";
    public static final String VERSION_NUM = "2.4.0";
    public static final String PACKAGE_NAME = "com.nexus.adbwatchdog";

    public static final int ADB_PORT = 5555;
    public static final String ADB_PORT_STR = "5555";

    public int intervalSec = 5;
    public int maxRestart = 3;
    public int restartWindowSec = 600;
    public int cooldownSec = 60;
    public int adbdSleepSec = 3;
    public int logHeartbeatSec = 300;
    public boolean recoveryEnable = true;
    public boolean logEnable = true;

    /** Intent actions for Main APP integration. */
    public static final String ACTION_START = "com.nexus.adbwatchdog.action.START";
    public static final String ACTION_STOP = "com.nexus.adbwatchdog.action.STOP";
    public static final String ACTION_INJECT = "com.nexus.adbwatchdog.action.INJECT";
    public static final String EXTRA_INJECT = "inject_cmd";
}
