/**
 * @file status.c
 * @brief Status snapshot helpers and Version 1.5 status-file writer.
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

/**
 * Fill time_str with local wall-clock time "YYYY-MM-DD HH:MM:SS".
 */
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
 * Overwrite the status file with the Version 1.5 snapshot.
 */
int status_write_file(const char *path, const watchdog_status_t *status)
{
    FILE *fp;
    const char *use_path;
    char tmp_path[256];
    int n;
    int written;

    if (status == NULL) {
        return -1;
    }

    use_path = (path != NULL) ? path : WATCHDOG_STATUS_PATH;

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
        written = fprintf(fp,
                          "TIME=%s\n"
                          "ADBD_PID=%d\n"
                          "ADBD=%s\n"
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
                          "MAX_ESTABLISHED=%d\n"
                          "TOTAL_CONNECTIONS=%d\n"
                          "UPTIME=%ld\n"
                          "CLIENTS=%s\n",
                          status->time_str,
                          status->adbd_pid,
                          status->adbd,
                          status->port5555,
                          status->established,
                          status->time_wait,
                          status->close_wait,
                          status->syn_recv,
                          status->fin_wait1,
                          status->fin_wait2,
                          status->last_ack,
                          status->closing,
                          status->close_state,
                          status->max_established,
                          status->total_connections,
                          status->uptime_seconds,
                          status->clients);
        (void)fflush(fp);
        (void)fclose(fp);
        return (written > 0) ? 0 : -1;
    }

    written = fprintf(fp,
                      "TIME=%s\n"
                      "ADBD_PID=%d\n"
                      "ADBD=%s\n"
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
                      "MAX_ESTABLISHED=%d\n"
                      "TOTAL_CONNECTIONS=%d\n"
                      "UPTIME=%ld\n"
                      "CLIENTS=%s\n",
                      status->time_str,
                      status->adbd_pid,
                      status->adbd,
                      status->port5555,
                      status->established,
                      status->time_wait,
                      status->close_wait,
                      status->syn_recv,
                      status->fin_wait1,
                      status->fin_wait2,
                      status->last_ack,
                      status->closing,
                      status->close_state,
                      status->max_established,
                      status->total_connections,
                      status->uptime_seconds,
                      status->clients);
    (void)fflush(fp);
    (void)fclose(fp);

    if (written <= 0) {
        (void)remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, use_path) != 0) {
        FILE *src;
        FILE *dst;
        char  buf[256];
        size_t r;

        src = fopen(tmp_path, "r");
        dst = fopen(use_path, "w");
        if (src == NULL || dst == NULL) {
            if (src != NULL) {
                fclose(src);
            }
            if (dst != NULL) {
                fclose(dst);
            }
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
