/**
 * @file process.h
 * @brief Process existence / PID lookup via /proc (no shell).
 */

#ifndef NEXUS_WATCHDOG_PROCESS_H
#define NEXUS_WATCHDOG_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check whether a process whose executable path matches exe_path exists.
 *
 * @param exe_path  Expected path, e.g. "/sbin/adbd".
 * @return 1 if found, 0 if not found, -1 on scan error.
 */
int process_is_running(const char *exe_path);

/**
 * Find PID of a process matching exe_path.
 *
 * @param exe_path  Expected path, e.g. "/sbin/adbd".
 * @return PID (>0) if found, -1 if not found or on error.
 */
int process_find_pid(const char *exe_path);

/**
 * Write current process PID to path.
 *
 * @param path  PID file path (may be NULL for default).
 * @return 0 on success, -1 on failure.
 */
int process_write_pid_file(const char *path);

/**
 * Remove PID file if it contains our PID (best effort).
 *
 * @param path  PID file path (may be NULL for default).
 */
void process_remove_pid_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_PROCESS_H */
