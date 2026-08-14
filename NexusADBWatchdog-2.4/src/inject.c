/**
 * @file inject.c
 * @brief Process one-shot Android-side fault inject commands (Version 2.4).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "inject.h"
#include "config.h"
#include "logger.h"
#include "property.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int inject_ctl(const char *ctl_name, const char *svc)
{
    return property_set_str(ctl_name, svc);
}

int inject_process_file(void)
{
    FILE *fp;
    char line[128];
    int acted = 0;

    fp = fopen(WATCHDOG_INJECT_PATH, "r");
    if (fp == NULL) {
        return 0;
    }

    if (fgets(line, (int)sizeof(line), fp) == NULL) {
        fclose(fp);
        (void)unlink(WATCHDOG_INJECT_PATH);
        return 0;
    }
    fclose(fp);
    (void)unlink(WATCHDOG_INJECT_PATH);

    util_trim(line);
    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }

    if (strcmp(line, "STOP_ADBD") == 0) {
        (void)logger_append_line(WATCHDOG_LOG_PATH,
                                 "INJECT: STOP_ADBD (test CASE A — Android-side)");
        (void)inject_ctl("ctl.stop", "adbd");
        acted = 1;
    } else if (strcmp(line, "BREAK_PORT") == 0) {
        (void)logger_append_line(WATCHDOG_LOG_PATH,
                                 "INJECT: BREAK_PORT (test CASE B/C — Android-side)");
        (void)property_set_str("persist.adb.tcp.port", "0");
        (void)property_set_str("service.adb.tcp.port", "0");
        (void)inject_ctl("ctl.stop", "adbd");
        (void)sleep(2);
        (void)inject_ctl("ctl.start", "adbd");
        acted = 1;
    } else if (strcmp(line, "CLEAR_TCP_PORT") == 0) {
        (void)logger_append_line(WATCHDOG_LOG_PATH,
                                 "INJECT: CLEAR_TCP_PORT (test CASE C — Android-side)");
        (void)property_set_str("persist.adb.tcp.port", "0");
        acted = 1;
    } else {
        (void)logger_append_line(WATCHDOG_LOG_PATH,
                                 "INJECT: unknown command ignored (CNXN/protocol inject removed in 2.4)");
    }

    return acted;
}
