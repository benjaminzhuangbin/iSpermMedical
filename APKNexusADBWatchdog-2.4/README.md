# Nexus ADB Watchdog — Android APK Project 2.4

Package: `com.nexus.adbwatchdog`  
APK name: `NexusADBWatchdog.apk`  
Target: Rockchip RK3288 / **Android 5.1.1 (API 22)**  
IDE: **Android Studio Arctic Fox 2020.3.1 Patch 2**

Faithful APK port of native **Nexus ADB Watchdog 2.4** (fault recovery only).

---

## Product hard rules (same as native 2.4)

1. Fault recovery **only** — never periodic adbd restart.
2. `ESTABLISHED > 0` (live QtScrcpy/PC) → **never** restart/stop adbd.
3. `ESTABLISHED = 0` / no PC client → **not** a fault.
4. **No** localhost CNXN probe (removed; caused 2.3 QtScrcpy drops).
5. **CLOSE_WAIT is not a recovery trigger** in this APK (diagnostic fields only).
6. Cooldown is post-fault only; if healthy → clear recovery state immediately.
7. Rate limit window (`max_restart=3` / `600s`) — **never** permanent disable.

Recovery cases:

| Case | Condition | Action |
|------|-----------|--------|
| A | ADBD=NO | setprop TCP 5555 + `ctl.start adbd` |
| B | ADBD=YES, PORT5555=NO | setprop + stop + sleep + start adbd |

---

## Open & build (Android Studio Arctic Fox)

1. Install Android Studio Arctic Fox 2020.3.1 Patch 2.
2. SDK Manager: install **Android SDK Platform 30** (compile) and ensure **API 22** platform is available for device.
3. `File → Open` → select folder `APKNexusADBWatchdog-2.4`.
4. Create `local.properties`:

```properties
sdk.dir=C:\\Users\\YOU\\AppData\\Local\\Android\\Sdk
```

5. Gradle Sync (project uses AGP **7.0.4** + Gradle **7.0.2**).
6. `Build → Build Bundle(s) / APK(s) → Build APK(s)`.

Debug APK output:

```
app/build/outputs/apk/debug/app-debug.apk
```

Rename/copy to `NexusADBWatchdog.apk` for deployment if desired.

Release:

```
Build → Generate Signed Bundle / APK → APK
```

Or CLI (with SDK + JDK 11 recommended for AGP 7):

```bat
gradlew.bat assembleDebug
gradlew.bat assembleRelease
```

---

## Install on device (root RK3288)

```bat
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

Grant root to the app when Magisk/SuperSU prompts (required).

---

## Start / stop Watchdog Service

### From Watchdog APK UI

Open **Nexus ADB Watchdog** → **Start**.

### From adb

```bat
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.START
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.STOP
```

### From your Main APP (Java)

```java
Intent i = new Intent();
i.setComponent(new ComponentName(
        "com.nexus.adbwatchdog",
        "com.nexus.adbwatchdog.NexusADBWatchdogService"));
i.setAction("com.nexus.adbwatchdog.action.START");
context.startService(i);
```

Stop:

```java
i.setAction("com.nexus.adbwatchdog.action.STOP");
context.startService(i);
```

Explicit package/class (also valid):

```java
Intent i = new Intent();
i.setClassName("com.nexus.adbwatchdog", "com.nexus.adbwatchdog.NexusADBWatchdogService");
i.setAction("com.nexus.adbwatchdog.action.START");
startService(i);
```

---

## Boot auto-start

`BootReceiver` listens for `BOOT_COMPLETED` and starts `NexusADBWatchdogService`.

Verify after reboot:

```bat
adb shell dumpsys activity services com.nexus.adbwatchdog
adb shell "run-as com.nexus.adbwatchdog cat /data/data/com.nexus.adbwatchdog/files/watchdog.status"
```

If `run-as` is blocked on production builds, use root:

```bat
adb shell su -c "cat /data/data/com.nexus.adbwatchdog/files/watchdog.status"
adb shell su -c "cat /data/data/com.nexus.adbwatchdog/files/watchdog.log"
```

---

## Check running / status / log

```bat
adb shell dumpsys activity services | findstr nexus
adb shell su -c "cat /data/data/com.nexus.adbwatchdog/files/watchdog.status"
adb shell su -c "tail -n 80 /data/data/com.nexus.adbwatchdog/files/watchdog.log"
```

Status first line must be:

```
Nexus ADB Watchdog 2.4
```

Healthy while QtScrcpy connected example:

```
ADBD=YES
PORT5555=YES
ESTABLISHED=1
CLIENT_STATE=CONNECTED
TCP_HEALTH=OK
ADB_HEALTH=OK
FAIL_REASON=NONE
LAST_ACTION=NONE
RESTART_COUNT=0
```

---

## Test inject (DEVELOPMENT / TEST ONLY)

Use MainActivity buttons, or:

```bat
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.INJECT --es inject_cmd STOP_ADBD
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.INJECT --es inject_cmd BREAK_PORT
```

Expected:

- **STOP_ADBD** → ADBD=NO → `START_ADBD` → ADBD=YES / PORT5555=YES
- **BREAK_PORT** → PORT5555=NO → `RESTART_ADBD` → PORT5555=YES

Production must not auto-run these tests.

---

## Ethernet-only (no USB) verification

1. Install APK over USB once; enable root for the app.
2. Disconnect USB; keep Ethernet (`192.168.31.11`).
3. Boot device; confirm Service starts (`BOOT_COMPLETED`) or start from Main APP.
4. From PC: `adb connect 192.168.31.11:5555` / QtScrcpy.
5. Confirm status stays healthy and **RESTART_COUNT** does not climb while mirror works.

---

## Project layout

```
APKNexusADBWatchdog-2.4/
  app/src/main/java/com/nexus/adbwatchdog/
    MainActivity.java
    NexusADBWatchdogService.java
    BootReceiver.java
    WatchdogEngine.java
    RecoveryEngine.java
    TcpNetProbe.java
    RootShell.java
    WatchdogStatus.java
    WatchdogConfig.java
    WatchdogLogger.java
    StatusStore.java
  app/src/main/AndroidManifest.xml
  build.gradle / settings.gradle / gradle.properties
  README.md
  docs/MAIN_APP_INTEGRATION.md
```

Files live under app private storage:

`/data/data/com.nexus.adbwatchdog/files/watchdog.status`  
`/data/data/com.nexus.adbwatchdog/files/watchdog.log`

**Not** `/data/local/watchdog/` (that path is for the native binary product).

---

## Architecture

```
MyMainApp.apk
    startService(Intent)
        ↓
NexusADBWatchdog.apk  (com.nexus.adbwatchdog)
    NexusADBWatchdogService  (every ~5s)
        ↓
    RootShell (su)
        ↓
    adbd / persist.adb.tcp.port / /proc/net/tcp
```

---

## Known requirements

- Device must be **rooted**; without `su`, status shows `ROOT_OK=0` / `FAIL_REASON=NO_ROOT`.
- Android 5.1.1 Service uses sticky + partial wake lock + foreground notification.
- Do not reintroduce 2.3 CNXN probe logic.
