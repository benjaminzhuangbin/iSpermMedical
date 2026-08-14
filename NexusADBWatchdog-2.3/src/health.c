/**
 * @file health.c
 * @brief TCP ADB + ADB protocol health classification (Version 2.3).
 */

#include "health.h"
#include "adb_proto.h"
#include "util.h"

#include <string.h>

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
                     unsigned int reason_len)
{
    if (out_tcp_health == NULL || out_adb_health == NULL || out_reason == NULL ||
        tcp_health_len == 0U || adb_health_len == 0U || reason_len == 0U) {
        return;
    }

    /* Default ADB_HEALTH from probe result; may be overridden below. */
    if (!adbd_ok || !port_ok) {
        util_strlcpy(out_adb_health, ADB_HEALTH_SKIPPED, adb_health_len);
    } else if (adb_health_ok > 0) {
        util_strlcpy(out_adb_health, ADB_HEALTH_OK, adb_health_len);
    } else if (adb_health_ok == 0) {
        util_strlcpy(out_adb_health, ADB_HEALTH_FAULT, adb_health_len);
    } else {
        util_strlcpy(out_adb_health, ADB_HEALTH_UNKNOWN, adb_health_len);
    }

    if (!adbd_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_reason, FAIL_REASON_ADBD_NOT_RUNNING, reason_len);
        return;
    }

    if (!port_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_reason, FAIL_REASON_PORT_NOT_LISTENING, reason_len);
        return;
    }

    if (!prop_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_PORT_PROP, reason_len);
        return;
    }

    /* Layer 4: ADB protocol/session health (offline-class fault). */
    if (adb_health_ok == 0) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_adb_health, ADB_HEALTH_FAULT, adb_health_len);
        util_strlcpy(out_reason, FAIL_REASON_ADB_PROTOCOL_FAULT, reason_len);
        return;
    }

    if (session_stale) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_reason, FAIL_REASON_SESSION_STALE, reason_len);
        return;
    }

    if (socket_fault) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_FAULT, reason_len);
        return;
    }

    /* Service healthy; client optional. ESTABLISHED=0 is NOT a fault. */
    if (established > 0) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_OK, tcp_health_len);
    } else {
        util_strlcpy(out_tcp_health, TCP_HEALTH_NO_CLIENT, tcp_health_len);
    }
    util_strlcpy(out_reason, FAIL_REASON_NONE, reason_len);
}
