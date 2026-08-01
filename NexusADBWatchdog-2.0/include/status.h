/**
 * @file status.h
 * @brief Status snapshot and status-file writer (Version 2.0).
 */

#ifndef NEXUS_WATCHDOG_STATUS_H
#define NEXUS_WATCHDOG_STATUS_H

#include "network.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One monitor cycle snapshot (Version 2.0).
 * Plain-text fields are intended for C# WinForms parsing.
 */
typedef struct watchdog_status {
    char time_str[32];          /**< "YYYY-MM-DD HH:MM:SS" */
    int  adbd_pid;              /**< adbd PID, or -1 if not found */
    char adbd[8];               /**< "YES" or "NO" */
    char port5555[8];           /**< "YES" or "NO" */

    int established;
    int time_wait;
    int close_wait;
    int syn_recv;
    int fin_wait1;
    int fin_wait2;
    int last_ack;
    int closing;
    int close_state;

    int max_established;
    int total_connections;
    long uptime_seconds;

    char clients[NETWORK_CLIENTS_BUF_LEN]; /**< Comma-separated client IPs */
    char client[NETWORK_IP_STR_LEN];       /**< First / primary client (CLIENT=) */
    char last_action[32];                  /**< NONE / START_ADBD / RESTART_ADBD / ... */

    int recovery_enabled;       /**< 1 if auto-recovery still allowed */
    int restart_count_window;   /**< Restarts inside current rate-limit window */
} watchdog_status_t;

void status_set_time_now(watchdog_status_t *status);
int  status_write_file(const char *path, const watchdog_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_STATUS_H */
