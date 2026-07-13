/**
 * @file watchdog.c
 * @brief Core monitor cycle and forever loop (Version 1 — monitor only).
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
#include <unistd.h>

/** Set to 1 by signal handler / request_stop to exit the loop. */
static volatile sig_atomic_t g_watchdog_stop = 0;

/**
 * Request the monitor loop to exit after the current cycle.
 */
void watchdog_request_stop(void)
{
    g_watchdog_stop = 1;
}

/**
 * Perform one monitoring cycle and fill status.
 */
int watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status)
{
    int adbd;
    int listen_rc;
    int est;

    if (cfg == NULL || status == NULL) {
        return -1;
    }

    memset(status, 0, sizeof(*status));
    status_set_time_now(status);

    /* Check 1: adbd process */
    adbd = process_is_running(WATCHDOG_ADBD_PATH);
    if (adbd == 1) {
        util_strlcpy(status->adbd, STATUS_ADBD_OK, sizeof(status->adbd));
    } else {
        util_strlcpy(status->adbd, STATUS_ADBD_LOST, sizeof(status->adbd));
    }

    /* Check 2: port 5555 listening */
    listen_rc = network_is_port_listening((unsigned int)WATCHDOG_ADB_PORT);
    if (listen_rc == 1) {
        util_strlcpy(status->port5555, STATUS_PORT_LISTEN, sizeof(status->port5555));
    } else {
        util_strlcpy(status->port5555, STATUS_PORT_NOT_LISTEN, sizeof(status->port5555));
    }

    /* Check 3: ESTABLISHED count (parse /proc/net/tcp; no shell/wc). */
    est = network_count_established((unsigned int)WATCHDOG_ADB_PORT);
    if (est < 0) {
        status->established = 0;
    } else {
        status->established = est;
    }

    /* Persist outputs according to config. */
    if (cfg->status_enable) {
        (void)status_write_file(WATCHDOG_STATUS_PATH, status);
    }
    if (cfg->log_enable) {
        (void)logger_append_status(WATCHDOG_LOG_PATH, status);
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
    interval = local.check_interval;
    if (interval < 1) {
        interval = WATCHDOG_DEFAULT_INTERVAL;
    }

    g_watchdog_stop = 0;

    while (!g_watchdog_stop) {
        watchdog_status_t status;
        int i;

        /* Reload config each cycle so operators can tune without restart. */
        (void)config_load(WATCHDOG_CONF_PATH, &local);
        interval = local.check_interval;
        if (interval < 1) {
            interval = WATCHDOG_DEFAULT_INTERVAL;
        }

        (void)watchdog_run_once(&local, &status);

        /*
         * Sleep in 1-second slices so SIGTERM can stop promptly.
         * Version 1 remains monitoring-only; no recovery actions.
         */
        for (i = 0; i < interval && !g_watchdog_stop; i++) {
            (void)sleep(1);
        }
    }

    return 0;
}
