/**
 * @file watchdog.h
 * @brief Core monitor + recovery loop API (Version 2.0).
 *
 * Version 2.0 adds automatic TCP ADB recovery with rate limiting.
 * Future:
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

int  watchdog_run_once(const watchdog_config_t *cfg, watchdog_status_t *status);
int  watchdog_run_loop(const watchdog_config_t *cfg);
void watchdog_request_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_WATCHDOG_H */
