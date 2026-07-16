/**
 * @file watchdog.h
 * @brief Core diagnostic monitor loop API (Version 1.5).
 *
 * Version 1.5: diagnostics and data collection only.
 * Does NOT restart adbd, kill processes, reboot, or setprop.
 *
 * Future (not implemented):
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
 * Perform one diagnostic monitoring cycle and fill status.
 *
 * @param cfg     Active configuration (must not be NULL).
 * @param status  Output snapshot (must not be NULL).
 * @return 0 on success, -1 if a critical I/O error occurred.
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
 * Safe to call from a signal handler.
 */
void watchdog_request_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_WATCHDOG_H */
