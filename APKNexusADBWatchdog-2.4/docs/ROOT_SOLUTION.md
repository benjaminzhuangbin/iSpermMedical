# Root solution — why stock `su` fails and how the APK gets UID 0

## Diagnosis (confirmed on RK3288 Android 5.1.1)

| Caller | Command | Result |
|--------|---------|--------|
| adb shell (uid 2000) | `su -c id` | `uid=0(root)` |
| APK (uid 10053) | any `su -c id` / `sh -c "su -c id"` | `su: uid 10053 not allowed to su` |

Stock `/system/xbin/su` is setuid (`-rwsr-sr-x`) and SELinux is Permissive, so elevation
**mechanically works**. The failure is a **userspace UID allowlist inside `su`**, not path
discovery and not SELinux.

Therefore:

- More `Runtime.exec(su)` path guessing cannot fix this.
- Becoming a normal `/system/priv-app` alone does **not** change the app UID enough to
  satisfy stock `su` (still a non-shell, non-root app id unless platform-signed
  `sharedUserId="android.uid.system"` — we do not ship OEM platform keys).
- Fake `ROOT_OK=1` without running `id` → `uid=0(root)` is forbidden.

## Product fix (one APK + factory firmware step)

Ship a dedicated helper binary **`nexus_su`** inside the APK project:

- Source: `native/nexus_su/nexus_su.c`
- Packaged in APK assets: `assets/native/armeabi-v7a/nexus_su`
- Also copied to `factory/nexus_su` and `release/nexus_su`

Factory (manufacturing) installs it once onto the system image:

```text
/system/xbin/nexus_su
owner root:root
mode 06755  (setuid)
```

Kernel setuid elevates **any** caller’s euid to 0. `nexus_su` does **not** implement
the stock `su` app-UID deny list. The APK then runs:

```text
/system/xbin/nexus_su -c id
→ uid=0(root)
```

Status must show:

```text
ROOT_OK=1
ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su
ROOT_UID=0
```

### What hospital users do

Nothing. Power on + Ethernet. No USB, no Magisk, no second APK, no manual start.

### What factory / engineering does (once per firmware)

Windows:

```bat
APKNexusADBWatchdog-2.4\factory\install_system.bat
adb reboot
```

Or see `factory/install_on_device.sh`.

This is a **firmware manufacturing** step (allowed). It is **not** an end-user step.

## What was tried and rejected

1. Bare / absolute `su` from the APK process — UID denied.
2. `/system/bin/sh -c "su -c …"` — same real UID, still denied.
3. Claiming root because `adb shell su` works — invalid; must be APK path.

## Capability after successful factory install

With `ROOT_OK=1`, the existing Watchdog 2.4 APK recovery engine can:

- `setprop persist.adb.tcp.port 5555`
- `setprop ctl.start / ctl.stop adbd`

…and restore Ethernet TCP ADB `192.168.31.11:5555` on real faults, without periodic
restarts while QtScrcpy is healthy (`ESTABLISHED>0`).
