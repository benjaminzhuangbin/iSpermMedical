# Nexus ADB Watchdog Version 2.4

Product-stable TCP ADB watchdog for Rockchip RK3288 / Android 5.1.1 (root).

**Core goal:** keep Ethernet TCP ADB (`192.168.31.11:5555`) available long-term for Data Manager integration.

Does **not** implement scrcpy / QtScrcpy / C# / APK / UI.

**Installs on Android:** `/data/local/watchdog/`  
Windows `*.bat` files are deploy/control tools only (**CRLF** in git/ZIP — required for `cmd.exe`).

---

## Path

```
NexusADBWatchdog-2.4/
NexusADBWatchdog-2.4/release/watchdog
```

Status **first line**:

```
Nexus ADB Watchdog 2.4
```

`watchdog -h` → Version **2.4.0**.

---

## Product hard rules (must never violate)

1. **Fault recovery only** — Watchdog must never periodically / proactively restart adbd.
2. If `ADBD=YES` and `PORT5555=YES` and there is **no clear Android-side fault** → **do nothing**.
3. If `ESTABLISHED>0` (live PC / QtScrcpy TCP session) → **never** `RESTART_ADBD` / stop adbd.
4. `ESTABLISHED=0` / no PC client → **not a fault**, never recovery.
5. Localhost CNXN / PC `offline` / internal probe failure → **not** a recovery trigger (removed in 2.4).
6. **Cooldown semantics:** detect clear fault → recover once → cooldown → re-check.  
   If already healthy → **immediately clear recovery state** and **must not** restart again.  
   Cooldown is **not** “every 60s restart adbd”.
7. Long-run goal: days/weeks/months of healthy TCP ADB + QtScrcpy without Watchdog killing the session.

---

## What 2.4 changes vs 2.3

| Topic | 2.4 behavior |
|-------|----------------|
| Localhost CNXN probe | **Removed** as recovery trigger (caused false `ADB_PROTOCOL_FAULT` while PC ADB/QtScrcpy worked) |
| Cooldown after false fault | **Fixed:** healthy state clears recovery immediately; no re-restart of live sessions |
| Live `ESTABLISHED>0` | **Hard block** on any adbd restart (protects QtScrcpy) |
| `ESTABLISHED=0` | **Never** a fault → `TCP_HEALTH=OK` + `ADB_HEALTH=OK` + `CLIENT_STATE=NO_CLIENT` |
| Auto recovery | Only: adbd missing, :5555 not LISTEN, wrong TCP prop (no live client), sustained CLOSE_WAIT (no live client) |
| PC `offline` | **Not** detected from Android |
| Rate limit | `max_restart` / window / cooldown — **never permanent disable** |
| install.bat | **CRLF** + STARTING banner (avoids silent Windows exit) |

---

## Health model

| Condition | TCP_HEALTH | ADB_HEALTH | CLIENT_STATE | FAIL_REASON |
|-----------|------------|------------|--------------|-------------|
| Service up + PC connected | OK | OK | CONNECTED | NONE |
| Service up + no PC | OK | OK | NO_CLIENT | NONE |
| adbd missing | FAULT | FAULT | * | ADBD_NOT_RUNNING |
| 5555 not LISTEN | FAULT | FAULT | * | PORT_NOT_LISTENING |
| prop ≠ 5555 | FAULT | OK | * | TCP_PORT_PROP |
| Sustained CLOSE_WAIT | FAULT | OK | * | TCP_FAULT |

---

## Recovery

| Case | Condition | Action |
|------|-----------|--------|
| A | adbd missing | setprop + start adbd |
| B | 5555 not LISTEN | setprop + stop + sleep + start |
| C | prop wrong / CLOSE_WAIT fault | setprop + restart adbd |

Rate limit: `max_restart=3` / `restart_window_sec=600` → wait for window; `cooldown_sec=60` after each recovery. `RECOVERY_ENABLED` stays 1 whenever conf allows.

---

## Build

```bat
set ANDROID_NDK_HOME=C:\path\to\ndk
build.bat
```

Linux:

```bash
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-22 \
  -DANDROID_STL=none -DCMAKE_BUILD_TYPE=Release
cmake --build build-android
```

---

## Install / start / stop

```bat
install.bat
start.bat
status.bat
stop.bat
```

`install.bat` must show:

```
Nexus ADB Watchdog 2.4 - install.bat STARTING
```

If you see **nothing**, the bat is still LF — re-download this folder or run `fix_crlf.ps1`.

---

## Test inject (Android-side only)

```bat
test_case_a.bat          REM STOP_ADBD
test_case_b.bat          REM BREAK_PORT
test_case_c.bat          REM CLEAR_TCP_PORT
```

```bat
adb shell "echo STOP_ADBD > /data/local/watchdog/watchdog.inject"
adb shell "echo BREAK_PORT > /data/local/watchdog/watchdog.inject"
adb shell "echo CLEAR_TCP_PORT > /data/local/watchdog/watchdog.inject"
```

| Test | Expected |
|------|----------|
| STOP_ADBD | ADBD=NO → START_ADBD → ADBD=YES PORT5555=YES TCP_HEALTH=OK ADB_HEALTH=OK |
| BREAK_PORT | PORT down → RESTART_ADBD → PORT5555=YES |
| No PC client | ESTABLISHED=0 → **no** recovery, RESTART_COUNT unchanged |
| Healthy + QtScrcpy | Must **not** restart adbd repeatedly |
| 3 recoveries | RATE_LIMIT until window ends; RECOVERY_ENABLED=1 |

---

## Configuration

| Key | Default | Meaning |
|-----|---------|---------|
| `interval` | 5 | Monitor period |
| `recovery_enable` | 1 | Allow recovery |
| `max_restart` | 3 | Max recoveries per window |
| `restart_window_sec` | 600 | Window; then count resets |
| `cooldown_sec` | 60 | Wait after each recovery |
| `adbd_sleep_sec` | 3 | Stop/start gap |
| `log_heartbeat_sec` | 300 | Idle log heartbeat |
| `fault_close_wait` | 3 | CLOSE_WAIT threshold |
| `fault_hold_sec` | 30 | Hold before TCP_FAULT recovery |

---

## Android 5.1.1 compatibility

- Pure C / NDK / CMake; no Java / modern APIs  
- `/proc/net/tcp`, properties, sockets — no `wc` / `tail` required  
- Root required for `ctl.start` / `ctl.stop` adbd  

---

## Known limitations

1. Cannot read Windows `adb devices` / `offline` from Android.  
2. Idle ESTABLISHED sessions are not treated as faults.  
3. Requires root.  
4. Windows bats must remain CRLF.

---

## Status fields

```
Nexus ADB Watchdog 2.4
TIME=
ADBD_PID=
ADBD=
PORT5555=
ESTABLISHED=
CLIENT=
CLIENTS=
CLIENT_STATE=
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
