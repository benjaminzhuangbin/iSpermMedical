# Nexus ADB Watchdog Version 2.3

Production TCP ADB watchdog for Rockchip RK3288 / Android 5.1.1 (root).

**Core goal:** keep Ethernet TCP ADB (`192.168.31.11:5555`) healthy long-term on the Android side — including the field fault where ping/TCP OK but PC shows `adb devices … offline`.

Does **not** implement scrcpy / QtScrcpy / C# / APK / UI.

**Installs on Android:** `/data/local/watchdog/`  
Windows `*.bat` files are deploy/control tools only.

---

## Path

```
NexusADBWatchdog-2.3/
NexusADBWatchdog-2.3/release/watchdog
```

After start, status **first line** is:

```
Nexus ADB Watchdog 2.3
```

`watchdog -h` also prints Version **2.3.0**.

---

## What 2.3 changes vs 2.2

| Topic | 2.3 behavior |
|-------|----------------|
| Field fault (TCP OK, `offline`) | Layer 4 **ADB protocol health** via localhost CNXN probe |
| `ADB_HEALTH=` | `OK` / `FAULT` / `UNKNOWN` / `SKIPPED` |
| Inject | `ADB_PROTOCOL_FAULT` (sticky until recovery) |
| `ESTABLISHED=0` | Still **never** a fault (`TCP_HEALTH=NO_CLIENT`) |
| Rate limit | `max_restart` per `restart_window_sec`; window end clears count — **never permanent disable** |
| Cooldown | `cooldown_sec` after **each** recovery; then allow next attempt |
| Session heuristic | Sustained FIN_WAIT / LAST_ACK / CLOSING → `SESSION_STALE` |

---

## Detection layers

1. **ADBD process** exists  
2. **:5555 LISTEN** (`/proc/net/tcp`)  
3. **TCP abnormal** (CLOSE_WAIT threshold / FIN_WAIT stale)  
4. **ADB protocol** — connect `127.0.0.1:5555`, send ADB `CNXN`, expect `CNXN` or `AUTH`

PC `adb devices … offline` cannot be read from Android. 2.3 does **not** fake that string. It uses an Android-side heuristic that treats broken protocol/session as fault and restarts adbd so the PC can reconnect cleanly.

---

## Health model

| Field | Values |
|-------|--------|
| `TCP_HEALTH` | `OK`, `NO_CLIENT`, `FAULT` |
| `ADB_HEALTH` | `OK`, `FAULT`, `UNKNOWN`, `SKIPPED` |
| `FAIL_REASON` | `NONE`, `ADBD_NOT_RUNNING`, `PORT_NOT_LISTENING`, `TCP_PORT_PROP`, `TCP_FAULT`, `SESSION_STALE`, `ADB_PROTOCOL_FAULT` |

`ESTABLISHED=0` ⇒ `TCP_HEALTH=NO_CLIENT` — **no recovery**, **no** `RESTART_COUNT` bump.

---

## Recovery

| Case | Condition | Action |
|------|-----------|--------|
| A | adbd missing | setprop + start adbd |
| B | 5555 not LISTEN | setprop + stop + sleep + start |
| C | prop ≠ 5555 / CLOSE_WAIT / SESSION_STALE | setprop + restart adbd |
| D | `ADB_HEALTH=FAULT` | restart adbd + clear protocol inject flag |

Rate limit:

- `max_restart=3` inside `restart_window_sec=600`
- After each recovery: wait `cooldown_sec=60`
- When window expires: `RESTART_COUNT` → 0; recovery stays allowed
- **Never** latch `RECOVERY_ENABLED=0` from restart count

Success: `ADBD=YES`, `PORT5555=YES`, `ADB_HEALTH=OK` (when probe enabled), `FAIL_REASON=NONE`, `LAST_ACTION=NONE` or `RECOVERY_SUCCESS`.

---

## Build (NDK + CMake)

```bat
set ANDROID_NDK_HOME=C:\path\to\ndk
build.bat
```

Target: `armeabi-v7a`, `android-22`, Release → `release/watchdog`.

Linux CI / agent example:

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a \
  -DANDROID_PLATFORM=android-22 \
  -DANDROID_STL=none \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android --config Release
```

---

## Install / start / stop

```bat
install.bat
start.bat
status.bat
stop.bat
```

`install.bat` prints step banners (`[1/6]` … `[OK] Install complete - Version 2.3`) and runs `watchdog -h` so the version is visible.

Manual:

```bat
adb shell "nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &"
adb shell "cat /data/local/watchdog/watchdog.status"
adb shell "cat /data/local/watchdog/watchdog.log"
```

---

## Test inject

```bat
test_case_a.bat          REM STOP_ADBD
test_case_b.bat          REM BREAK_PORT
test_case_c.bat          REM CLEAR_TCP_PORT
test_case_d.bat          REM ADB_PROTOCOL_FAULT
```

Or:

```bat
adb shell "echo STOP_ADBD > /data/local/watchdog/watchdog.inject"
adb shell "echo BREAK_PORT > /data/local/watchdog/watchdog.inject"
adb shell "echo CLEAR_TCP_PORT > /data/local/watchdog/watchdog.inject"
adb shell "echo ADB_PROTOCOL_FAULT > /data/local/watchdog/watchdog.inject"
```

### Expected results

| Test | Expected |
|------|----------|
| A `STOP_ADBD` | `ADBD=NO` → `START_ADBD` → `ADBD=YES` `PORT5555=YES` `ADB_HEALTH=OK` |
| B `BREAK_PORT` | `PORT5555=NO` → `RESTART_ADBD` → `PORT5555=YES` |
| C no client | `ESTABLISHED=0` → **no** recovery, count unchanged |
| D `ADB_PROTOCOL_FAULT` | `ADB_HEALTH=FAULT` → `FIX_ADB_PROTOCOL` → healthy again; count++ |
| E 3 recoveries | `RATE_LIMIT` until window ends; `RECOVERY_ENABLED=1` always |
| F long run | Past recoveries never permanently disable recovery |

---

## Configuration (`watchdog.conf`)

| Key | Default | Meaning |
|-----|---------|---------|
| `interval` | 5 | Monitor period (seconds) |
| `recovery_enable` | 1 | Allow recovery |
| `max_restart` | 3 | Max recoveries per window |
| `restart_window_sec` | 600 | Window; then count resets |
| `cooldown_sec` | 60 | Wait after each recovery |
| `adbd_sleep_sec` | 3 | Sleep between stop/start |
| `log_heartbeat_sec` | 300 | Idle log heartbeat |
| `fault_close_wait` | 3 | CLOSE_WAIT threshold |
| `fault_hold_sec` | 30 | Hold before TCP/session recovery |
| `adb_proto_check` | 1 | Enable CNXN probe |
| `adb_proto_interval_sec` | 30 | Probe interval |
| `adb_proto_timeout_ms` | 2000 | Probe timeout |

---

## Android 5.1.1 compatibility

- Pure C / NDK / CMake; no Java / modern Android APIs  
- Uses `/proc/net/tcp`, `getprop`/`setprop` via `__system_property_*`, sockets  
- Does **not** require `wc`, `tail`, `netstat`, or busybox  
- Root required for `ctl.start` / `ctl.stop` adbd  

---

## Known limitations

1. Android cannot read Windows `adb devices` lines. Protocol probe + TCP heuristics approximate the offline-class fault.  
2. Idle ESTABLISHED PC sessions with no traffic are **not** treated as fault (avoids false restarts).  
3. Localhost CNXN probe opens a short TCP session to `:5555`; interval defaults to 30s.  
4. If PC offline is caused **only** by a broken Windows adb server while Android protocol is fine, Watchdog will correctly show healthy — fix the PC adb server / reconnect.  
5. Requires root on the device.

---

## Status fields (minimum)

```
Nexus ADB Watchdog 2.3
TIME=
ADBD_PID=
ADBD=
PORT5555=
ESTABLISHED=
CLIENT=
CLIENTS=
TCP_HEALTH=
ADB_HEALTH=
FAIL_REASON=
LAST_ACTION=
RECOVERY_ENABLED=
RECOVERY_COOLDOWN=
RESTART_COUNT=
RESTART_WINDOW=
COOLDOWN=
TOTAL_CONNECTIONS=
UPTIME=
PROP_TCP=
PROP_OK=
CLOSE_WAIT=
TIME_WAIT=
SYN_RECV=
```

Unreliable items are reported as `UNKNOWN` / `SKIPPED` — never forged `OK`.
