/**
 * @file status.c
 * @brief Status snapshot helpers and Version 2.0 status-file writer.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "status.h"
#include "config.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void status_set_time_now(watchdog_status_t *status)
{
    time_t     now;
    struct tm  tm_now;

    if (status == NULL) {
        return;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        util_strlcpy(status->time_str, "1970-01-01 00:00:00", sizeof(status->time_str));
        return;
    }

    if (localtime_r(&now, &tm_now) == NULL) {
        util_strlcpy(status->time_str, "1970-01-01 00:00:00", sizeof(status->time_str));
        return;
    }

    (void)strftime(status->time_str, sizeof(status->time_str),
                   "%Y-%m-%d %H:%M:%S", &tm_now);
}

/**
 * Write Version 2.0 status format (C# parseable), plus diagnostic extras.
 */
int status_write_file(const char *path, const watchdog_status_t *status)
{
    FILE *fp;
    const char *use_path;
    char tmp_path[256];
    int n;
    int written;
    const char *client;

    if (status == NULL) {
        return -1;
    }

    use_path = (path != NULL) ? path : WATCHDOG_STATUS_PATH;
    client = (status->client[0] != '\0') ? status->client : "";

    n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", use_path);
    if (n <= 0 || (size_t)n >= sizeof(tmp_path)) {
        return -1;
    }

    fp = fopen(tmp_path, "w");
    if (fp == NULL) {
        fp = fopen(use_path, "w");
        if (fp == NULL) {
            return -1;
        }
    }

    written = fprintf(fp,
                      "TIME=%s\n"
                      "ADBD=%s\n"
                      "PORT5555=%s\n"
                      "ESTABLISHED=%d\n"
                      "CLIENT=%s\n"
                      "LAST_ACTION=%s\n"
                      "ADBD_PID=%d\n"
                      "CLIENTS=%s\n"
                      "MAX_ESTABLISHED=%d\n"
                      "TOTAL_CONNECTIONS=%d\n"
                      "UPTIME=%ld\n"
                      "RECOVERY_ENABLED=%d\n"
                      "RESTART_COUNT=%d\n"
                      "TIME_WAIT=%d\n"
                      "CLOSE_WAIT=%d\n"
                      "SYN_RECV=%d\n",
                      status->time_str,
                      status->adbd,
                      status->port5555,
                      status->established,
                      client,
                      status->last_action,
                      status->adbd_pid,
                      status->clients,
                      status->max_established,
                      status->total_connections,
                      status->uptime_seconds,
                      status->recovery_enabled,
                      status->restart_count_window,
                      status->time_wait,
                      status->close_wait,
                      status->syn_recv);

    (void)fflush(fp);
    (void)fclose(fp);

    if (written <= 0) {
        (void)remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, use_path) != 0) {
        /* Fallback: already wrote to use_path if tmp open failed above. */
        FILE *src = fopen(tmp_path, "r");
        FILE *dst;
        char buf[256];
        size_t r;

        if (src == NULL) {
            return 0; /* wrote directly to use_path */
        }
        dst = fopen(use_path, "w");
        if (dst == NULL) {
            fclose(src);
            (void)remove(tmp_path);
            return -1;
        }
        while ((r = fread(buf, 1U, sizeof(buf), src)) > 0U) {
            if (fwrite(buf, 1U, r, dst) != r) {
                fclose(src);
                fclose(dst);
                (void)remove(tmp_path);
                return -1;
            }
        }
        fclose(src);
        fclose(dst);
        (void)remove(tmp_path);
    }

    return 0;
}
