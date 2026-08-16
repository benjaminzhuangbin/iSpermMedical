# Nexus ADB Watchdog — Android APK Project 2.4

Package: `com.nexus.adbwatchdog`  
APK: `release/NexusADBWatchdog.apk`  
Target: Rockchip RK3288 / **Android 5.1.1 (API 22)**  
IDE: **Android Studio Arctic Fox 2020.3.1 Patch 2**

Faithful APK port of native **Nexus ADB Watchdog 2.4** (fault recovery only).

---

## Critical: how the APK gets real root (UID 0)

### Confirmed firmware fact

```text
adb shell "su -c id"     → uid=0(root)          # shell UID allowed
APK uid 10053 + stock su → "uid 10053 not allowed to su"
```

Stock `/system/xbin/su` is setuid, SELinux Permissive, but **rejects app UIDs**.
`sh -c "su -c …"` does **not** bypass that check.

### Product solution (one APK)

Factory installs a dedicated setuid helper shipped with this project:

```text
/system/xbin/nexus_su   root:root  mode 06755
```

APK then executes:

```text
/system/xbin/nexus_su -c id  →  uid=0(root)
```

Status must show:

```text
ROOT_OK=1
ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su
ROOT_UID=0
```

Details: [`docs/ROOT_SOLUTION.md`](docs/ROOT_SOLUTION.md)

**Do not** treat `adb shell su` success as APK root. Only APK-path `id` → `uid=0(root)` counts.

---

## Factory install (manufacturing once — not hospital user)

Hospital users: power on + Ethernet only. No USB, no Magisk, no second APK.

Engineering / firmware flash (USB or Ethernet adb **once**):

```bat
cd APKNexusADBWatchdog-2.4
factory\install_system.bat
adb reboot
```

What it does:

1. Copies `factory/nexus_su` → `/system/xbin/nexus_su` (setuid 6755)
2. Installs APK as `/system/priv-app/NexusADBWatchdog/`
3. Requires reboot

Verify after reboot:

```bat
adb shell "cat /sdcard/NexusADBWatchdog/watchdog.status"
adb shell "grep ROOT /sdcard/NexusADBWatchdog/watchdog.log"
adb shell "/system/xbin/nexus_su -c id"
```

Expect `ROOT_OK=1` / `ROOT_UID=0`.

### Plain `adb install` without nexus_su

Will still show `ROOT_OK=0` / `FAIL_REASON=NO_ROOT` / `NEED_FACTORY_NEXUS_SU`.
That is expected and honest — stock `su` cannot elevate this app UID.

---

## Log folder

```text
/sdcard/NexusADBWatchdog/
  watchdog.status
  watchdog.log
```

---

## Permanent auto-start

- `Application.onCreate` starts Service
- Opening APK starts Service
- `BOOT_COMPLETED` starts Service
- Stop is disabled (permanent run)

Main APP:

```java
Intent i = new Intent();
i.setComponent(new ComponentName(
        "com.nexus.adbwatchdog",
        "com.nexus.adbwatchdog.NexusADBWatchdogService"));
i.setAction("com.nexus.adbwatchdog.action.START");
context.startService(i);
```

---

## Product hard rules (native 2.4 — unchanged)

1. Fault recovery **only** — never periodic adbd restart.
2. `ESTABLISHED > 0` → **never** restart/stop adbd.
3. `ESTABLISHED = 0` → **not** a fault.
4. **No** localhost CNXN probe.
5. CLOSE_WAIT is diagnostic only — not a recovery trigger.
6. Rate limit window — **never** permanent disable.

| Case | Condition | Action |
|------|-----------|--------|
| A | ADBD=NO | setprop TCP 5555 + `ctl.start adbd` |
| B | ADBD=YES, PORT5555=NO | setprop + stop + sleep + start adbd |

---

## Build (Android Studio Arctic Fox)

1. Open folder `APKNexusADBWatchdog-2.4`.
2. `local.properties` → `sdk.dir=...`
3. Optional rebuild helper: `native/nexus_su/build.sh` (NDK r21e, armeabi-v7a).
4. `Build → Build APK(s)` or:

```bat
gradlew.bat assembleDebug
copy app\build\outputs\apk\debug\app-debug.apk release\NexusADBWatchdog.apk
```

---

## Verify root (must be APK path)

```bat
adb shell "cat /sdcard/NexusADBWatchdog/watchdog.status"
```

Required:

```text
ROOT_OK=1
ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su
ROOT_UID=0
```

Optional direct proof of helper:

```bat
adb shell "/system/xbin/nexus_su -c id"
```

---

## Verify ADB recovery (Ethernet, no USB)

1. Factory-install APK + `nexus_su`; reboot; Ethernet only.
2. PC: `adb connect 192.168.31.11:5555` / QtScrcpy — must stay healthy; **RESTART_COUNT** must not climb.
3. Inject CASE A (test UI or):

```bat
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.INJECT --es inject_cmd STOP_ADBD
```

Expect recovery → ADBD=YES / PORT5555=YES / device back.

4. Inject CASE B: `BREAK_PORT` → RESTART_ADBD → 5555 listening again.

---

## Does this APK have Ethernet TCP ADB auto-recovery?

| Condition | Answer |
|-----------|--------|
| After factory install of `nexus_su` + APK, `ROOT_OK=1` | **Yes** — Watchdog 2.4 recovery runs as real root |
| Only `adb install` as data app, no `nexus_su` | **No** — stock `su` blocks app UID; status stays `NO_ROOT` |

---

## Project layout

```text
APKNexusADBWatchdog-2.4/
  app/src/main/java/com/nexus/adbwatchdog/   # Service + 2.4 engine
  app/src/main/assets/native/armeabi-v7a/nexus_su
  native/nexus_su/                           # setuid helper source
  factory/install_system.bat                 # manufacturing install
  factory/install_on_device.sh
  factory/nexus_su
  release/NexusADBWatchdog.apk
  docs/ROOT_SOLUTION.md
  docs/MAIN_APP_INTEGRATION.md
```
