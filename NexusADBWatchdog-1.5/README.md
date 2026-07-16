# Nexus ADB Watchdog Version 1.5

Native C diagnostic daemon for Android 5.1.1 (Rockchip RK3288 / ARMv7).

**Version 1.5 — diagnostic / data collection only. No automatic recovery.**

Collects enough runtime information to determine why TCP ADB becomes unstable
after repeated scrcpy/ADB connections. Designed for continuous 24/7 operation.

Does **not** restart adbd, kill adbd, reboot, or change system properties.

---

## Project path (GitHub / workspace)

```
NexusADBWatchdog-1.5/
```

Prebuilt binary:

```
NexusADBWatchdog-1.5/release/watchdog
```

---

## Project layout

```
NexusADBWatchdog-1.5/
    README.md
    build.bat
    install.bat
    uninstall.bat
    CMakeLists.txt
    watchdog.conf
    include/
    src/
    release/
        watchdog
        watchdog.conf
```

---

## What Version 1.5 collects (every cycle, default 1 second)

| Field | Description |
|-------|-------------|
| TIME | Local wall-clock time |
| ADBD_PID | PID of `/sbin/adbd`, or `-1` |
| ADBD | `YES` / `NO` |
| PORT5555 | `YES` / `NO` (listening) |
| ESTABLISHED / TIME_WAIT / CLOSE_WAIT / SYN_RECV / FIN_WAIT1 / FIN_WAIT2 / LAST_ACK / CLOSING / CLOSE | Counts for local port 5555 from `/proc/net/tcp` |
| CLIENTS | Remote IPv4 addresses currently ESTABLISHED |
| MAX_ESTABLISHED | Peak ESTABLISHED since start |
| TOTAL_CONNECTIONS | Cumulative newly observed ESTABLISHED endpoints |
| UPTIME | Seconds since watchdog start |

TCP data is read **directly from `/proc/net/tcp`**. No `netstat`, no `wc`.

---

## Runtime files (device)

| File | Behavior |
|------|----------|
| `/data/local/watchdog/watchdog.conf` | Read each cycle |
| `/data/local/watchdog/watchdog.status` | Overwritten each cycle |
| `/data/local/watchdog/watchdog.log` | Appended each cycle (never overwritten) |
| `/data/local/watchdog/watchdog.log.1` | Previous log after rotation |
| `/data/local/watchdog/watchdog.pid` | Daemon PID |

### Configuration (`watchdog.conf`)

```
interval=1
log_rotate_mb=10
status_update=1
```

### Log rotation

When `watchdog.log` exceeds `log_rotate_mb` megabytes (default 10):

1. Rename `watchdog.log` → `watchdog.log.1` (replacing any previous `.1`)
2. Create a new `watchdog.log` on the next append

### Status example

```
TIME=2026-07-13 16:44:06
ADBD_PID=351
ADBD=YES
PORT5555=YES
ESTABLISHED=2
TIME_WAIT=0
CLOSE_WAIT=0
SYN_RECV=0
FIN_WAIT1=0
FIN_WAIT2=0
LAST_ACK=0
CLOSING=0
CLOSE=0
MAX_ESTABLISHED=4
TOTAL_CONNECTIONS=325
UPTIME=7251
CLIENTS=192.168.31.100
```

### Log example

```
----------------------------------------
TIME=2026-07-13 16:44:06
ADBD PID=351
PORT5555=LISTEN
ESTABLISHED=2
TIME_WAIT=0
CLOSE_WAIT=0
SYN_RECV=0
FIN_WAIT1=0
FIN_WAIT2=0
LAST_ACK=0
CLOSING=0
CLOSE=0
CLIENTS=192.168.31.100
TOTAL_CONNECTIONS=325
MAX_ESTABLISHED=4
UPTIME=7251
----------------------------------------
```

---

## Requirements (Windows build PC)

| Tool | Purpose |
|------|---------|
| Android NDK (r16+; r21+ OK) | Cross-compile `armeabi-v7a` |
| CMake 3.10+ | Build system |
| Ninja or MinGW Make | Generator |
| `adb` (platform-tools) | Install / uninstall |

```bat
set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\21.4.7075529
```

Target: ABI `armeabi-v7a`, API `android-22` (Android 5.1).

---

## Build

```bat
cd NexusADBWatchdog-1.5
build.bat
```

Output:

* `release\watchdog`
* `release\watchdog.conf`

---

## Install

```bat
cd NexusADBWatchdog-1.5
install.bat
```

Install path: `/data/local/watchdog/`

---

## Start

```bat
adb shell "nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &"
```

Or on device:

```sh
nohup /data/local/watchdog/watchdog &
```

Check:

```bat
adb shell "cat /data/local/watchdog/watchdog.status"
adb shell "tail -n 50 /data/local/watchdog/watchdog.log"
```

---

## Uninstall

```bat
cd NexusADBWatchdog-1.5
uninstall.bat
```

Stops only the watchdog (via its PID file) and removes installed files.
**Never** stops/starts adbd or changes system properties.

---

## Constraints (Version 1.5 MUST NOT)

* stop / start / restart adbd
* `setprop`
* reboot
* `kill` / `killall` of adbd or other system processes

(The install/uninstall scripts may `kill` **only** the watchdog PID from `watchdog.pid`.)

---

## Resource targets

* CPU &lt; 1% under normal operation
* Minimal memory, no intentional heap growth / leaks
* Stable 24/7 continuous run

---

## Future versions (not implemented)

* **V2** — automatic adbd recovery (uses this diagnostic data)
* **V3** — medical application monitoring
* **V4** — heartbeat with Windows WinForms host

---

## Network context

| Host | Static IP |
|------|-----------|
| Android (RK3288) | `192.168.31.11` |
| Windows PC | `192.168.31.100` |

TCP ADB port: `5555`.
