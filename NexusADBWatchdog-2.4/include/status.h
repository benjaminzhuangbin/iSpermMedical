/**
 * @file status.h
 * @brief Status snapshot (Version 2.4).
 */

#ifndef NEXUS_WATCHDOG_STATUS_H
#define NEXUS_WATCHDOG_STATUS_H

#include "network.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct watchdog_status {
    char time_str[32];
    int  adbd_pid;
    char adbd[8];
    char port5555[8];
    int  established;
    int  time_wait;
    int  close_wait;
    int  syn_recv;

    int max_established;
    int total_connections;
    long uptime_seconds;

    char clients[NETWORK_CLIENTS_BUF_LEN];
    char client[NETWORK_IP_STR_LEN];
    char last_action[32];

    char tcp_health[16];
    char adb_health[16];
    char client_state[16];
    char fail_reason[32];

    int recovery_enabled;       /**< From conf policy (1=allowed). Never latched off. */
    int recovery_cooldown;      /**< 1 while in post-recovery cooldown. */
    int restart_count_window;
    int restart_window_sec;
    long cooldown_remaining;

    char prop_tcp[32];
    int  prop_ok;
} watchdog_status_t;

void status_set_time_now(watchdog_status_t *status);
int  status_write_file(const char *path, const watchdog_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_STATUS_H */
