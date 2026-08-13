/**
 * @file inject.h
 * @brief Safe test inject file for simulating CASE A / CASE B.
 */

#ifndef NEXUS_WATCHDOG_INJECT_H
#define NEXUS_WATCHDOG_INJECT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process /data/local/watchdog/watchdog.inject if present, then delete it.
 *
 * Supported one-line commands:
 *   STOP_ADBD     - ctl.stop adbd  (CASE A simulation)
 *   BREAK_PORT    - clear TCP port + restart adbd (CASE B / C simulation)
 *
 * @return 1 if an inject action ran, 0 if none.
 */
int inject_process_file(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_INJECT_H */
