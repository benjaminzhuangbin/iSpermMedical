# iSpermMedical

Medical instrument software workspace.

## Nexus ADB Watchdog

Native Android daemons (C) for Ethernet TCP ADB on RK3288 / Android 5.1.1.

| Version | Path | Notes |
|---------|------|-------|
| 1.0 | `NexusADBWatchdog/` | Basic monitor |
| 1.5 | `NexusADBWatchdog-1.5/` | Diagnostics |
| 2.0 | `NexusADBWatchdog-2.0/` | Early recovery (permanent latch / aggressive no-client) |
| 2.1 | `NexusADBWatchdog-2.1/` | Cooldown + no-client OK |
| 2.2 | `NexusADBWatchdog-2.2/` | TCP_HEALTH + CLOSE_WAIT heuristic |
| 2.3 | `NexusADBWatchdog-2.3/` | CNXN probe (false positives on field devices) |
| **2.4** | [`NexusADBWatchdog-2.4/`](NexusADBWatchdog-2.4/) | **Product-stable: no CNXN recovery; ESTABLISHED=0 = OK** |

**Use Version 2.4:** `NexusADBWatchdog-2.4/`

## Nexus ADB Watchdog APK

| Version | Path | Notes |
|---------|------|-------|
| **2.4 APK** | [`APKNexusADBWatchdog-2.4/`](APKNexusADBWatchdog-2.4/) | Android Studio Arctic Fox project + `com.nexus.adbwatchdog` Service APK |

Debug APK (built): `APKNexusADBWatchdog-2.4/release/NexusADBWatchdog.apk`
