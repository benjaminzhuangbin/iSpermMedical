/**
 * @file health.h
 * @brief Android-side TCP ADB health classification (Version 2.4).
 *
 * ESTABLISHED=0 is NEVER a fault: TCP_HEALTH=OK, ADB_HEALTH=OK.
 * Localhost CNXN probe is NOT used (removed as recovery trigger in 2.4).
 */

#ifndef NEXUS_WATCHDOG_HEALTH_H
#define NEXUS_WATCHDOG_HEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_HEALTH_OK         "OK"
#define TCP_HEALTH_FAULT      "FAULT"
#define TCP_HEALTH_UNKNOWN    "UNKNOWN"

#define ADB_HEALTH_OK         "OK"
#define ADB_HEALTH_FAULT      "FAULT"
#define ADB_HEALTH_UNKNOWN    "UNKNOWN"

#define CLIENT_STATE_CONNECTED "CONNECTED"
#define CLIENT_STATE_NO_CLIENT "NO_CLIENT"

#define FAIL_REASON_NONE               "NONE"
#define FAIL_REASON_ADBD_NOT_RUNNING   "ADBD_NOT_RUNNING"
#define FAIL_REASON_PORT_NOT_LISTENING "PORT_NOT_LISTENING"
#define FAIL_REASON_TCP_PORT_PROP      "TCP_PORT_PROP"
#define FAIL_REASON_TCP_FAULT          "TCP_FAULT"

/**
 * Classify Android-observable health only.
 *
 * Healthy service (adbd + :5555 LISTEN + prop + no socket fault):
 *   TCP_HEALTH=OK, ADB_HEALTH=OK, FAIL_REASON=NONE
 *   even when ESTABLISHED=0 (client optional).
 */
void health_classify(int adbd_ok,
                    int port_ok,
                    int prop_ok,
                    int established,
                    int socket_fault,
                    char *out_tcp_health,
                    unsigned int tcp_health_len,
                    char *out_adb_health,
                    unsigned int adb_health_len,
                    char *out_client_state,
                    unsigned int client_state_len,
                    char *out_reason,
                    unsigned int reason_len);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_HEALTH_H */
