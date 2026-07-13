/**
 * @file logger.c
 * @brief Append-only cycle logging.
 */

#include "logger.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

/**
 * Append one monitor cycle to the log file.
 */
int logger_append_status(const char *path, const watchdog_status_t *status)
{
    FILE *fp;
    const char *use_path;
    int written;

    if (status == NULL) {
        return -1;
    }

    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;
    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }

    written = fprintf(fp,
                      "--------------------------------\n"
                      "TIME=%s\n"
                      "ADBD=%s\n"
                      "PORT5555=%s\n"
                      "ESTABLISHED=%d\n",
                      status->time_str,
                      status->adbd,
                      status->port5555,
                      status->established);
    (void)fflush(fp);
    (void)fclose(fp);

    return (written > 0) ? 0 : -1;
}

/**
 * Append a free-form line to the log.
 */
int logger_append_line(const char *path, const char *line)
{
    FILE *fp;
    const char *use_path;
    int written;

    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;
    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }

    if (line == NULL) {
        written = fprintf(fp, "\n");
    } else {
        written = fprintf(fp, "%s\n", line);
    }

    (void)fflush(fp);
    (void)fclose(fp);

    return (written > 0) ? 0 : -1;
}
