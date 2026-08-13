# Nexus ADB Watchdog Version 2.0

Native C daemon for Android 5.1.1 (Rockchip RK3288 / ARMv7).

**Version 2.0 — TCP ADB automatic recovery** (rate-limited).

Built on Version 1.5 diagnostics, and adds safe auto-recovery when `adbd`
or TCP port `5555` becomes unhealthy.

---

## Project path

```
NexusADBWatchdog-2.0/
```

Prebuilt binary:

```
NexusADBWatchdog-2.0/release/watchdog
```

Device install path:

```
/data/local/watchdog/
```

---

## What it does

Every cycle (default **5 seconds**):

1. Check `/sbin/adbd` process  
2. Check TCP port **5555** listening (`/proc/net/tcp`)  
3. Count **ESTABLISHED** clients on 5555  
4. Update `watchdog.status` / append `watchdog.log`  
5. If unhealthy → run recovery (with rate limit)

### Recovery cases

| Case | Condition | Action |
|------|-----------|--------|
| A | `adbd` missing | `setprop` TCP port + `ctl.start adbd` |
| B | port 5555 not listening | `setprop persist.adb.tcp.port 5555` → stop adbd → sleep 3s → start adbd |
| C | adbd+port OK, but `ESTABLISHED=0` for **60s** | same restart as B |

### Safety (anti loop)

```
max_restart=3
restart_window_sec=600   # 10 minutes
```

If more than 3 recoveries occur inside 10 minutes:

* auto-recovery **stops**
* writes `/data/local/watchdog/watchdog.error`
* `LAST_ACTION=RECOVERY_DISABLED`

---

## Configuration (`watchdog.conf`)

```
interval=5
log_rotate_mb=10
status_update=1
recovery_enable=1
no_client_timeout=60
max_restart=3
restart_window_sec=600
adbd_sleep_sec=3
```

Set `recovery_enable=0` to monitor only (like 1.5).

---

## Status file

`/data/local/watchdog/watchdog.status`

```
TIME=2026-08-01 12:00:00
ADBD=YES
PORT5555=YES
ESTABLISHED=1
CLIENT=192.168.31.100
LAST_ACTION=NONE
```

`LAST_ACTION` examples:

* `NONE`
* `START_ADBD`
* `RESTART_ADBD`
* `RECOVERY_DISABLED`

---

## Log file

`/data/local/watchdog/watchdog.log` (append-only, rotates at 10 MB)

Example:

```
2026-08-01 12:00:01
ADBD OK
PORT 5555 OK
TCP CLIENT NONE
ESTABLISHED=0
LAST_ACTION=NONE

2026-08-01 12:01:05
TCP CLIENT NONE
TCP timeout
restart adbd
```

---

## Build (Windows)

Requires Android NDK + CMake.

```bat
set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\<version>
cd NexusADBWatchdog-2.0
build.bat
```

Target: `armeabi-v7a` / `android-22`.

---

## Install / Start

```bat
cd NexusADBWatchdog-2.0
install.bat
adb shell "nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &"
```

Or manually:

```bat
adb push release\watchdog /data/local/watchdog/
adb shell
chmod 755 /data/local/watchdog/watchdog
nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &
```

Check:

```bat
adb shell "cat /data/local/watchdog/watchdog.status"
adb shell "tail -n 50 /data/local/watchdog/watchdog.log"
```

Uninstall:

```bat
uninstall.bat
```

---

## Test plan

### Test 1 — Normal start

1. Install + start watchdog  
2. On PC: `adb connect 192.168.31.11:5555`  
3. Confirm `adb devices` shows `192.168.31.11:5555 device`  
4. Confirm status: `ADBD=YES`, `PORT5555=YES`, `CLIENT=192.168.31.100`

### Test 2 — Disconnect TCP

1. `adb disconnect`  
2. Watch log: `TCP CLIENT NONE`  
3. After `no_client_timeout` (60s), watchdog may restart adbd  
4. Reconnect from PC and confirm recovery

### Test 3 — Stop adbd

On Android (root):

```sh
stop adbd
```

Expect watchdog Case A/B recovery (`START_ADBD` / `RESTART_ADBD`), then:

```bat
adb connect 192.168.31.11:5555
```

### Test 4 — Android reboot (Ethernet only)

1. Reboot Android with only Ethernet connected  
2. Start watchdog (or have medical app launch it)  
3. Confirm TCP ADB becomes usable again for the Windows PC

### Rate-limit check

Force multiple recoveries quickly; after 3 within 10 minutes, confirm:

* `watchdog.error` exists  
* `LAST_ACTION=RECOVERY_DISABLED`  
* no further adbd restarts

---

## Notes for C# WinForms integration

Watchdog 2.0 does **not** manage scrcpy / file transfer / Windows adb UI.

Suggested host flow:

1. C# starts  
2. Check TCP ADB  
3. If abnormal → ensure watchdog is running / wait for recovery  
4. Re-check ADB  
5. Start scrcpy

---

## Network

| Host | IP |
|------|----|
| Android RK3288 | `192.168.31.11` |
| Windows PC | `192.168.31.100` |
| ADB TCP | `5555` |
