/**
 * @file adb_proto.h
 * @brief Android-side ADB wire-protocol health probe (Version 2.3).
 *
 * Connects to 127.0.0.1:5555, sends ADB CNXN, expects CNXN or AUTH.
 * Does not use the Windows `adb` CLI. Compatible with Android 5.1.1.
 */

#ifndef NEXUS_WATCHDOG_ADB_PROTO_H
#define NEXUS_WATCHDOG_ADB_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Probe result codes for ADB_HEALTH= reporting. */
#define ADB_HEALTH_OK       "OK"
#define ADB_HEALTH_FAULT    "FAULT"
#define ADB_HEALTH_UNKNOWN  "UNKNOWN"
#define ADB_HEALTH_SKIPPED  "SKIPPED"

/**
 * Probe local TCP ADB daemon protocol health.
 *
 * @param port        Local TCP port (typically 5555).
 * @param timeout_ms  Connect/read timeout in milliseconds.
 * @return 1 if CNXN/AUTH response received (healthy),
 *         0 if connect/protocol failed (unhealthy),
 *        -1 if probe could not run (I/O setup error => UNKNOWN).
 */
int adb_proto_probe(unsigned int port, int timeout_ms);

/**
 * Sticky inject flag path helpers (ADB_PROTOCOL_FAULT test).
 * Flag file forces ADB_HEALTH=FAULT until cleared (e.g. after recovery).
 */
int  adb_proto_inject_fault_set(void);
int  adb_proto_inject_fault_clear(void);
int  adb_proto_inject_fault_active(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_ADB_PROTO_H */
