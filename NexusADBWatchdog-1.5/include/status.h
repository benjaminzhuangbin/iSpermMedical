/**
 * @file status.h
 * @brief Diagnostic status snapshot and status-file writer (Version 1.5).
 */

#ifndef NEXUS_WATCHDOG_STATUS_H
#define NEXUS_WATCHDOG_STATUS_H

#include "network.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One monitor cycle diagnostic snapshot (Version 1.5).
 * Plain-text fields are intended for later C# WinForms parsing.
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

    int max_established;        /**< Max ESTABLISHED since start */
    int total_connections;      /**< Cumulative newly observed connections */
    long uptime_seconds;        /**< Seconds since watchdog start */

    char clients[NETWORK_CLIENTS_BUF_LEN]; /**< Comma-separated client IPs */
} watchdog_status_t;

/**
 * Fill time_str with local wall-clock time.
 *
 * @param status  Status object to update (must not be NULL).
 */
void status_set_time_now(watchdog_status_t *status);

/**
 * Overwrite the status file with the Version 1.5 snapshot format.
 *
 * @param path    Status file path (may be NULL for default).
 * @param status  Snapshot to write.
 * @return 0 on success, -1 on failure.
 */
int status_write_file(const char *path, const watchdog_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_STATUS_H */
