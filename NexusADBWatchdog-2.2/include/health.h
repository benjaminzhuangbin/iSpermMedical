/**
 * @file health.h
 * @brief TCP ADB health classification (Version 2.2).
 *
 * ESTABLISHED=0 => NO_CLIENT (not FAULT).
 * Android cannot read PC "adb devices ... offline"; heuristics only.
 */

#ifndef NEXUS_WATCHDOG_HEALTH_H
#define NEXUS_WATCHDOG_HEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_HEALTH_OK         "OK"
#define TCP_HEALTH_NO_CLIENT  "NO_CLIENT"
#define TCP_HEALTH_FAULT      "FAULT"

#define FAIL_REASON_NONE              "NONE"
#define FAIL_REASON_ADBD_NOT_RUNNING  "ADBD_NOT_RUNNING"
#define FAIL_REASON_PORT_NOT_LISTENING "PORT_NOT_LISTENING"
#define FAIL_REASON_TCP_PORT_PROP     "TCP_PORT_PROP"
#define FAIL_REASON_TCP_FAULT         "TCP_FAULT"

/**
 * Classify health from Android-observable signals.
 *
 * @param adbd_ok       1 if adbd running
 * @param port_ok       1 if :5555 LISTEN
 * @param prop_ok       1 if persist.adb.tcp.port == 5555
 * @param established   ESTABLISHED count on :5555
 * @param close_wait    CLOSE_WAIT count on :5555
 * @param socket_fault  1 if sustained CLOSE_WAIT fault detected
 * @param out_health    Buffer for TCP_HEALTH=
 * @param health_len    Size of out_health
 * @param out_reason    Buffer for FAIL_REASON=
 * @param reason_len    Size of out_reason
 */
void health_classify(int adbd_ok,
                     int port_ok,
                     int prop_ok,
                     int established,
                     int close_wait,
                     int socket_fault,
                     char *out_health,
                     unsigned int health_len,
                     char *out_reason,
                     unsigned int reason_len);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_HEALTH_H */
