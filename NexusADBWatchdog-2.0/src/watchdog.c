/**
 * @file watchdog.c
 * @brief Monitor loop + recovery orchestration (Version 2.0).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "watchdog.h"
#include "logger.h"
#include "network.h"
#include "process.h"
#include "recovery.h"
#include "status.h"
#include "util.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_watchdog_stop = 0;

static int g_max_established = 0;
static int g_total_connections = 0;
static time_t g_start_time = 0;

static network_endpoint_t g_prev_endpoints[NETWORK_MAX_ENDPOINTS];
static int g_prev_endpoint_count = 0;
static int g_have_prev_endpoints = 0;

static recovery_state_t g_recovery;

void watchdog_request_stop(void)
{
    g_watchdog_stop = 1;
}

/**
 * Extract first client IP from comma-separated clients list.
 */
static void watchdog_set_primary_client(watchdog_status_t *status)
{
    size_t i;

    if (status == NULL) {
        return;
    }

    status->client[0] = '\0';
    if (status->clients[0] == '\0') {
        return;
    }

    for (i = 0U; i + 1U < sizeof(status->client) &&
                 status->clients[i] != '\0' &&
                 status->clients[i] != ','; i++) {
        status->client[i] = status->clients[i];
    }
    status->client[i] = '\0';
}

int watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status)
{
    network_tcp_stats_t tcp;
    int adbd_pid;
    int adbd_ok;
    int port_ok;
    int new_conns;
    time_t now;

    if (cfg == NULL || status == NULL) {
        return -1;
    }

    if (g_start_time == 0) {
        g_start_time = time(NULL);
        if (g_start_time == (time_t)-1) {
            g_start_time = 1;
        }
    }

    memset(status, 0, sizeof(*status));
    status_set_time_now(status);

    adbd_pid = process_find_pid(WATCHDOG_ADBD_PATH);
    status->adbd_pid = (adbd_pid > 0) ? adbd_pid : -1;
    adbd_ok = (status->adbd_pid > 0) ? 1 : 0;
    util_strlcpy(status->adbd, adbd_ok ? "YES" : "NO", sizeof(status->adbd));

    if (network_collect_tcp_stats((unsigned int)WATCHDOG_ADB_PORT, &tcp) != 0) {
        memset(&tcp, 0, sizeof(tcp));
    }

    port_ok = tcp.listening ? 1 : 0;
    util_strlcpy(status->port5555, port_ok ? "YES" : "NO", sizeof(status->port5555));

    status->established = tcp.established;
    status->time_wait = tcp.time_wait;
    status->close_wait = tcp.close_wait;
    status->syn_recv = tcp.syn_recv;
    status->fin_wait1 = tcp.fin_wait1;
    status->fin_wait2 = tcp.fin_wait2;
    status->last_ack = tcp.last_ack;
    status->closing = tcp.closing;
    status->close_state = tcp.close_state;
    util_strlcpy(status->clients, tcp.clients, sizeof(status->clients));
    watchdog_set_primary_client(status);

    if (tcp.established > g_max_established) {
        g_max_established = tcp.established;
    }
    status->max_established = g_max_established;

    if (g_have_prev_endpoints) {
        new_conns = network_count_new_endpoints(g_prev_endpoints,
                                                g_prev_endpoint_count,
                                                tcp.endpoints,
                                                tcp.endpoint_count);
    } else {
        new_conns = tcp.endpoint_count;
        g_have_prev_endpoints = 1;
    }
    g_total_connections += new_conns;
    status->total_connections = g_total_connections;

    g_prev_endpoint_count = tcp.endpoint_count;
    if (g_prev_endpoint_count > NETWORK_MAX_ENDPOINTS) {
        g_prev_endpoint_count = NETWORK_MAX_ENDPOINTS;
    }
    if (g_prev_endpoint_count > 0) {
        memcpy(g_prev_endpoints, tcp.endpoints,
               (size_t)g_prev_endpoint_count * sizeof(network_endpoint_t));
    }

    now = time(NULL);
    if (now == (time_t)-1 || now < g_start_time) {
        status->uptime_seconds = 0;
    } else {
        status->uptime_seconds = (long)(now - g_start_time);
    }

    /* Version 2.0: automatic recovery (rate-limited). */
    (void)recovery_evaluate(&g_recovery, cfg, adbd_ok, port_ok, tcp.established);

    util_strlcpy(status->last_action, g_recovery.last_action, sizeof(status->last_action));
    status->recovery_enabled = g_recovery.enabled;
    status->restart_count_window = g_recovery.restart_count;

    if (cfg->status_update) {
        (void)status_write_file(WATCHDOG_STATUS_PATH, status);
    }
    if (cfg->log_enable) {
        (void)logger_append_status(WATCHDOG_LOG_PATH, status, cfg->log_rotate_mb);
    }

    return 0;
}

int watchdog_run_loop(const watchdog_config_t *cfg)
{
    watchdog_config_t local;
    int interval;

    if (cfg == NULL) {
        return -1;
    }

    local = *cfg;
    interval = local.interval;
    if (interval < 1) {
        interval = WATCHDOG_DEFAULT_INTERVAL;
    }

    g_watchdog_stop = 0;
    g_start_time = time(NULL);
    if (g_start_time == (time_t)-1) {
        g_start_time = 1;
    }

    recovery_init(&g_recovery, &local);

    while (!g_watchdog_stop) {
        watchdog_status_t status;
        int i;

        (void)config_load(WATCHDOG_CONF_PATH, &local);
        interval = local.interval;
        if (interval < 1) {
            interval = WATCHDOG_DEFAULT_INTERVAL;
        }

        /* Config may re-enable recovery_enable, but rate-limit latch stays. */
        if (!local.recovery_enable) {
            g_recovery.enabled = 0;
        }

        (void)watchdog_run_once(&local, &status);

        for (i = 0; i < interval && !g_watchdog_stop; i++) {
            (void)sleep(1);
        }
    }

    return 0;
}
