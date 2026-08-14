/**
 * @file health.h
 * @brief TCP ADB + ADB protocol health classification (Version 2.3).
 *
 * ESTABLISHED=0 => NO_CLIENT (not FAULT).
 * Layer 4: ADB_HEALTH via localhost CNXN probe / inject flag.
 */

#ifndef NEXUS_WATCHDOG_HEALTH_H
#define NEXUS_WATCHDOG_HEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_HEALTH_OK         "OK"
#define TCP_HEALTH_NO_CLIENT  "NO_CLIENT"
#define TCP_HEALTH_FAULT      "FAULT"
#define TCP_HEALTH_UNKNOWN    "UNKNOWN"

#define FAIL_REASON_NONE               "NONE"
#define FAIL_REASON_ADBD_NOT_RUNNING   "ADBD_NOT_RUNNING"
#define FAIL_REASON_PORT_NOT_LISTENING "PORT_NOT_LISTENING"
#define FAIL_REASON_TCP_PORT_PROP      "TCP_PORT_PROP"
#define FAIL_REASON_TCP_FAULT          "TCP_FAULT"
#define FAIL_REASON_ADB_PROTOCOL_FAULT "ADB_PROTOCOL_FAULT"
#define FAIL_REASON_SESSION_STALE      "SESSION_STALE"

/**
 * Classify Layer 1–4 health from Android-observable signals.
 *
 * @param adb_health_ok  1=OK, 0=FAULT, -1=UNKNOWN/SKIPPED (probe not conclusive)
 * @param adb_health_str Buffer for ADB_HEALTH=
 */
void health_classify(int adbd_ok,
                     int port_ok,
                     int prop_ok,
                     int established,
                     int socket_fault,
                     int session_stale,
                     int adb_health_ok,
                     char *out_tcp_health,
                     unsigned int tcp_health_len,
                     char *out_adb_health,
                     unsigned int adb_health_len,
                     char *out_reason,
                     unsigned int reason_len);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_HEALTH_H */
