# Nexus ADB Watchdog Version 2.2

**Production test version** for Rockchip RK3288 / Android 5.1.1 (root).

Keeps Ethernet TCP ADB (`192.168.31.11:5555`) available long-term and recovers on **real** Android-side faults.

Does **not** implement scrcpy / QtScrcpy / C# / APK / UI.

**Installs on Android:** `/data/local/watchdog/`  
Windows `*.bat` files are deploy/control tools only.

---

## Path

```
NexusADBWatchdog-2.2/
NexusADBWatchdog-2.2/release/watchdog
```

After start, status **first line** is:

```
Nexus ADB Watchdog 2.2
```

---

## What 2.2 changes vs 2.1

| Topic | 2.2 behavior |
|-------|----------------|
| No PC client (`ESTABLISHED=0`) | `TCP_HEALTH=NO_CLIENT` — **never** restarts adbd |
| `adb disconnect` | Normal; no recovery storm |
| Rate limit | `max_restart` → **cooldown** → auto resume (`RECOVERY_COOLDOWN`) — **never permanent disable** |
| Health fields | `TCP_HEALTH` / `FAIL_REASON` |
| Case C | Wrong `persist.adb.tcp.port` **or** sustained high `CLOSE_WAIT` (Android heuristic) |
| PC `offline` | **Cannot** be read from Android; documented limitation below |
| install.bat | CRLF + Android 5.1-safe (no multi-file `touch`) |

---

## Important limitation (PC offline)

Android Watchdog **cannot** read PC `adb devices` lines such as:

```
192.168.31.11:5555    offline
```

It only observes Android-side signals (`adbd`, `:5555 LISTEN`, props, `/proc/net/tcp` states).

If PC shows `offline` while Android still has `ADBD=YES` + `PORT5555=YES` and clean sockets, Watchdog may correctly show `TCP_HEALTH=OK` or `NO_CLIENT` and **not** restart — until an Android-observable fault appears (or CLOSE_WAIT fault heuristic trips).

There is **no** fake “100% offline detection”.

---

## Health model

| TCP_HEALTH | Meaning |
|------------|---------|
| `OK` | Service up + at least one ESTABLISHED client |
| `NO_CLIENT` | Service up, ESTABLISHED=0 (normal idle) |
| `FAULT` | Real fault (`FAIL_REASON` set) |

`FAIL_REASON` examples: `NONE`, `ADBD_NOT_RUNNING`, `PORT_NOT_LISTENING`, `TCP_PORT_PROP`, `TCP_FAULT`.

---

## Recovery

| Case | Condition | Action |
|------|-----------|--------|
| A | adbd missing | setprop + start adbd |
| B | 5555 not LISTEN | setprop + stop + sleep + start |
| C | prop ≠ 5555 **or** CLOSE_WAIT ≥ threshold for hold time | setprop + restart adbd |

Rate limit: `max_restart=3` / `restart_window_sec=600` → `RECOVERY_COOLDOWN=1` for `cooldown_sec=60` → clear count and resume.

---

## Configuration (`watchdog.conf`)

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
fault_close_wait=3
fault_hold_sec=30
```

---

## A. Build

```bat
set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\<version>
cd NexusADBWatchdog-2.2
build.bat
```

Target: `armeabi-v7a` / API 22.

## B. Install

```bat
install.bat
```

## C. Start

```bat
start.bat
```

## D. Status

```bat
status.bat
adb shell "cat /data/local/watchdog/watchdog.status"
```

First line must be: `Nexus ADB Watchdog 2.2`

## E. Log

```bat
adb shell "cat /data/local/watchdog/watchdog.log"
```

(No `tail` / `wc` / `which` required.)

## F. Stop

```bat
stop.bat
```

## G. Restart watchdog

```bat
stop.bat
start.bat
```

## H. TCP ADB normal

```bat
adb connect 192.168.31.11:5555
adb devices
```

Expect: `192.168.31.11:5555 device`  
Status: `TCP_HEALTH=OK`, `CLIENT=192.168.31.100` (typical)

## I. adb disconnect (must NOT storm recovery)

```bat
adb disconnect 192.168.31.11:5555
```

Wait ≥ 90s, `status.bat`.

Expect:

- `ESTABLISHED=0`
- `TCP_HEALTH=NO_CLIENT`
- `FAIL_REASON=NONE`
- `RESTART_COUNT` unchanged
- `ADBD_PID` unchanged

## J. 5555 LISTEN fault

```bat
test_case_b.bat
```

Expect recovery → `PORT5555=YES`

## K. adbd fault

```bat
test_case_a.bat
```

Expect `START_ADBD` → `ADBD=YES`

## L. Recovery / prop

```bat
test_case_c.bat
```

Expect `PROP_OK=1`

## M. Cooldown

Force 3 recoveries quickly → `RECOVERY_COOLDOWN=1`  
Wait `cooldown_sec` → `RECOVERY_COOLDOWN=0`, recovery works again  
**Never** permanent `RECOVERY_DISABLED`.

---

## Network

| Host | IP |
|------|----|
| Android | `192.168.31.11` |
| Windows PC | `192.168.31.100` |
| ADB TCP | `5555` |

USB ADB remains for engineering/emergency only.
