/**
 * @file logger.h
 * @brief Append-only logging with size-based rotation (Version 1.5).
 */

#ifndef NEXUS_WATCHDOG_LOGGER_H
#define NEXUS_WATCHDOG_LOGGER_H

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Append one diagnostic cycle block to the log file.
 * Never overwrites existing content (append only).
 * Performs rotation when size exceeds rotate_mb megabytes:
 *   watchdog.log -> watchdog.log.1, then create a new watchdog.log.
 *
 * @param path       Log file path (may be NULL for default).
 * @param status     Current diagnostic snapshot.
 * @param rotate_mb  Rotate threshold in MB (<=0 disables rotation).
 * @return 0 on success, -1 on failure.
 */
int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb);

/**
 * Append a free-form line to the log (startup/shutdown markers).
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
