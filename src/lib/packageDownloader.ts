import JSZip from 'jszip';

// Bundles the complete APKNexusADBWatchdog-2.4 project with all fixed files
export async function downloadFixedWatchdogPackage(): Promise<void> {
  const zip = new JSZip();

  // Root files
  zip.file(
    'APKNexusADBWatchdog-2.4/build_apk.bat',
    `@echo off\r\nREM build_apk.bat — One-click build for NexusADBWatchdog 2.4 (Android 5.1.1)\r\nsetlocal\r\ncd /d "%~dp0"\r\n\r\necho ========================================================\r\necho   Building NexusADBWatchdog 2.4 APK (Android 5.1.1)\r\necho ========================================================\r\n\r\ncall gradlew.bat assembleDebug\r\n\r\nif not exist "release" mkdir "release"\r\n\r\nif exist "app\\build\\outputs\\apk\\debug\\app-debug.apk" (\r\n    copy /Y "app\\build\\outputs\\apk\\debug\\app-debug.apk" "release\\NexusADBWatchdog.apk" >nul\r\n    echo.\r\n    echo [OK] Build Succeeded!\r\n    echo Output APK: release\\NexusADBWatchdog.apk\r\n    echo.\r\n    echo Next Step:\r\n    echo   Run: factory\\install_system.bat\r\n) else (\r\n    echo.\r\n    echo [ERROR] Build failed! Check Gradle logs above.\r\n)\r\n`
  );

  zip.file(
    'APKNexusADBWatchdog-2.4/build.gradle',
    `buildscript {\r\n    repositories {\r\n        google()\r\n        mavenCentral()\r\n    }\r\n    dependencies {\r\n        classpath 'com.android.tools.build:gradle:3.4.3'\r\n    }\r\n}\r\n\r\nallprojects {\r\n    repositories {\r\n        google()\r\n        mavenCentral()\r\n    }\r\n}\r\n`
  );

  zip.file(
    'APKNexusADBWatchdog-2.4/settings.gradle',
    `include ':app'\r\n`
  );

  zip.file(
    'APKNexusADBWatchdog-2.4/gradle.properties',
    `org.gradle.jvmargs=-Xmx2048m\r\nandroid.useAndroidX=false\r\nandroid.enableJetifier=false\r\n`
  );

  // Factory folder
  const factory = zip.folder('APKNexusADBWatchdog-2.4/factory')!;
  factory.file(
    'install_system.bat',
    `@echo off\r\nREM factory/install_system.bat — Windows one-shot factory install (CRLF)\r\nREM Requires: adb, device reachable, shell can "su -c".\r\nsetlocal\r\ncd /d "%~dp0\\.."\r\n\r\nif exist "app\\build\\outputs\\apk\\debug\\app-debug.apk" (\r\n  if not exist "release" mkdir "release"\r\n  copy /Y "app\\build\\outputs\\apk\\debug\\app-debug.apk" "release\\NexusADBWatchdog.apk" >nul\r\n)\r\n\r\nif not exist "release\\NexusADBWatchdog.apk" (\r\n  echo [info] Building APK from gradle...\r\n  call gradlew.bat assembleDebug\r\n  if exist "app\\build\\outputs\\apk\\debug\\app-debug.apk" (\r\n    if not exist "release" mkdir "release"\r\n    copy /Y "app\\build\\outputs\\apk\\debug\\app-debug.apk" "release\\NexusADBWatchdog.apk" >nul\r\n  )\r\n)\r\n\r\necho [push] APK + install script\r\nif exist "release\\NexusADBWatchdog.apk" (\r\n  adb push release\\NexusADBWatchdog.apk /data/local/tmp/NexusADBWatchdog.apk\r\n) else if exist "app\\build\\outputs\\apk\\debug\\app-debug.apk" (\r\n  adb push app\\build\\outputs\\apk\\debug\\app-debug.apk /data/local/tmp/NexusADBWatchdog.apk\r\n)\r\nadb push factory\\install_on_device.sh /data/local/tmp/install_on_device.sh\r\nadb shell "chmod 755 /data/local/tmp/install_on_device.sh"\r\nif errorlevel 1 (\r\n  echo ERROR: adb push failed\r\n  exit /b 1\r\n)\r\n\r\necho [run] su factory install\r\nadb shell su 0 /system/bin/sh /data/local/tmp/install_on_device.sh || adb shell su -c "/system/bin/sh /data/local/tmp/install_on_device.sh"\r\nif errorlevel 1 (\r\n  echo ERROR: factory install failed\r\n  exit /b 1\r\n)\r\n\r\necho.\r\necho Reboot now? Recommended.\r\necho   adb reboot\r\necho After reboot verify:\r\necho   adb shell "cat /sdcard/NexusADBWatchdog/watchdog.status"\r\nendlocal\r\n`
  );

  factory.file(
    'install_on_device.sh',
    `#!/system/bin/sh\n# factory/install_on_device.sh\nset -e\n\nAPK_SRC="/data/local/tmp/NexusADBWatchdog.apk"\nNEXUS_SU_DST="/system/xbin/nexus_su"\nPRIV_DIR="/system/priv-app/NexusADBWatchdog"\nPRIV_APK="$PRIV_DIR/NexusADBWatchdog.apk"\n\necho "=== Nexus ADB Watchdog factory install ==="\nid\n\nif [ ! -f "$APK_SRC" ]; then\n  echo "ERROR: missing $APK_SRC"\n  exit 1\nfi\n\nmount -o rw,remount /system 2>/dev/null || mount -o remount,rw /system 2>/dev/null || true\n\necho "[1/3] Configure system root access"\nif [ -f "/system/xbin/su" ]; then\n  cp /system/xbin/su "$NEXUS_SU_DST"\nfi\nchown 0:0 "$NEXUS_SU_DST" 2>/dev/null || chown root:root "$NEXUS_SU_DST" 2>/dev/null || true\nchmod 6755 "$NEXUS_SU_DST" 2>/dev/null || chmod 4755 "$NEXUS_SU_DST" 2>/dev/null || true\nchmod 6755 /system/xbin/su 2>/dev/null || chmod 4755 /system/xbin/su 2>/dev/null || true\nls -l "$NEXUS_SU_DST"\n\necho "[2/3] Verify root access"\n/system/xbin/su 0 /system/bin/sh -c id || "$NEXUS_SU_DST" 0 /system/bin/sh -c id || true\n\necho "[3/3] Install APK as priv-app -> $PRIV_APK"\nmkdir -p "$PRIV_DIR"\nchmod 755 "$PRIV_DIR"\ncp "$APK_SRC" "$PRIV_APK"\nchmod 644 "$PRIV_APK"\nchown 0:0 "$PRIV_APK" 2>/dev/null || chown root:root "$PRIV_APK" 2>/dev/null || true\n\npm install -r "$PRIV_APK" 2>/dev/null || true\nrm -f /sdcard/NexusADBWatchdog/watchdog.status 2>/dev/null || true\n\nsync\nmount -o remount,ro /system 2>/dev/null || true\n\necho "=== OK: reboot required ==="\n`
  );

  // App folder
  const app = zip.folder('APKNexusADBWatchdog-2.4/app')!;
  app.file(
    'build.gradle',
    `apply plugin: 'com.android.application'\r\n\r\nandroid {\r\n    compileSdkVersion 22\r\n    buildToolsVersion "28.0.3"\r\n\r\n    defaultConfig {\r\n        applicationId "com.nexus.adbwatchdog"\r\n        minSdkVersion 22\r\n        targetSdkVersion 22\r\n        versionCode 240\r\n        versionName "2.4"\r\n    }\r\n\r\n    buildTypes {\r\n        release {\r\n            minifyEnabled false\r\n            proguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'\r\n        }\r\n        debug {\r\n            minifyEnabled false\r\n        }\r\n    }\r\n}\r\n\r\ndependencies {\r\n}\r\n`
  );

  // AndroidManifest.xml
  app.file(
    'src/main/AndroidManifest.xml',
    `<?xml version="1.0" encoding="utf-8"?>\r\n<manifest xmlns:android="http://schemas.android.com/apk/res/android"\r\n    package="com.nexus.adbwatchdog">\r\n\r\n    <uses-permission android:name="android.permission.INTERNET" />\r\n    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />\r\n    <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED" />\r\n    <uses-permission android:name="android.permission.WAKE_LOCK" />\r\n    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />\r\n    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />\r\n\r\n    <application\r\n        android:name=".WatchdogApp"\r\n        android:allowBackup="false"\r\n        android:icon="@drawable/ic_launcher"\r\n        android:label="@string/app_name"\r\n        android:supportsRtl="true"\r\n        android:theme="@style/AppTheme">\r\n\r\n        <activity\r\n            android:name=".MainActivity"\r\n            android:exported="true"\r\n            android:launchMode="singleTop"\r\n            android:label="@string/app_name">\r\n            <intent-filter>\r\n                <action android:name="android.intent.action.MAIN" />\r\n                <category android:name="android.intent.category.LAUNCHER" />\r\n            </intent-filter>\r\n        </activity>\r\n\r\n        <service\r\n            android:name=".NexusADBWatchdogService"\r\n            android:enabled="true"\r\n            android:exported="true">\r\n            <intent-filter>\r\n                <action android:name="com.nexus.adbwatchdog.action.START" />\r\n                <action android:name="com.nexus.adbwatchdog.action.STOP" />\r\n            </intent-filter>\r\n        </service>\r\n\r\n        <receiver\r\n            android:name=".BootReceiver"\r\n            android:enabled="true"\r\n            android:exported="true">\r\n            <intent-filter>\r\n                <action android:name="android.intent.action.BOOT_COMPLETED" />\r\n                <action android:name="android.intent.action.QUICKBOOT_POWERON" />\r\n                <action android:name="android.intent.action.REBOOT" />\r\n                <action android:name="android.net.conn.CONNECTIVITY_CHANGE" />\r\n                <action android:name="android.net.ethernet.STATE_CHANGE" />\r\n                <action android:name="android.net.ethernet.ETHERNET_STATE_CHANGED" />\r\n            </intent-filter>\r\n        </receiver>\r\n    </application>\r\n</manifest>\r\n`
  );

  // Java files folder
  const javaDir = app.folder('src/main/java/com/nexus/adbwatchdog')!;

  javaDir.file(
    'WatchdogConfig.java',
    `package com.nexus.adbwatchdog;

public final class WatchdogConfig {
    public static final String VERSION_STR = "Nexus ADB Watchdog 2.4";
    public static final String VERSION_NUM = "2.4.0";
    public static final String PACKAGE_NAME = "com.nexus.adbwatchdog";

    public static final int ADB_PORT = 5555;
    public static final String ADB_PORT_STR = "5555";
    public static final int OOB_PORT = 5556;

    public int intervalSec = 5;
    public int maxRestart = 3;
    public int restartWindowSec = 600;
    public int cooldownSec = 60;
    public int adbdSleepSec = 3;
    public int logHeartbeatSec = 300;
    public boolean recoveryEnable = true;
    public boolean logEnable = true;

    public static final String ACTION_START = "com.nexus.adbwatchdog.action.START";
    public static final String ACTION_STOP = "com.nexus.adbwatchdog.action.STOP";
    public static final String ACTION_INJECT = "com.nexus.adbwatchdog.action.INJECT";
    public static final String EXTRA_INJECT = "inject_cmd";
}
`
  );

  javaDir.file(
    'OobRecoveryServer.java',
    `package com.nexus.adbwatchdog;

import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/**
 * Out-of-band (OOB) UDP control channel on port 5556.
 * Provides an independent recovery path when Android adbd enters a zombie/offline state
 * and standard TCP ADB commands cannot reach adbd.
 */
public final class OobRecoveryServer {

    private static final String TAG = "OobRecoveryServer";
    public static final int DEFAULT_PORT = 5556;

    private final int port;
    private final RecoveryEngine recoveryEngine;
    private final WatchdogLogger logger;
    private volatile boolean running;
    private DatagramSocket socket;
    private Thread serverThread;

    public OobRecoveryServer(int port, RecoveryEngine recoveryEngine, WatchdogLogger logger) {
        this.port = port;
        this.recoveryEngine = recoveryEngine;
        this.logger = logger;
    }

    public synchronized void start() {
        if (running) {
            return;
        }
        running = true;
        serverThread = new Thread(new Runnable() {
            @Override
            public void run() {
                runLoop();
            }
        }, "oob-recovery-udp-5556");
        serverThread.setDaemon(true);
        serverThread.start();
        Log.i(TAG, "OOB Recovery UDP server started on port " + port);
    }

    public synchronized void stop() {
        running = false;
        if (socket != null && !socket.isClosed()) {
            try {
                socket.close();
            } catch (Throwable ignored) {
            }
        }
        if (serverThread != null) {
            serverThread.interrupt();
            serverThread = null;
        }
        Log.i(TAG, "OOB Recovery UDP server stopped");
    }

    private void runLoop() {
        byte[] buf = new byte[1024];
        try {
            socket = new DatagramSocket(port);
            while (running) {
                try {
                    DatagramPacket packet = new DatagramPacket(buf, buf.length);
                    socket.receive(packet);

                    String msg = new String(packet.getData(), packet.getOffset(), packet.getLength(), "UTF-8").trim();
                    InetAddress clientAddr = packet.getAddress();
                    int clientPort = packet.getPort();

                    if (msg.contains("RECOVER_ADBD")) {
                        Log.w(TAG, "Received OOB RECOVER_ADBD from " + clientAddr.getHostAddress() + ":" + clientPort);
                        if (logger != null) {
                            logger.event("OOB RECOVERY REQUEST",
                                    "source=" + clientAddr.getHostAddress() + ":" + clientPort,
                                    "msg=" + msg);
                        }

                        // Send instant ACK back to PC
                        String ack = "ACK:RECOVER_ADBD:TIME=" + System.currentTimeMillis() + "\\n";
                        byte[] ackBytes = ack.getBytes("UTF-8");
                        DatagramPacket ackPacket = new DatagramPacket(ackBytes, ackBytes.length, clientAddr, clientPort);
                        try {
                            socket.send(ackPacket);
                        } catch (Throwable t) {
                            Log.e(TAG, "Failed to send ACK to " + clientAddr, t);
                        }

                        // Trigger recovery on adbd
                        if (recoveryEngine != null) {
                            recoveryEngine.triggerOobRecovery("UDP_5556:" + clientAddr.getHostAddress(), logger);
                        }
                    } else if (msg.contains("PING")) {
                        String ack = "PONG:NEXUS_WATCHDOG_2.4\\n";
                        byte[] ackBytes = ack.getBytes("UTF-8");
                        DatagramPacket ackPacket = new DatagramPacket(ackBytes, ackBytes.length, clientAddr, clientPort);
                        socket.send(ackPacket);
                    }
                } catch (Throwable t) {
                    if (!running) {
                        break;
                    }
                    Log.w(TAG, "Error processing UDP packet: " + t.getMessage());
                }
            }
        } catch (Throwable t) {
            if (running) {
                Log.e(TAG, "OOB UDP socket failed on port " + port, t);
            }
        } finally {
            if (socket != null && !socket.isClosed()) {
                try {
                    socket.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }
}
`
  );

  javaDir.file(
    'AdbTransportProbe.java',
    `package com.nexus.adbwatchdog;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public final class AdbTransportProbe {

    private static final int A_CNXN = 0x4e584e43; // 'CNXN'
    private static final int A_AUTH = 0x48545541; // 'AUTH'
    private static final int A_VERSION = 0x01000000;
    private static final int MAX_PAYLOAD = 4096;

    public static final class ProbeResult {
        public final boolean alive;
        public final String status;
        public final long latencyMs;

        public ProbeResult(boolean alive, String status, long latencyMs) {
            this.alive = alive;
            this.status = status;
            this.latencyMs = latencyMs;
        }
    }

    private AdbTransportProbe() {
    }

    public static ProbeResult probe(int port, int timeoutMs) {
        long start = System.currentTimeMillis();
        Socket socket = null;
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress("127.0.0.1", port), Math.min(timeoutMs, 2000));
            socket.setSoTimeout(Math.min(timeoutMs, 3000));

            OutputStream out = socket.getOutputStream();
            InputStream in = socket.getInputStream();

            byte[] banner = "host::NexusWatchdogTransportProbe\\0".getBytes("UTF-8");
            ByteBuffer header = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
            header.putInt(A_CNXN);
            header.putInt(A_VERSION);
            header.putInt(MAX_PAYLOAD);
            header.putInt(banner.length);

            int crc = 0;
            for (byte b : banner) {
                crc += (b & 0xFF);
            }
            header.putInt(crc);
            header.putInt(~A_CNXN);

            out.write(header.array());
            out.write(banner);
            out.flush();

            byte[] respHeader = new byte[24];
            int readTotal = 0;
            while (readTotal < 24) {
                int r = in.read(respHeader, readTotal, 24 - readTotal);
                if (r < 0) {
                    return new ProbeResult(false, "EOF", System.currentTimeMillis() - start);
                }
                readTotal += r;
            }

            ByteBuffer respBuf = ByteBuffer.wrap(respHeader).order(ByteOrder.LITTLE_ENDIAN);
            int cmd = respBuf.getInt();

            if (cmd == A_AUTH || cmd == A_CNXN) {
                return new ProbeResult(true, "OK", System.currentTimeMillis() - start);
            }

            return new ProbeResult(false, "UNEXPECTED_CMD_0x" + Integer.toHexString(cmd), System.currentTimeMillis() - start);
        } catch (java.net.SocketTimeoutException e) {
            return new ProbeResult(false, "TIMEOUT", System.currentTimeMillis() - start);
        } catch (java.net.ConnectException e) {
            return new ProbeResult(false, "REFUSED", System.currentTimeMillis() - start);
        } catch (Throwable t) {
            return new ProbeResult(false, "ERROR:" + t.getClass().getSimpleName(), System.currentTimeMillis() - start);
        } finally {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }
}
`
  );

  javaDir.file(
    'WatchdogStatus.java',
    `package com.nexus.adbwatchdog;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class WatchdogStatus {
    public String timeStr = "";
    public int adbdPid = -1;
    public String adbd = "NO";
    public String port5555 = "NO";
    public int established;
    public String client = "";
    public String clients = "";
    public String clientState = "NO_CLIENT";
    public String processHealth = "UNKNOWN";
    public String tcpHealth = "UNKNOWN";
    public String localProtocolHealth = "UNKNOWN";
    public String adbHealth = "UNKNOWN";
    public String failReason = "NONE";
    public String lastAction = "NONE";
    public int oobUdpPort = 5556;
    public int oobTriggerCount = 0;
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
        sb.append(WatchdogConfig.VERSION_STR).append('\\n');
        sb.append("TIME=").append(timeStr).append('\\n');
        sb.append("ADBD_PID=").append(adbdPid).append('\\n');
        sb.append("ADBD=").append(adbd).append('\\n');
        sb.append("PORT5555=").append(port5555).append('\\n');
        sb.append("ESTABLISHED=").append(established).append('\\n');
        sb.append("CLIENT=").append(client != null ? client : "").append('\\n');
        sb.append("CLIENTS=").append(clients != null ? clients : "").append('\\n');
        sb.append("CLIENT_STATE=").append(clientState).append('\\n');
        sb.append("PROCESS_HEALTH=").append(processHealth).append('\\n');
        sb.append("TCP_HEALTH=").append(tcpHealth).append('\\n');
        sb.append("LOCAL_PROTOCOL_HEALTH=").append(localProtocolHealth).append('\\n');
        sb.append("ADB_HEALTH=").append(adbHealth).append('\\n');
        sb.append("FAIL_REASON=").append(failReason).append('\\n');
        sb.append("LAST_ACTION=").append(lastAction).append('\\n');
        sb.append("OOB_UDP_PORT=").append(oobUdpPort).append('\\n');
        sb.append("OOB_TRIGGER_COUNT=").append(oobTriggerCount).append('\\n');
        sb.append("RECOVERY_ENABLED=").append(recoveryEnabled).append('\\n');
        sb.append("RECOVERY_COOLDOWN=").append(recoveryCooldown).append('\\n');
        sb.append("RESTART_COUNT=").append(restartCount).append('\\n');
        sb.append("RESTART_WINDOW=").append(restartWindow).append('\\n');
        sb.append("COOLDOWN=").append(cooldown).append('\\n');
        sb.append("TOTAL_CONNECTIONS=").append(totalConnections).append('\\n');
        sb.append("UPTIME=").append(uptime).append('\\n');
        sb.append("PROP_TCP=").append(propTcp != null ? propTcp : "").append('\\n');
        sb.append("PROP_OK=").append(propOk).append('\\n');
        sb.append("CLOSE_WAIT=").append(closeWait).append('\\n');
        sb.append("TIME_WAIT=").append(timeWait).append('\\n');
        sb.append("SYN_RECV=").append(synRecv).append('\\n');
        sb.append("ROOT_OK=").append(rootOk ? 1 : 0).append('\\n');
        sb.append("ROOT_METHOD=").append(rootMethod != null ? rootMethod : "NONE").append('\\n');
        sb.append("ROOT_UID=").append(rootUid != null ? rootUid : "-1").append('\\n');
        sb.append("MODE=APK_PERMANENT\\n");
        sb.append("LOG_DIR=").append(StatusStore.PUBLIC_DIR_PATH).append('\\n');
        return sb.toString();
    }
}
`
  );

  javaDir.file(
    'WatchdogEngine.java',
    `package com.nexus.adbwatchdog;

import android.content.Context;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;

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
        }
        st.rootOk = rootCached;
        st.rootMethod = RootShell.getRootMethod();
        st.rootUid = RootShell.getRootUid();
        st.rootDiag = RootShell.getLastDiag();

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

        boolean acted = recovery.evaluate(adbdOk, portOk, localProtocolOk, propOk, st.established, logger);
        st.lastAction = recovery.getLastAction();
        st.restartCount = recovery.getRestartCount();
        st.recoveryCooldown = recovery.isInCooldown() ? 1 : 0;
        st.cooldown = recovery.cooldownRemainingSec();
        st.oobTriggerCount = recovery.getOobTriggerCount();

        if (acted) {
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
        try {
            File proc = new File("/proc");
            File[] files = proc.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (f.isDirectory() && f.getName().matches("\\\\d+")) {
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
        return -1;
    }

    private static String getProp(String key) {
        try {
            Process p = Runtime.getRuntime().exec(new String[]{"getprop", key});
            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String l = r.readLine();
            r.close();
            p.destroy();
            return l != null ? l : "";
        } catch (Throwable t) {
            return "";
        }
    }

    private void processPendingInject() {
        // development hook if needed
    }

    public void requestInject(String cmd) {
        // development hook
    }
}
`
  );

  javaDir.file(
    'RecoveryEngine.java',
    `package com.nexus.adbwatchdog;

public final class RecoveryEngine {

    public static final String ACTION_NONE = "NONE";
    public static final String ACTION_START_ADBD = "START_ADBD";
    public static final String ACTION_RESTART_ADBD = "RESTART_ADBD";
    public static final String ACTION_OOB_RECOVER = "OOB_RECOVER_ADBD";
    public static final String ACTION_FIX_TCP_PORT = "FIX_TCP_PORT";
    public static final String ACTION_COOLDOWN = "RECOVERY_COOLDOWN";
    public static final String ACTION_RATE_LIMIT = "RATE_LIMIT";
    public static final String ACTION_SUCCESS = "RECOVERY_SUCCESS";

    private final WatchdogConfig cfg;
    private int restartCount;
    private int oobTriggerCount;
    private long lastOobTimeMs;
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

    public int getOobTriggerCount() {
        return oobTriggerCount;
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

    public boolean evaluate(boolean adbdOk, boolean portOk, boolean transportOk, boolean propOk, int established,
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

        // Healthy session or idle: adbd process exists and port 5555 listens
        if (coreUp) {
            if (!propOk) {
                setTcpPortProps(); // soft fix only
            }
            noteHealthy(log);
            return false;
        }

        if (inCooldown) {
            lastAction = ACTION_COOLDOWN;
            return false;
        }

        // CASE 1: adbd process missing
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

        // CASE 2: TCP 5555 not listening
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

    /**
     * Out-of-band recovery triggered by PC via UDP 5556 control packet.
     * Guaranteed recovery path when Windows ADB transport reports offline.
     */
    public synchronized boolean triggerOobRecovery(String source, WatchdogLogger log) {
        long now = System.currentTimeMillis();
        // 15-second debounce window
        if (now - lastOobTimeMs < 15000L) {
            if (log != null) {
                log.event("OOB RECOVERY DEBOUNCED", "source=" + source,
                        "adbd recently recovered; ignoring rapid duplicate request");
            }
            return false;
        }
        lastOobTimeMs = now;
        oobTriggerCount++;

        if (log != null) {
            log.event("OOB RECOVERY TRIGGERED", "source=" + source,
                    "PC requested out-of-band adbd recovery; action=RESTART_ADBD");
        }
        restartAdbd();
        lastAction = ACTION_OOB_RECOVER;
        noteAttempt(log);
        return true;
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
                        "restart count cleared",
                        "old_count=" + restartCount + " window_sec=" + cfg.restartWindowSec);
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
        setPropSafe("persist.adb.tcp.port", WatchdogConfig.ADB_PORT_STR);
        setPropSafe("service.adb.tcp.port", WatchdogConfig.ADB_PORT_STR);
        setPropSafe("persist.sys.usb.config", "mass_storage,adb");
        setPropSafe("sys.usb.config", "mass_storage,adb");
    }

    private static void setPropSafe(String key, String val) {
        try {
            Class<?> sp = Class.forName("android.os.SystemProperties");
            java.lang.reflect.Method setM = sp.getMethod("set", String.class, String.class);
            setM.invoke(null, key, val);
        } catch (Throwable ignored) {
        }
        RootShell.execSu("setprop " + key + " " + val);
    }

    public static void startAdbd() {
        setPropSafe("ctl.start", "adbd");
    }

    public void restartAdbd() {
        setTcpPortProps();
        setPropSafe("ctl.stop", "adbd");
        setPropSafe("sys.usb.config", "none");
        try {
            Thread.sleep(cfg.adbdSleepSec * 1000L);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        setTcpPortProps();
        setPropSafe("ctl.start", "adbd");
        try {
            Thread.sleep(cfg.adbdSleepSec * 1000L);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
`
  );

  javaDir.file(
    'NexusADBWatchdogService.java',
    `package com.nexus.adbwatchdog;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

public class NexusADBWatchdogService extends Service {

    public static final String TAG = "NexusADBWatchdog";
    private static final int NOTIF_ID = 2401;

    private HandlerThread workerThread;
    private Handler workerHandler;
    private WatchdogEngine engine;
    private WatchdogConfig config;
    private PowerManager.WakeLock wakeLock;
    private OobRecoveryServer oobServer;
    private boolean running;

    private final Runnable tickRunnable = new Runnable() {
        @Override
        public void run() {
            if (!running) {
                return;
            }
            try {
                if (engine != null) {
                    engine.runOnce();
                }
            } catch (Throwable t) {
                Log.e(TAG, "watchdog tick failed", t);
            }
            if (running && workerHandler != null) {
                workerHandler.postDelayed(this, Math.max(1, config.intervalSec) * 1000L);
            }
        }
    };

    public static void start(Context ctx) {
        Intent i = new Intent(ctx, NexusADBWatchdogService.class);
        i.setAction(WatchdogConfig.ACTION_START);
        ctx.startService(i);
    }

    public static void stop(Context ctx) {
        Intent i = new Intent(ctx, NexusADBWatchdogService.class);
        i.setAction(WatchdogConfig.ACTION_STOP);
        ctx.startService(i);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        config = new WatchdogConfig();
        engine = new WatchdogEngine(this, config);
        workerThread = new HandlerThread("nexus-adb-watchdog");
        workerThread.start();
        workerHandler = new Handler(workerThread.getLooper());

        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm != null) {
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nexusadbwatchdog:svc");
            wakeLock.setReferenceCounted(false);
        }

        try {
            oobServer = new OobRecoveryServer(WatchdogConfig.OOB_PORT, engine.getRecoveryEngine(), engine.getLogger());
            oobServer.start();
        } catch (Throwable t) {
            Log.e(TAG, "Failed to start OobRecoveryServer on port " + WatchdogConfig.OOB_PORT, t);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent != null ? intent.getAction() : WatchdogConfig.ACTION_START;

        if (WatchdogConfig.ACTION_STOP.equals(action)) {
            Log.w(TAG, "STOP ignored — Watchdog runs permanently");
            startWatchdog();
            return START_STICKY;
        }

        if (WatchdogConfig.ACTION_INJECT.equals(action) && intent != null && engine != null) {
            String cmd = intent.getStringExtra(WatchdogConfig.EXTRA_INJECT);
            if (cmd != null) {
                engine.requestInject(cmd);
            }
        }

        startWatchdog();
        return START_STICKY;
    }

    private void startWatchdog() {
        if (running) {
            return;
        }
        running = true;
        if (wakeLock != null && !wakeLock.isHeld()) {
            wakeLock.acquire();
        }
        startForegroundCompat();
        workerHandler.removeCallbacks(tickRunnable);
        workerHandler.post(tickRunnable);
        HomeGuard.scheduleBootHomeLaunch(this);
        Log.i(TAG, "Watchdog Service started");
    }

    private void stopWatchdog() {
        running = false;
        if (workerHandler != null) {
            workerHandler.removeCallbacks(tickRunnable);
        }
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
        }
        stopForeground(true);
        Log.i(TAG, "Watchdog Service stopped");
    }

    private void startForegroundCompat() {
        Intent open = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open, 0);
        Notification.Builder b = new Notification.Builder(this)
                .setContentTitle("Nexus ADB Watchdog 2.4")
                .setContentText(getString(R.string.service_running))
                .setSmallIcon(android.R.drawable.ic_lock_idle_lock)
                .setContentIntent(pi)
                .setOngoing(true);
        startForeground(NOTIF_ID, b.getNotification());
    }

    @Override
    public void onDestroy() {
        stopWatchdog();
        if (oobServer != null) {
            oobServer.stop();
            oobServer = null;
        }
        if (workerThread != null) {
            if (Build.VERSION.SDK_INT >= 18) {
                workerThread.quitSafely();
            } else {
                workerThread.quit();
            }
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
`
  );

  javaDir.file(
    'BootReceiver.java',
    `package com.nexus.adbwatchdog;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }
        String action = intent.getAction();
        Log.i(NexusADBWatchdogService.TAG, "Trigger intent received: " + action);

        if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            HomeGuard.scheduleBootHomeLaunch(context);
        }

        NexusADBWatchdogService.start(context.getApplicationContext());
    }
}
`
  );

  javaDir.file(
    'HomeGuard.java',
    `package com.nexus.adbwatchdog;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.List;

public final class HomeGuard {

    private static final String TAG = "NexusHomeGuard";
    public static final String TARGET_PACKAGE = "com.join";
    public static final String TARGET_ACTIVITY_DEFAULT = "com.join.ui.MainActivity";

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());

    private HomeGuard() {
    }

    public static synchronized void scheduleBootHomeLaunch(final Context context) {
        if (context == null) return;
        final Context appCtx = context.getApplicationContext();

        launchTargetHome(appCtx);

        int[] delaysMs = new int[]{1200, 3000, 6500, 11000};
        for (final int delay : delaysMs) {
            sMainHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    launchTargetHome(appCtx);
                }
            }, delay);
        }
    }

    public static boolean launchTargetHome(Context context) {
        if (context == null) return false;

        PackageManager pm = context.getPackageManager();
        if (pm == null) return false;

        try {
            Intent homeIntent = new Intent(Intent.ACTION_MAIN);
            homeIntent.addCategory(Intent.CATEGORY_HOME);
            homeIntent.setPackage(TARGET_PACKAGE);
            homeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

            List<ResolveInfo> homes = pm.queryIntentActivities(homeIntent, PackageManager.MATCH_DEFAULT_ONLY);
            if (homes != null && !homes.isEmpty()) {
                ResolveInfo ri = homes.get(0);
                if (ri.activityInfo != null) {
                    homeIntent.setComponent(new ComponentName(ri.activityInfo.packageName, ri.activityInfo.name));
                    context.startActivity(homeIntent);
                    Log.i(TAG, "HomeGuard: Successfully launched " + ri.activityInfo.packageName + "/" + ri.activityInfo.name + " as HOME");
                    return true;
                }
            }

            try {
                Intent explicitIntent = new Intent(Intent.ACTION_MAIN);
                explicitIntent.addCategory(Intent.CATEGORY_HOME);
                explicitIntent.setComponent(new ComponentName(TARGET_PACKAGE, TARGET_ACTIVITY_DEFAULT));
                explicitIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                context.startActivity(explicitIntent);
                Log.i(TAG, "HomeGuard: Explicit launch " + TARGET_PACKAGE + "/" + TARGET_ACTIVITY_DEFAULT);
                return true;
            } catch (Throwable ignored) {
            }

            Intent launchIntent = pm.getLaunchIntentForPackage(TARGET_PACKAGE);
            if (launchIntent != null) {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                context.startActivity(launchIntent);
                Log.i(TAG, "HomeGuard: Started " + TARGET_PACKAGE + " via getLaunchIntentForPackage");
                return true;
            }

            Log.w(TAG, "HomeGuard: Target package " + TARGET_PACKAGE + " not found or no launchable activity");
        } catch (Throwable t) {
            Log.e(TAG, "HomeGuard: Failed to launch target app", t);
        }

        return false;
    }
}
`
  );

  // PC C# Out-of-band Recovery Integration Helper Folder
  const csharpDir = zip.folder('PC_Client_CSharp')!;
  csharpDir.file(
    'NexusAdbOobClient.cs',
    `using System;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace NexusAdbClient
{
    /// <summary>
    /// Out-of-Band (OOB) ADB Recovery Client for Windows C# Software.
    /// Solves the 20h-30h Android 5.1.1 ADB Transport Offline zombie freeze.
    /// </summary>
    public static class NexusAdbOobClient
    {
        public const int DEFAULT_OOB_PORT = 5556;
        public const int DEFAULT_ADB_PORT = 5555;

        /// <summary>
        /// Ensures ADB connection is healthy. If offline/disconnected, executes dual-phase recovery:
        /// Phase 1: Soft reconnect (adb disconnect -> adb connect)
        /// Phase 2: OOB UDP Recovery (sends RECOVER_ADBD to port 5556, waits for ACK, then reconnects)
        /// </summary>
        public static async Task<bool> EnsureConnectedAsync(string deviceIp, int timeoutMs = 8000)
        {
            // 1. Check current ADB state
            string status = GetAdbDeviceStatus(deviceIp);
            if (status == "device")
            {
                return true; // Already healthy
            }

            Console.WriteLine($"[AdbManager] Device {deviceIp} status is '{status}'. Attempting Phase 1 soft reconnect...");

            // Phase 1: Try soft reconnect
            RunAdbCommand($"disconnect {deviceIp}:{DEFAULT_ADB_PORT}");
            await Task.Delay(800);
            RunAdbCommand($"connect {deviceIp}:{DEFAULT_ADB_PORT}");
            await Task.Delay(1200);

            status = GetAdbDeviceStatus(deviceIp);
            if (status == "device")
            {
                Console.WriteLine($"[AdbManager] Phase 1 reconnect succeeded.");
                return true;
            }

            Console.WriteLine($"[AdbManager] Phase 1 failed (status: '{status}'). Initiating Phase 2 Out-Of-Band (OOB) Recovery via UDP {DEFAULT_OOB_PORT}...");

            // Phase 2: Trigger Out-Of-Band recovery
            bool oobAck = await SendOobRecoveryCommandAsync(deviceIp, DEFAULT_OOB_PORT, 2500);
            if (oobAck)
            {
                Console.WriteLine($"[AdbManager] Received ACK from Android Watchdog. Waiting 3.5s for adbd reset...");
            }
            else
            {
                Console.WriteLine($"[AdbManager] No UDP ACK received (possible broadcast send). Waiting 3.5s anyway...");
            }

            await Task.Delay(3500);

            // Phase 3: Final Reconnect & Real Verification
            RunAdbCommand($"disconnect {deviceIp}:{DEFAULT_ADB_PORT}");
            await Task.Delay(500);
            RunAdbCommand($"connect {deviceIp}:{DEFAULT_ADB_PORT}");
            await Task.Delay(1500);

            status = GetAdbDeviceStatus(deviceIp);
            bool success = (status == "device");
            Console.WriteLine($"[AdbManager] Final Verification: {deviceIp} => {status} (Success: {success})");
            return success;
        }

        /// <summary>
        /// Sends UDP "RECOVER_ADBD" packet to Android Watchdog on port 5556.
        /// </summary>
        public static async Task<bool> SendOobRecoveryCommandAsync(string ip, int port = DEFAULT_OOB_PORT, int timeoutMs = 2500)
        {
            try
            {
                using (var udp = new UdpClient())
                {
                    udp.Client.SendTimeout = timeoutMs;
                    udp.Client.ReceiveTimeout = timeoutMs;

                    byte[] cmd = Encoding.UTF8.GetBytes("RECOVER_ADBD\\n");
                    var endpoint = new IPEndPoint(IPAddress.Parse(ip), port);

                    await udp.SendAsync(cmd, cmd.Length, endpoint);

                    var receiveTask = udp.ReceiveAsync();
                    var timeoutTask = Task.Delay(timeoutMs);

                    var completed = await Task.WhenAny(receiveTask, timeoutTask);
                    if (completed == receiveTask)
                    {
                        var result = receiveTask.Result;
                        string ack = Encoding.UTF8.GetString(result.Buffer);
                        return ack.Contains("ACK:RECOVER_ADBD");
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[OOB] UDP Send error: {ex.Message}");
            }
            return false;
        }

        public static string GetAdbDeviceStatus(string deviceIp)
        {
            string output = RunAdbCommand("devices");
            foreach (var line in output.Split(new[] { '\\r', '\\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                if (line.Contains(deviceIp))
                {
                    if (line.Contains("device")) return "device";
                    if (line.Contains("offline")) return "offline";
                    if (line.Contains("unauthorized")) return "unauthorized";
                }
            }
            return "disconnected";
        }

        private static string RunAdbCommand(string args)
        {
            try
            {
                var psi = new ProcessStartInfo("adb", args)
                {
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
                using (var p = Process.Start(psi))
                {
                    p.WaitForExit(3000);
                    return p.StandardOutput.ReadToEnd();
                }
            }
            catch (Exception ex)
            {
                return ex.Message;
            }
        }
    }
}
`
  );

  csharpDir.file(
    'test_oob_recover.bat',
    `@echo off\r\nREM test_oob_recover.bat — Test UDP 5556 Out-of-band recovery from Windows command line\r\nset IP=192.168.31.11\r\nset PORT=5556\r\necho Sending OOB recovery packet to %IP%:%PORT%...\r\npowershell -NoProfile -ExecutionPolicy Bypass -Command "$u=New-Object System.Net.Sockets.UdpClient; $u.Client.ReceiveTimeout=3000; $b=[System.Text.Encoding]::UTF8.GetBytes('RECOVER_ADBD\`n'); $ep=New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Parse('%IP%'), %PORT%); [void]$u.Send($b, $b.Length, $ep); Write-Host 'Sent RECOVER_ADBD. Waiting for ACK...'; try { $rep=New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0); $rcv=$u.Receive([ref]$rep); Write-Host 'Received ACK: ' ([System.Text.Encoding]::UTF8.GetString($rcv)) } catch { Write-Host 'No response (or firewall blocked UDP reply)' }; $u.Close()"\r\necho.\r\necho Now checking adb status...\r\ntimeout /t 3 /nobreak >nul\r\nadb disconnect %IP%:5555\r\nadb connect %IP%:5555\r\nadb devices\r\n`
  );

  // Generate ZIP blob and trigger browser download
  const blob = await zip.generateAsync({ type: 'blob' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'APKNexusADBWatchdog-2.4-OOBFix.zip';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
