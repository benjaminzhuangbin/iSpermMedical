package com.nexus.adbwatchdog;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Status snapshot matching native 2.4 watchdog.status fields.
 */
public class WatchdogStatus {
    public String timeStr = "";
    public int adbdPid = -1;
    public String adbd = "NO";
    public String port5555 = "NO";
    public int established;
    public String client = "";
    public String clients = "";
    public String clientState = "NO_CLIENT";
    public String tcpHealth = "UNKNOWN";
    public String adbHealth = "UNKNOWN";
    public String failReason = "NONE";
    public String lastAction = "NONE";
    public int recoveryEnabled = 1;
    public int recoveryCooldown;
    public int restartCount;
    public int restartWindow = 600;
    public long cooldown;
    public int totalConnections;
    public long uptime;
    public String propTcp = "";
    public int propOk;
    public int closeWait;
    public int timeWait;
    public int synRecv;
    public boolean rootOk;
    public String rootMethod = "NONE";
    public String rootUid = "-1";
    public String rootDiag = "";

    public void stampNow() {
        timeStr = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
    }

    public String toStatusFile() {
        StringBuilder sb = new StringBuilder();
        sb.append(WatchdogConfig.VERSION_STR).append('\n');
        sb.append("TIME=").append(timeStr).append('\n');
        sb.append("ADBD_PID=").append(adbdPid).append('\n');
        sb.append("ADBD=").append(adbd).append('\n');
        sb.append("PORT5555=").append(port5555).append('\n');
        sb.append("ESTABLISHED=").append(established).append('\n');
        sb.append("CLIENT=").append(client != null ? client : "").append('\n');
        sb.append("CLIENTS=").append(clients != null ? clients : "").append('\n');
        sb.append("CLIENT_STATE=").append(clientState).append('\n');
        sb.append("TCP_HEALTH=").append(tcpHealth).append('\n');
        sb.append("ADB_HEALTH=").append(adbHealth).append('\n');
        sb.append("FAIL_REASON=").append(failReason).append('\n');
        sb.append("LAST_ACTION=").append(lastAction).append('\n');
        sb.append("RECOVERY_ENABLED=").append(recoveryEnabled).append('\n');
        sb.append("RECOVERY_COOLDOWN=").append(recoveryCooldown).append('\n');
        sb.append("RESTART_COUNT=").append(restartCount).append('\n');
        sb.append("RESTART_WINDOW=").append(restartWindow).append('\n');
        sb.append("COOLDOWN=").append(cooldown).append('\n');
        sb.append("TOTAL_CONNECTIONS=").append(totalConnections).append('\n');
        sb.append("UPTIME=").append(uptime).append('\n');
        sb.append("PROP_TCP=").append(propTcp != null ? propTcp : "").append('\n');
        sb.append("PROP_OK=").append(propOk).append('\n');
        sb.append("CLOSE_WAIT=").append(closeWait).append('\n');
        sb.append("TIME_WAIT=").append(timeWait).append('\n');
        sb.append("SYN_RECV=").append(synRecv).append('\n');
        sb.append("ROOT_OK=").append(rootOk ? 1 : 0).append('\n');
        sb.append("ROOT_METHOD=").append(rootMethod != null ? rootMethod : "NONE").append('\n');
        sb.append("ROOT_UID=").append(rootUid != null ? rootUid : "-1").append('\n');
        sb.append("MODE=APK_PERMANENT\n");
        sb.append("LOG_DIR=").append(StatusStore.PUBLIC_DIR_PATH).append('\n');
        return sb.toString();
    }
}
