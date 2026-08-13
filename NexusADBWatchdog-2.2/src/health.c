/**
 * @file health.c
 * @brief TCP ADB health classification.
 */

#include "health.h"
#include "util.h"

#include <string.h>

void health_classify(int adbd_ok,
                     int port_ok,
                     int prop_ok,
                     int established,
                     int close_wait,
                     int socket_fault,
                     char *out_health,
                     unsigned int health_len,
                     char *out_reason,
                     unsigned int reason_len)
{
    (void)close_wait;

    if (out_health == NULL || out_reason == NULL ||
        health_len == 0U || reason_len == 0U) {
        return;
    }

    if (!adbd_ok) {
        util_strlcpy(out_health, TCP_HEALTH_FAULT, health_len);
        util_strlcpy(out_reason, FAIL_REASON_ADBD_NOT_RUNNING, reason_len);
        return;
    }

    if (!port_ok) {
        util_strlcpy(out_health, TCP_HEALTH_FAULT, health_len);
        util_strlcpy(out_reason, FAIL_REASON_PORT_NOT_LISTENING, reason_len);
        return;
    }

    if (!prop_ok) {
        util_strlcpy(out_health, TCP_HEALTH_FAULT, health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_PORT_PROP, reason_len);
        return;
    }

    if (socket_fault) {
        util_strlcpy(out_health, TCP_HEALTH_FAULT, health_len);
        util_strlcpy(out_reason, FAIL_REASON_TCP_FAULT, reason_len);
        return;
    }

    /* Service healthy; client optional. */
    if (established > 0) {
        util_strlcpy(out_health, TCP_HEALTH_OK, health_len);
    } else {
        util_strlcpy(out_health, TCP_HEALTH_NO_CLIENT, health_len);
    }
    util_strlcpy(out_reason, FAIL_REASON_NONE, reason_len);
}
