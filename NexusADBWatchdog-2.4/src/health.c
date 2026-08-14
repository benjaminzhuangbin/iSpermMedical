/**
 * @file health.c
 * @brief Android-side TCP ADB health classification (Version 2.4).
 */

#include "health.h"
#include "util.h"

#include <string.h>

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
                     unsigned int reason_len)
{
    if (out_tcp_health == NULL || out_adb_health == NULL ||
        out_client_state == NULL || out_reason == NULL ||
        tcp_health_len == 0U || adb_health_len == 0U ||
        client_state_len == 0U || reason_len == 0U) {
        return;
    }

    /* Client presence is informational only — never a fault. */
    if (established > 0) {
        util_strlcpy(out_client_state, CLIENT_STATE_CONNECTED, client_state_len);
    } else {
        util_strlcpy(out_client_state, CLIENT_STATE_NO_CLIENT, client_state_len);
    }

    if (!adbd_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_adb_health, ADB_HEALTH_FAULT, adb_health_len);
        util_strlcpy(out_reason, FAIL_REASON_ADBD_NOT_RUNNING, reason_len);
        return;
    }

    if (!port_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_adb_health, ADB_HEALTH_FAULT, adb_health_len);
        util_strlcpy(out_reason, FAIL_REASON_PORT_NOT_LISTENING, reason_len);
        return;
    }

    if (!prop_ok) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        /* adbd running + port listening, but TCP port property wrong */
        util_strlcpy(out_adb_health, ADB_HEALTH_OK, adb_health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_PORT_PROP, reason_len);
        return;
    }

    if (socket_fault) {
        util_strlcpy(out_tcp_health, TCP_HEALTH_FAULT, tcp_health_len);
        util_strlcpy(out_adb_health, ADB_HEALTH_OK, adb_health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_FAULT, reason_len);
        return;
    }

    /*
     * Android-side TCP ADB service is healthy.
     * ESTABLISHED=0 / no PC client is normal and must stay OK.
     */
    util_strlcpy(out_tcp_health, TCP_HEALTH_OK, tcp_health_len);
    util_strlcpy(out_adb_health, ADB_HEALTH_OK, adb_health_len);
    util_strlcpy(out_reason, FAIL_REASON_NONE, reason_len);
}
