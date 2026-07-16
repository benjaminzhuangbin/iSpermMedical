/**
 * @file watchdog.c
 * @brief Core diagnostic cycle and forever loop (Version 1.5 — monitor only).
 *
 * Collects adbd PID, TCP state counters, client IPs, max ESTABLISHED,
 * cumulative new connections, and uptime. Never restarts adbd or mutates
 * system state.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "watchdog.h"
#include "logger.h"
#include "network.h"
#include "process.h"
#include "status.h"
#include "util.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** Set to 1 by signal handler / request_stop to exit the loop. */
static volatile sig_atomic_t g_watchdog_stop = 0;

/** Cumulative counters retained across cycles (process lifetime). */
static int g_max_established = 0;
static int g_total_connections = 0;
static time_t g_start_time = 0;

/** Previous-cycle ESTABLISHED endpoints for new-connection detection. */
static network_endpoint_t g_prev_endpoints[NETWORK_MAX_ENDPOINTS];
static int g_prev_endpoint_count = 0;
static int g_have_prev_endpoints = 0;

/**
 * Request the monitor loop to exit after the current cycle.
 */
void watchdog_request_stop(void)
{
    g_watchdog_stop = 1;
}

/**
 * Perform one diagnostic monitoring cycle and fill status.
 */
int watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status)
{
    network_tcp_stats_t tcp;
    int adbd_pid;
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

    /* 2/3. adbd PID + running YES/NO */
    adbd_pid = process_find_pid(WATCHDOG_ADBD_PATH);
    status->adbd_pid = (adbd_pid > 0) ? adbd_pid : -1;
    if (status->adbd_pid > 0) {
        util_strlcpy(status->adbd, "YES", sizeof(status->adbd));
    } else {
        util_strlcpy(status->adbd, "NO", sizeof(status->adbd));
    }

    /* 4/5/6. TCP listen + state counters + client IPs from /proc/net/tcp */
    if (network_collect_tcp_stats((unsigned int)WATCHDOG_ADB_PORT, &tcp) != 0) {
        memset(&tcp, 0, sizeof(tcp));
    }

    if (tcp.listening) {
        util_strlcpy(status->port5555, "YES", sizeof(status->port5555));
    } else {
        util_strlcpy(status->port5555, "NO", sizeof(status->port5555));
    }

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

    /* 7. Maximum ESTABLISHED since start */
    if (tcp.established > g_max_established) {
        g_max_established = tcp.established;
    }
    status->max_established = g_max_established;

    /* 8. Cumulative newly observed ESTABLISHED endpoints */
    if (g_have_prev_endpoints) {
        new_conns = network_count_new_endpoints(g_prev_endpoints,
                                                g_prev_endpoint_count,
                                                tcp.endpoints,
                                                tcp.endpoint_count);
    } else {
        /* First cycle: count currently established as baseline total. */
        new_conns = tcp.endpoint_count;
        g_have_prev_endpoints = 1;
    }
    g_total_connections += new_conns;
    status->total_connections = g_total_connections;

    /* Remember current endpoints for next cycle. */
    g_prev_endpoint_count = tcp.endpoint_count;
    if (g_prev_endpoint_count > NETWORK_MAX_ENDPOINTS) {
        g_prev_endpoint_count = NETWORK_MAX_ENDPOINTS;
    }
    if (g_prev_endpoint_count > 0) {
        memcpy(g_prev_endpoints, tcp.endpoints,
               (size_t)g_prev_endpoint_count * sizeof(network_endpoint_t));
    }

    /* 9. Uptime */
    now = time(NULL);
    if (now == (time_t)-1 || now < g_start_time) {
        status->uptime_seconds = 0;
    } else {
        status->uptime_seconds = (long)(now - g_start_time);
    }

    /* Persist outputs */
    if (cfg->status_update) {
        (void)status_write_file(WATCHDOG_STATUS_PATH, status);
    }
    if (cfg->log_enable) {
        (void)logger_append_status(WATCHDOG_LOG_PATH, status, cfg->log_rotate_mb);
    }

    return 0;
}

/**
 * Run the forever monitor loop until stop is requested.
 */
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

    while (!g_watchdog_stop) {
        watchdog_status_t status;
        int i;

        /* Reload config each cycle so operators can tune without restart. */
        (void)config_load(WATCHDOG_CONF_PATH, &local);
        interval = local.interval;
        if (interval < 1) {
            interval = WATCHDOG_DEFAULT_INTERVAL;
        }

        (void)watchdog_run_once(&local, &status);

        /*
         * Sleep in 1-second slices so SIGTERM can stop promptly.
         * Version 1.5 remains monitoring-only; no recovery actions.
         */
        for (i = 0; i < interval && !g_watchdog_stop; i++) {
            (void)sleep(1);
        }
    }

    return 0;
}
