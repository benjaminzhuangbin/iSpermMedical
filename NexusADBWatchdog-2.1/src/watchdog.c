/**
 * @file watchdog.c
 * @brief Monitor loop + recovery orchestration (Version 2.1).
 *
 * ESTABLISHED=0 is normal (no PC client) and never triggers recovery.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "watchdog.h"
#include "inject.h"
#include "logger.h"
#include "network.h"
#include "process.h"
#include "property.h"
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
static time_t g_last_log_time = 0;

static network_endpoint_t g_prev_endpoints[NETWORK_MAX_ENDPOINTS];
static int g_prev_endpoint_count = 0;
static int g_have_prev_endpoints = 0;

static recovery_state_t g_recovery;

/* Previous snapshot for change detection (quiet logs). */
static char g_prev_adbd[8];
static char g_prev_port[8];
static char g_prev_action[32];
static int  g_prev_recovery_enabled = -1;
static int  g_prev_established = -1;
static int  g_have_prev_snap = 0;

void watchdog_request_stop(void)
{
    g_watchdog_stop = 1;
}

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

/**
 * Decide whether to append a status log this cycle.
 * Always log on recovery/action change or service health change.
 * ESTABLISHED 0<->1 alone does not force log spam unless heartbeat.
 */
static int watchdog_should_log(const watchdog_status_t *status,
                               int recovery_acted,
                               int heartbeat_sec)
{
    time_t now;

    if (status == NULL) {
        return 0;
    }

    if (recovery_acted) {
        return 1;
    }

    if (!g_have_prev_snap) {
        return 1;
    }

    if (strcmp(status->adbd, g_prev_adbd) != 0) {
        return 1;
    }
    if (strcmp(status->port5555, g_prev_port) != 0) {
        return 1;
    }
    if (strcmp(status->last_action, g_prev_action) != 0) {
        return 1;
    }
    if (status->recovery_enabled != g_prev_recovery_enabled) {
        return 1;
    }

    /* Heartbeat only (does not treat ESTABLISHED flip as fault). */
    now = time(NULL);
    if (heartbeat_sec > 0 && g_last_log_time > 0 &&
        now != (time_t)-1 &&
        (now - g_last_log_time) >= (time_t)heartbeat_sec) {
        return 1;
    }

    (void)g_prev_established;
    return 0;
}

static void watchdog_remember_snap(const watchdog_status_t *status)
{
    if (status == NULL) {
        return;
    }
    util_strlcpy(g_prev_adbd, status->adbd, sizeof(g_prev_adbd));
    util_strlcpy(g_prev_port, status->port5555, sizeof(g_prev_port));
    util_strlcpy(g_prev_action, status->last_action, sizeof(g_prev_action));
    g_prev_recovery_enabled = status->recovery_enabled;
    g_prev_established = status->established;
    g_have_prev_snap = 1;
    g_last_log_time = time(NULL);
}

int watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status)
{
    network_tcp_stats_t tcp;
    int adbd_pid;
    int adbd_ok;
    int port_ok;
    int prop_ok;
    int new_conns;
    int recovery_acted;
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

    /* Optional safe test inject (one-shot file). */
    (void)inject_process_file();

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
    util_strlcpy(status->clients, tcp.clients, sizeof(status->clients));
    watchdog_set_primary_client(status);

    (void)property_get_str("persist.adb.tcp.port",
                           status->prop_tcp,
                           (unsigned int)sizeof(status->prop_tcp));
    prop_ok = property_adb_tcp_port_ok(WATCHDOG_ADB_PORT_STR);
    status->prop_ok = prop_ok;

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

    /*
     * Recovery uses adbd / port / prop only.
     * ESTABLISHED count is informational and never passed as a fault.
     */
    recovery_acted = recovery_evaluate(&g_recovery, cfg, adbd_ok, port_ok, prop_ok);

    util_strlcpy(status->last_action, g_recovery.last_action, sizeof(status->last_action));
    status->recovery_enabled = g_recovery.enabled;
    status->restart_count_window = g_recovery.restart_count;
    status->cooldown_remaining = recovery_cooldown_remaining(&g_recovery);

    if (cfg->status_update) {
        (void)status_write_file(WATCHDOG_STATUS_PATH, status);
    }

    if (cfg->log_enable &&
        watchdog_should_log(status, recovery_acted, cfg->log_heartbeat_sec)) {
        (void)logger_append_status(WATCHDOG_LOG_PATH, status, cfg->log_rotate_mb);
        watchdog_remember_snap(status);
    } else if (!g_have_prev_snap) {
        watchdog_remember_snap(status);
    } else {
        /* Still track action/health for next comparison without writing. */
        util_strlcpy(g_prev_adbd, status->adbd, sizeof(g_prev_adbd));
        util_strlcpy(g_prev_port, status->port5555, sizeof(g_prev_port));
        util_strlcpy(g_prev_action, status->last_action, sizeof(g_prev_action));
        g_prev_recovery_enabled = status->recovery_enabled;
        g_prev_established = status->established;
        g_have_prev_snap = 1;
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

        (void)watchdog_run_once(&local, &status);

        for (i = 0; i < interval && !g_watchdog_stop; i++) {
            (void)sleep(1);
        }
    }

    return 0;
}
