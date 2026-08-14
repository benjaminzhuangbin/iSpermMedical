/**
 * @file util.c
 * @brief Small shared helpers (string trim, safe copy, mkdir).
 */

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Trim leading/trailing whitespace in place.
 *
 * @param s  Mutable C string (may be NULL).
 */
void util_trim(char *s)
{
    char *start;
    char *end;
    size_t len;

    if (s == NULL) {
        return;
    }

    start = s;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1U);
    }

    len = strlen(s);
    if (len == 0U) {
        return;
    }

    end = s + len - 1U;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
}

/**
 * Copy string with guaranteed NUL termination.
 *
 * @param dst   Destination buffer.
 * @param src   Source string (may be NULL).
 * @param size  Destination capacity in bytes.
 */
void util_strlcpy(char *dst, const char *src, size_t size)
{
    size_t i;

    if (dst == NULL || size == 0U) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (i = 0U; i + 1U < size && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/**
 * Create a directory if it does not exist (mode 0755).
 *
 * @param path  Directory path.
 * @return 0 on success (exists or created), -1 on failure.
 */
int util_mkdir_p(const char *path)
{
    struct stat st;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }

    if (mkdir(path, 0755) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    return -1;
}
