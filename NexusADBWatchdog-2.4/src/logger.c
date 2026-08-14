/**
 * @file logger.c
 * @brief Logging for Version 2.4 (change / recovery oriented).
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

int logger_rotate_if_needed(const char *path, int rotate_mb)
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
    (void)unlink(WATCHDOG_LOG_ROTATED_PATH);
    (void)rename(path, WATCHDOG_LOG_ROTATED_PATH);
    return 0;
}

int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb)
{
    FILE *fp;
    const char *use_path;
    int written;

    if (status == NULL) {
        return -1;
    }
    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;
    (void)logger_rotate_if_needed(use_path, rotate_mb);
    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }

    written = fprintf(fp,
                      "\n%s\n"
                      "%s\n"
                      "ADBD=%s PORT5555=%s ESTABLISHED=%d CLIENT_STATE=%s CLIENT=%s\n"
                      "TCP_HEALTH=%s ADB_HEALTH=%s FAIL_REASON=%s\n"
                      "LAST_ACTION=%s RECOVERY_ENABLED=%d RECOVERY_COOLDOWN=%d\n"
                      "RESTART_COUNT=%d RESTART_WINDOW=%d COOLDOWN=%ld\n",
                      status->time_str,
                      WATCHDOG_VERSION_STR,
                      status->adbd,
                      status->port5555,
                      status->established,
                      status->client_state,
                      (status->client[0] != '\0') ? status->client : "-",
                      status->tcp_health,
                      status->adb_health,
                      status->fail_reason,
                      status->last_action,
                      status->recovery_enabled,
                      status->recovery_cooldown,
                      status->restart_count_window,
                      status->restart_window_sec,
                      status->cooldown_remaining);

    (void)fflush(fp);
    (void)fclose(fp);
    return (written > 0) ? 0 : -1;
}

int logger_append_event(const char *path, const char *text)
{
    FILE *fp;
    const char *use_path;
    int written;

    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;
    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }
    if (text == NULL) {
        written = fprintf(fp, "\n");
    } else {
        written = fprintf(fp, "%s", text);
        if (text[0] != '\0' && text[strlen(text) - 1U] != '\n') {
            written += fprintf(fp, "\n");
        }
    }
    (void)fflush(fp);
    (void)fclose(fp);
    return (written > 0) ? 0 : -1;
}

int logger_append_line(const char *path, const char *line)
{
    return logger_append_event(path, line);
}
