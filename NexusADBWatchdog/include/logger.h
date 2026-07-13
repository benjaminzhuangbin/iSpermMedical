/**
 * @file logger.h
 * @brief Append-only logging for Nexus ADB Watchdog.
 */

#ifndef NEXUS_WATCHDOG_LOGGER_H
#define NEXUS_WATCHDOG_LOGGER_H

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Append one monitor cycle to the log file.
 * Format:
 *   --------------------------------
 *   TIME=...
 *   ADBD=...
 *   PORT5555=...
 *   ESTABLISHED=...
 *
 * @param path    Log file path (may be NULL for default).
 * @param status  Current monitor status snapshot.
 * @return 0 on success, -1 on failure.
 */
int logger_append_status(const char *path, const watchdog_status_t *status);

/**
 * Append a free-form line to the log (optional helper for startup/shutdown).
 *
 * @param path  Log file path (may be NULL for default).
 * @param line  Text line without trailing newline (may be NULL).
 * @return 0 on success, -1 on failure.
 */
int logger_append_line(const char *path, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_LOGGER_H */
