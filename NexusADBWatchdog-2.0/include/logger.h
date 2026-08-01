/**
 * @file logger.h
 * @brief Append-only logging with rotation (Version 2.0).
 */

#ifndef NEXUS_WATCHDOG_LOGGER_H
#define NEXUS_WATCHDOG_LOGGER_H

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Append one Version 2.0 human-readable cycle block.
 *
 * @param path       Log path (NULL = default).
 * @param status     Snapshot.
 * @param rotate_mb  Rotate threshold in MB.
 * @return 0 on success, -1 on failure.
 */
int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb);

/**
 * Append a free-form multi-line event (recovery messages, etc.).
 *
 * @param path  Log path (NULL = default).
 * @param text  Text block (may contain newlines; no trailing NUL issues).
 * @return 0 on success, -1 on failure.
 */
int logger_append_event(const char *path, const char *text);

/**
 * Append a single line.
 */
int logger_append_line(const char *path, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_LOGGER_H */
