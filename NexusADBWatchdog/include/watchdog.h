/**
 * @file watchdog.h
 * @brief Core monitor loop API for Nexus ADB Watchdog (Version 1).
 *
 * Version 1: monitoring only — never restarts adbd or mutates system state.
 *
 * Future hooks (not implemented yet):
 *   V2 - automatic adbd recovery
 *   V3 - medical application monitoring
 *   V4 - heartbeat with Windows WinForms host
 */

#ifndef NEXUS_WATCHDOG_WATCHDOG_H
#define NEXUS_WATCHDOG_WATCHDOG_H

#include "config.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Perform one monitoring cycle and fill status.
 *
 * Checks:
 *   1) /sbin/adbd process present
 *   2) TCP port 5555 listening
 *   3) ESTABLISHED connection count on port 5555
 *
 * @param cfg     Active configuration (must not be NULL).
 * @param status  Output snapshot (must not be NULL).
 * @return 0 on success (checks completed), -1 if a critical I/O error occurred.
 */
int watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status);

/**
 * Run the forever monitor loop until a stop flag is set.
 *
 * @param cfg  Active configuration (must not be NULL).
 * @return 0 on clean stop, -1 on fatal error.
 */
int watchdog_run_loop(const watchdog_config_t *cfg);

/**
 * Request the monitor loop to exit after the current cycle.
 * Safe to call from a signal handler (writes a volatile flag).
 */
void watchdog_request_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_WATCHDOG_H */
