# Nexus ADB Watchdog

Native C daemon for Android 5.1.1 (Rockchip RK3288 / ARMv7).

**Version 1 — monitoring only.**

Monitors TCP ADB (`adbd` + port `5555`) and writes status/log files under `/data/local/watchdog/`. Does **not** restart adbd, kill processes, reboot, or change system properties.

---

## Project layout

```
NexusADBWatchdog/
    README.md
    build.bat
    install.bat
    CMakeLists.txt
    watchdog.conf
    include/
        watchdog.h
        logger.h
        network.h
        process.h
        status.h
        config.h
        util.h
    src/
        main.c
        watchdog.c
        logger.c
        network.c
        process.c
        status.c
        config.c
        util.c
    release/          # build output (watchdog + watchdog.conf)
```

---

## Requirements (Windows build PC)

| Tool | Purpose |
|------|---------|
| Android NDK (r16+ recommended; r21+ OK) | Cross-compile for `armeabi-v7a` |
| CMake 3.10+ | Build system |
| Ninja or MinGW Make | Generator used by `build.bat` |
| Android platform-tools (`adb`) | Device install via `install.bat` |

Set NDK path (example):

```bat
set ANDROID_NDK_HOME=C:\Android\Sdk\ndk\21.4.7075529
```

`build.bat` also checks `ANDROID_NDK_ROOT`, `NDK_ROOT`, and common SDK NDK folders.

---

## Build

From `NexusADBWatchdog\`:

```bat
build.bat
```

Output:

* `release\watchdog` — native ARM executable
* `release\watchdog.conf` — default configuration

Target:

* ABI: `armeabi-v7a`
* API: `android-22` (Android 5.1)

### Manual CMake (optional)

```bat
cmake -S . -B build-android ^
  -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake ^
  -DANDROID_ABI=armeabi-v7a ^
  -DANDROID_PLATFORM=android-22 ^
  -DANDROID_STL=none ^
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-android --config Release
```

---

## Install (device)

Device must be reachable over USB ADB (maintenance) or TCP ADB.

```bat
install.bat
```

This will:

1. `mkdir -p /data/local/watchdog`
2. `adb push` binary + `watchdog.conf`
3. `chmod` executable / config / log / status files

Install path:

```
/data/local/watchdog/
```

---

## Startup

On the Android device (root shell):

```sh
nohup /data/local/watchdog/watchdog &
```

From Windows:

```bat
adb shell "nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &"
```

Foreground / debug:

```sh
/data/local/watchdog/watchdog -f
```

Custom config:

```sh
/data/local/watchdog/watchdog -c /data/local/watchdog/watchdog.conf
```

Stop:

```sh
kill $(cat /data/local/watchdog/watchdog.pid)
```

---

## Runtime files

| File | Behavior |
|------|----------|
| `/data/local/watchdog/watchdog.conf` | Read each cycle |
| `/data/local/watchdog/watchdog.status` | Overwritten each cycle |
| `/data/local/watchdog/watchdog.log` | Appended each cycle |
| `/data/local/watchdog/watchdog.pid` | Daemon PID |

### Configuration (`watchdog.conf`)

```
CHECK_INTERVAL=10
LOG_ENABLE=1
STATUS_ENABLE=1
```

### Status example

```
TIME=2026-07-13 15:00:00
ADBD=OK
PORT5555=LISTEN
ESTABLISHED=1
```

### Log example

```
--------------------------------
TIME=2026-07-13 15:00:00
ADBD=OK
PORT5555=LISTEN
ESTABLISHED=1
```

Quick check from Windows:

```bat
adb shell "cat /data/local/watchdog/watchdog.status"
adb shell "cat /data/local/watchdog/watchdog.log"
```

---

## Version 1 checks (every cycle)

1. **adbd process** — expects `/sbin/adbd` → `OK` / `LOST`
2. **Port 5555 listen** — parses `/proc/net/tcp` → `LISTEN` / `NOT_LISTEN`
3. **ESTABLISHED count** — parses `/proc/net/tcp` directly (no `wc` / shell)

Loop:

```
while (true) {
    Check adbd
    Check port 5555
    Count ESTABLISHED on 5555
    Update status file
    Append log
    Sleep CHECK_INTERVAL seconds
}
```

---

## Design notes

* ANSI C / C99, modular headers for later versions
* Prefer POSIX `/proc` parsing over shell
* No Java / APK / Android Service
* CPU target &lt; 0.1%, memory target &lt; 1 MB
* Interfaces reserved for later:
  * **V2** automatic adbd recovery
  * **V3** medical application monitoring
  * **V4** heartbeat with Windows WinForms host

---

## Network context

| Host | Static IP |
|------|-----------|
| Android (RK3288) | `192.168.31.11` |
| Windows PC | `192.168.31.100` |

TCP ADB port: `5555` (`persist.adb.tcp.port=5555`).

USB ADB remains for maintenance only; customers use Ethernet only.

---

## License / delivery

Production-ready Version 1 source tree for firmware integration.
