/**
 * @file inject.h
 * @brief Safe test inject for Android-side CASE A / B / C only (Version 2.4).
 */

#ifndef NEXUS_WATCHDOG_INJECT_H
#define NEXUS_WATCHDOG_INJECT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process /data/local/watchdog/watchdog.inject if present, then delete it.
 *
 * Supported:
 *   STOP_ADBD       - ctl.stop adbd
 *   BREAK_PORT      - clear TCP port + restart adbd
 *   CLEAR_TCP_PORT  - clear persist.adb.tcp.port
 *
 * ADB_PROTOCOL_FAULT / localhost CNXN inject removed in 2.4.
 */
int inject_process_file(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_INJECT_H */
