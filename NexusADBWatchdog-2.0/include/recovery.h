/**
 * @file recovery.h
 * @brief Automatic TCP ADB recovery actions for Version 2.0.
 *
 * Case A: adbd missing          -> start adbd
 * Case B: port 5555 not listen  -> setprop + stop + sleep + start
 * Case C: no ESTABLISHED client -> wait timeout, then same as B
 *
 * Rate-limited: max_restart within restart_window_sec, then disable
 * further recovery and write watchdog.error.
 */

#ifndef NEXUS_WATCHDOG_RECOVERY_H
#define NEXUS_WATCHDOG_RECOVERY_H

#include "config.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** LAST_ACTION / recovery result codes (string values used in status). */
#define RECOVERY_ACTION_NONE              "NONE"
#define RECOVERY_ACTION_START_ADBD        "START_ADBD"
#define RECOVERY_ACTION_RESTART_ADBD      "RESTART_ADBD"
#define RECOVERY_ACTION_RECOVERY_DISABLED "RECOVERY_DISABLED"
#define RECOVERY_ACTION_RATE_LIMITED      "RATE_LIMITED"

/**
 * Recovery runtime state (kept across monitor cycles).
 */
typedef struct recovery_state {
    int     enabled;                 /**< Cleared when rate limit trips. */
    int     restart_count;           /**< Restarts inside current window. */
    time_t  window_start;            /**< Start of current rate-limit window. */
    time_t  no_client_since;         /**< First time ESTABLISHED became 0; 0=N/A */
    char    last_action[32];         /**< LAST_ACTION string for status file. */
    int     recoveries_total;        /**< Lifetime recovery attempts. */
} recovery_state_t;

void recovery_init(recovery_state_t *st, const watchdog_config_t *cfg);

int recovery_evaluate(recovery_state_t *st,
                      const watchdog_config_t *cfg,
                      int adbd_ok,
                      int port_ok,
                      int established);

int recovery_write_error_file(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_RECOVERY_H */
