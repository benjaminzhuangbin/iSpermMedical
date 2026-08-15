package com.nexus.adbwatchdog;

import android.content.Context;
import android.text.TextUtils;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public final class WatchdogLogger {
    private final Context appContext;
    private final WatchdogConfig cfg;
    private long lastHeartbeatMs;
    private String prevSnap = "";

    public WatchdogLogger(Context ctx, WatchdogConfig cfg) {
        this.appContext = ctx.getApplicationContext();
        this.cfg = cfg;
        this.lastHeartbeatMs = 0;
    }

    public void event(String a, String b, String c) {
        if (!cfg.logEnable) {
            return;
        }
        String ts = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
        StringBuilder sb = new StringBuilder();
        sb.append('\n').append(ts).append('\n');
        if (!TextUtils.isEmpty(a)) sb.append(a).append('\n');
        if (!TextUtils.isEmpty(b)) sb.append(b).append('\n');
        if (!TextUtils.isEmpty(c)) sb.append(c).append('\n');
        StatusStore.appendLog(appContext, sb.toString());
    }

    public void line(String msg) {
        if (!cfg.logEnable) {
            return;
        }
        StatusStore.appendLog(appContext, msg);
    }

    public void maybeStatus(WatchdogStatus st, boolean recoveryActed) {
        if (!cfg.logEnable) {
            return;
        }
        String snap = st.adbd + "|" + st.port5555 + "|" + st.tcpHealth + "|" + st.adbHealth
                + "|" + st.failReason + "|" + st.lastAction + "|" + st.recoveryCooldown;
        long now = System.currentTimeMillis();
        boolean heartbeat = cfg.logHeartbeatSec > 0
                && lastHeartbeatMs > 0
                && (now - lastHeartbeatMs) >= cfg.logHeartbeatSec * 1000L;
        if (!recoveryActed && snap.equals(prevSnap) && !heartbeat && lastHeartbeatMs != 0) {
            return;
        }
        prevSnap = snap;
        lastHeartbeatMs = now;
        StringBuilder sb = new StringBuilder();
        sb.append('\n').append(st.timeStr).append('\n');
        sb.append(WatchdogConfig.VERSION_STR).append('\n');
        sb.append("ADBD=").append(st.adbd)
                .append(" PORT5555=").append(st.port5555)
                .append(" ESTABLISHED=").append(st.established)
                .append(" CLIENT_STATE=").append(st.clientState)
                .append(" CLIENT=").append(TextUtils.isEmpty(st.client) ? "-" : st.client)
                .append('\n');
        sb.append("TCP_HEALTH=").append(st.tcpHealth)
                .append(" ADB_HEALTH=").append(st.adbHealth)
                .append(" FAIL_REASON=").append(st.failReason).append('\n');
        sb.append("LAST_ACTION=").append(st.lastAction)
                .append(" RECOVERY_ENABLED=").append(st.recoveryEnabled)
                .append(" RECOVERY_COOLDOWN=").append(st.recoveryCooldown).append('\n');
        sb.append("RESTART_COUNT=").append(st.restartCount)
                .append(" RESTART_WINDOW=").append(st.restartWindow)
                .append(" COOLDOWN=").append(st.cooldown).append('\n');
        StatusStore.appendLog(appContext, sb.toString());
    }
}
