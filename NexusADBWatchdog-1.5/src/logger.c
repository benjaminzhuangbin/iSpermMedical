/**
 * @file logger.c
 * @brief Append-only cycle logging with size-based rotation (Version 1.5).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "logger.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Rotate log if it exceeds rotate_mb megabytes.
 * Renames path -> rotated path (watchdog.log.1), then a new file is created
 * on the next append.
 *
 * @return 0 always (rotation failures are non-fatal).
 */
static int logger_rotate_if_needed(const char *path, int rotate_mb)
{
    struct stat st;
    long long limit;
    long long size;

    if (path == NULL || rotate_mb <= 0) {
        return 0;
    }

    if (stat(path, &st) != 0) {
        return 0;
    }

    size = (long long)st.st_size;
    limit = (long long)rotate_mb * 1024LL * 1024LL;

    if (size <= limit) {
        return 0;
    }

    /* Remove previous rotated file, then rename current log. */
    (void)unlink(WATCHDOG_LOG_ROTATED_PATH);
    if (rename(path, WATCHDOG_LOG_ROTATED_PATH) != 0) {
        /* Non-fatal: continue appending to existing log. */
        return 0;
    }

    return 0;
}

/**
 * Append one diagnostic cycle block to the log file.
 */
int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb)
{
    FILE *fp;
    const char *use_path;
    int written;
    const char *port_str;

    if (status == NULL) {
        return -1;
    }

    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;

    (void)logger_rotate_if_needed(use_path, rotate_mb);

    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }

    /* Log uses LISTEN/NOT_LISTEN wording to match diagnostic example. */
    if (strcmp(status->port5555, "YES") == 0) {
        port_str = "LISTEN";
    } else {
        port_str = "NOT_LISTEN";
    }

    written = fprintf(fp,
                      "----------------------------------------\n"
                      "TIME=%s\n"
                      "ADBD PID=%d\n"
                      "PORT5555=%s\n"
                      "ESTABLISHED=%d\n"
                      "TIME_WAIT=%d\n"
                      "CLOSE_WAIT=%d\n"
                      "SYN_RECV=%d\n"
                      "FIN_WAIT1=%d\n"
                      "FIN_WAIT2=%d\n"
                      "LAST_ACK=%d\n"
                      "CLOSING=%d\n"
                      "CLOSE=%d\n"
                      "CLIENTS=%s\n"
                      "TOTAL_CONNECTIONS=%d\n"
                      "MAX_ESTABLISHED=%d\n"
                      "UPTIME=%ld\n"
                      "----------------------------------------\n",
                      status->time_str,
                      status->adbd_pid,
                      port_str,
                      status->established,
                      status->time_wait,
                      status->close_wait,
                      status->syn_recv,
                      status->fin_wait1,
                      status->fin_wait2,
                      status->last_ack,
                      status->closing,
                      status->close_state,
                      status->clients,
                      status->total_connections,
                      status->max_established,
                      status->uptime_seconds);

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
