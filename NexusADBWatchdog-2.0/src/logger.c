/**
 * @file logger.c
 * @brief Append-only logging with rotation (Version 2.0).
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

    (void)unlink(WATCHDOG_LOG_ROTATED_PATH);
    (void)rename(path, WATCHDOG_LOG_ROTATED_PATH);
    return 0;
}

/**
 * Append Version 2.0 human-readable cycle block.
 */
int logger_append_status(const char *path,
                         const watchdog_status_t *status,
                         int rotate_mb)
{
    FILE *fp;
    const char *use_path;
    int written;
    const char *adbd_line;
    const char *port_line;
    const char *tcp_line;

    if (status == NULL) {
        return -1;
    }

    use_path = (path != NULL) ? path : WATCHDOG_LOG_PATH;
    (void)logger_rotate_if_needed(use_path, rotate_mb);

    fp = fopen(use_path, "a");
    if (fp == NULL) {
        return -1;
    }

    adbd_line = (strcmp(status->adbd, "YES") == 0) ? "ADBD OK" : "ADBD LOST";
    port_line = (strcmp(status->port5555, "YES") == 0) ? "PORT 5555 OK" : "PORT 5555 NOT LISTEN";

    if (status->established > 0 && status->client[0] != '\0') {
        tcp_line = "TCP CLIENT";
    } else if (status->established > 0) {
        tcp_line = "TCP CLIENT";
    } else {
        tcp_line = "TCP CLIENT NONE";
    }

    if (status->established > 0 && status->client[0] != '\0') {
        written = fprintf(fp,
                          "\n%s\n"
                          "%s\n"
                          "%s\n"
                          "%s %s\n"
                          "ESTABLISHED=%d\n"
                          "LAST_ACTION=%s\n",
                          status->time_str,
                          adbd_line,
                          port_line,
                          tcp_line,
                          status->client,
                          status->established,
                          status->last_action);
    } else {
        written = fprintf(fp,
                          "\n%s\n"
                          "%s\n"
                          "%s\n"
                          "%s\n"
                          "ESTABLISHED=%d\n"
                          "LAST_ACTION=%s\n",
                          status->time_str,
                          adbd_line,
                          port_line,
                          tcp_line,
                          status->established,
                          status->last_action);
    }

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
