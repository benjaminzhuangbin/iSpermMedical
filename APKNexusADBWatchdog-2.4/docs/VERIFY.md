# Verification checklist — Nexus ADB Watchdog APK 2.4

## 1. Factory install (engineering once)

```bat
cd APKNexusADBWatchdog-2.4
factory\install_system.bat
adb reboot
```

## 2. Prove APK obtained UID 0

```bat
adb shell "cat /sdcard/NexusADBWatchdog/watchdog.status"
adb shell "grep ROOT /sdcard/NexusADBWatchdog/watchdog.log"
```

Must contain:

```text
ROOT_OK=1
ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su
ROOT_UID=0
```

Helper sanity (not a substitute for APK proof):

```bat
adb shell "/system/xbin/nexus_su -c id"
```

Expect `uid=0(root)`.

## 3. Healthy Ethernet + QtScrcpy (must NOT restart)

1. Disconnect USB; Ethernet only (`192.168.31.11`).
2. PC: QtScrcpy / `adb connect 192.168.31.11:5555`.
3. Watch `watchdog.status` for several minutes:

```text
ADBD=YES
PORT5555=YES
ESTABLISHED>0
FAIL_REASON=NONE
LAST_ACTION=NONE
RESTART_COUNT=0   (must not climb)
```

## 4. CASE A recovery

```bat
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.INJECT --es inject_cmd STOP_ADBD
```

Expect: ADBD recovers, `192.168.31.11:5555` → `device`, QtScrcpy usable again.

## 5. CASE B recovery

```bat
adb shell am startservice -n com.nexus.adbwatchdog/.NexusADBWatchdogService -a com.nexus.adbwatchdog.action.INJECT --es inject_cmd BREAK_PORT
```

Expect: PORT5555 back to YES after RESTART_ADBD.

## 6. Capability statement

| Setup | Ethernet TCP ADB auto-recovery |
|-------|--------------------------------|
| Factory `nexus_su` + APK, `ROOT_OK=1` | **YES** |
| Data-only `adb install`, no setuid helper | **NO** (`NO_ROOT`) |
