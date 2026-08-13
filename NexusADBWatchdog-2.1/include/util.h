/**
 * @file util.h
 * @brief Shared utility helpers.
 */

#ifndef NEXUS_WATCHDOG_UTIL_H
#define NEXUS_WATCHDOG_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Trim leading/trailing whitespace in place. */
void util_trim(char *s);

/** Copy string with guaranteed NUL termination. */
void util_strlcpy(char *dst, const char *src, size_t size);

/** Create directory if missing (mode 0755). */
int util_mkdir_p(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_UTIL_H */
