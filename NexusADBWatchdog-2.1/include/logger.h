/**
 * @file logger.h
 * @brief Change/recovery-oriented logging (Version 2.1).
 */

#ifndef NEXUS_WATCHDOG_LOGGER_H
#define NEXUS_WATCHDOG_LOGGER_H

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb);

int logger_append_event(const char *path, const char *text);
int logger_append_line(const char *path, const char *line);

/**
 * Rotate if needed (exposed for event path).
 */
int logger_rotate_if_needed(const char *path, int rotate_mb);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_LOGGER_H */
