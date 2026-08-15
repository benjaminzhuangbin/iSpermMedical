# Main APP → Nexus ADB Watchdog Service

Package: `com.nexus.adbwatchdog`  
Service: `com.nexus.adbwatchdog.NexusADBWatchdogService`

## Start

```java
Intent i = new Intent();
i.setComponent(new android.content.ComponentName(
        "com.nexus.adbwatchdog",
        "com.nexus.adbwatchdog.NexusADBWatchdogService"));
i.setAction("com.nexus.adbwatchdog.action.START");
context.startService(i);
```

## Stop

```java
Intent i = new Intent();
i.setComponent(new android.content.ComponentName(
        "com.nexus.adbwatchdog",
        "com.nexus.adbwatchdog.NexusADBWatchdogService"));
i.setAction("com.nexus.adbwatchdog.action.STOP");
context.startService(i);
```

## Manifest note (Main APP)

No special permission is required to start an **exported** service on API 22
when both apps are installed. Keep Watchdog installed as a separate APK.

## Suggested Main APP flow

1. Main APP `onCreate` / after login.
2. `startService(START)`.
3. Do not embed Watchdog as a library — keep two APKs.
