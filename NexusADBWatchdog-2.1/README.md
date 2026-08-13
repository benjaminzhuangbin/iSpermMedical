# Nexus ADB Watchdog Version 2.1

Industrial TCP ADB watchdog for **Rockchip RK3288 / Android 5.1.1 (root)**.

**Goal:** Keep Android ADB TCP (`192.168.31.11:5555`) available long-term over Ethernet.  
Watchdog does **not** run scrcpy / C# / UI. It only keeps the ADB TCP **service** healthy.

---

## Project path (GitHub)

```
NexusADBWatchdog-2.1/
```

Prebuilt binary:

```
NexusADBWatchdog-2.1/release/watchdog
```

**Installs on Android:** `/data/local/watchdog/`  
Windows scripts are deploy/control tools only.

---

## What changed from 2.0 → 2.1

| 2.0 problem | 2.1 fix |
|-------------|---------|
| `ESTABLISHED=0` after 60s restarted adbd | **Removed.** No client = normal |
| `max_restart=3` permanently disabled recovery | **Cooldownoldown 60s then auto-resume** |
| Confusing `RECOVERY_DISABLED` forever | `RECOVERY_COOLDOWN` → `RECOVERY_RESUMED` |
| Log spam every 5s | Log on change/recovery + optional heartbeat |

---

## Health checks (every ~5s)

1. `/sbin/adbd` process exists  
2. TCP `5555` LISTEN (`/proc/net/tcp`, not netstat)  
3. `persist.adb.tcp.port == 5555`  

**Not a fault:** `ESTABLISHED=0` / empty `CLIENT=`

Normal idle status:

```
ADBD=YES
PORT5555=YES
ESTABLISHED=0
CLIENT=
LAST_ACTION=NONE
RECOVERY_ENABLED=1
```

---

## Recovery cases

| Case | Condition | Action |
|------|-----------|--------|
| A | adbd missing | setprop + `ctl.start=adbd` |
| B | 5555 not LISTEN | setprop + stop + sleep + start |
| C | `persist.adb.tcp.port` ≠ 5555 | setprop + restart adbd |

Rate limit: `max_restart=3` / `restart_window_sec=600` → **cooldown** `cooldown_sec=60` → auto resume.  
**Never** permanently disables recovery.

---

## Configuration

`watchdog.conf`:

```
interval=5
log_rotate_mb=10
status_update=1
recovery_enable=1
max_restart=3
restart_window_sec=600
cooldown_sec=60
adbd_sleep_sec=3
log_heartbeat_sec=300
```

---

## Build (Windows)

```bat
set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\<version>
cd NexusADBWatchdog-2.1
build.bat
```

Target: `armeabi-v7a` / API 22.

---

## Install / Start / Status (Windows → Android)

```bat
cd NexusADBWatchdog-2.1
install.bat
start.bat
status.bat
```

Manual equivalent:

```bat
adb push release\watchdog /data/local/watchdog/
adb shell "chmod 755 /data/local/watchdog/watchdog"
adb shell "nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &"
adb shell "cat /data/local/watchdog/watchdog.status"
```

Stop / uninstall:

```bat
stop.bat
uninstall.bat
```

---

## Safe test inject (no Android reboot)

Watchdog reads one-shot file `/data/local/watchdog/watchdog.inject` then deletes it:

| Command | Effect |
|---------|--------|
| `STOP_ADBD` | stop adbd (CASE A) |
| `BREAK_PORT` | clear TCP port + restart (CASE B/C) |
| `CLEAR_TCP_PORT` | clear persist port (CASE C) |

Helpers:

```bat
test_case_a.bat
test_case_b.bat
test_case_c.bat
```

---

## Full test plan

### TEST 1 — Idle normal (no PC client)

```bat
start.bat
status.bat
```

**Expected:**

```
ADBD=YES
PORT5555=YES
ESTABLISHED=0
LAST_ACTION=NONE
RECOVERY_ENABLED=1
```

No adbd restart. Wait 90s — still no restart.

### TEST 2 — adb connect

```bat
adb connect 192.168.31.11:5555
adb devices
status.bat
```

**Expected:** `ESTABLISHED=1`, `CLIENT=192.168.31.100` (or PC IP)

### TEST 3 — adb disconnect (critical)

```bat
adb disconnect 192.168.31.11:5555
```

Wait **≥ 90 seconds**, then `status.bat`.

**Expected:**

- `ESTABLISHED=0`
- `RESTART_COUNT` **unchanged**
- `LAST_ACTION=NONE` (or not a new restart)
- `ADBD_PID` **unchanged**
- **No** adbd restart

### TEST 4 — 5555 listener fault

```bat
test_case_b.bat
```

Wait ~15s, `status.bat`.

**Expected:** recovery `RESTART_ADBD` / `FIX_TCP_PORT`, then `PORT5555=YES`

### TEST 5 — adbd missing

```bat
test_case_a.bat
```

Wait ~10s, `status.bat`.

**Expected:** `START_ADBD`, then `ADBD=YES` `PORT5555=YES`

### TEST 6 — cooldown auto-resume (not permanent disable)

Force multiple recoveries (repeat TEST 4/5). After 3 in window:

**Expected:** `LAST_ACTION=RECOVERY_COOLDOWN`, `RECOVERY_ENABLED=0`, `COOLDOWN>0`

Wait `cooldown_sec` (60s):

**Expected:** `RECOVERY RESUMED` in log, `RECOVERY_ENABLED=1`, can recover again  
**Not** stuck forever requiring kill/reboot.

---

## Network

| Host | IP |
|------|----|
| Android | `192.168.31.11` |
| Windows PC | `192.168.31.100` |
| ADB TCP | `5555` |

---

## Deliverables

- Complete C sources + headers  
- `CMakeLists.txt`, `build.bat`  
- `release/watchdog`, `watchdog.conf`  
- `install.bat` / `uninstall.bat` / `start.bat` / `stop.bat` / `status.bat`  
- `test_case_a.bat` / `test_case_b.bat` / `test_case_c.bat`  
- This README  

Watchdog binary runs **on Android**, not on Windows.
