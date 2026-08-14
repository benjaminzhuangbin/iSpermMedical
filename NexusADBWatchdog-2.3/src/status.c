/**
 * @file status.c
 * @brief Status file writer for Version 2.3.
 * First line is always the product version string.
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
    time_t now;
    struct tm tm_now;

    if (status == NULL) {
        return;
    }
    now = time(NULL);
    if (now == (time_t)-1 || localtime_r(&now, &tm_now) == NULL) {
        util_strlcpy(status->time_str, "1970-01-01 00:00:00", sizeof(status->time_str));
        return;
    }
    (void)strftime(status->time_str, sizeof(status->time_str),
                   "%Y-%m-%d %H:%M:%S", &tm_now);
}

static int status_fprintf_all(FILE *fp, const watchdog_status_t *status)
{
    const char *client = (status->client[0] != '\0') ? status->client : "";

    /*
     * First line MUST be the version banner (user-visible requirement).
     */
    return fprintf(fp,
                   "%s\n"
                   "TIME=%s\n"
                   "ADBD_PID=%d\n"
                   "ADBD=%s\n"
                   "PORT5555=%s\n"
                   "ESTABLISHED=%d\n"
                   "CLIENT=%s\n"
                   "CLIENTS=%s\n"
                   "TCP_HEALTH=%s\n"
                   "ADB_HEALTH=%s\n"
                   "FAIL_REASON=%s\n"
                   "LAST_ACTION=%s\n"
                   "RECOVERY_ENABLED=%d\n"
                   "RECOVERY_COOLDOWN=%d\n"
                   "RESTART_COUNT=%d\n"
                   "RESTART_WINDOW=%d\n"
                   "COOLDOWN=%ld\n"
                   "TOTAL_CONNECTIONS=%d\n"
                   "UPTIME=%ld\n"
                   "PROP_TCP=%s\n"
                   "PROP_OK=%d\n"
                   "CLOSE_WAIT=%d\n"
                   "TIME_WAIT=%d\n"
                   "SYN_RECV=%d\n",
                   WATCHDOG_VERSION_STR,
                   status->time_str,
                   status->adbd_pid,
                   status->adbd,
                   status->port5555,
                   status->established,
                   client,
                   status->clients,
                   status->tcp_health,
                   status->adb_health,
                   status->fail_reason,
                   status->last_action,
                   status->recovery_enabled,
                   status->recovery_cooldown,
                   status->restart_count_window,
                   status->restart_window_sec,
                   status->cooldown_remaining,
                   status->total_connections,
                   status->uptime_seconds,
                   status->prop_tcp,
                   status->prop_ok,
                   status->close_wait,
                   status->time_wait,
                   status->syn_recv);
}

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
        written = status_fprintf_all(fp, status);
        (void)fflush(fp);
        (void)fclose(fp);
        return (written > 0) ? 0 : -1;
    }

    written = status_fprintf_all(fp, status);
    (void)fflush(fp);
    (void)fclose(fp);
    if (written <= 0) {
        (void)remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, use_path) != 0) {
        FILE *src = fopen(tmp_path, "r");
        FILE *dst;
        char buf[256];
        size_t r;

        if (src == NULL) {
            return -1;
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
