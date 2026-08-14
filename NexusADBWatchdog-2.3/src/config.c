/**
 * @file config.c
 * @brief Configuration parser for Version 2.3.
 */

#include "config.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_set_defaults(watchdog_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->interval = WATCHDOG_DEFAULT_INTERVAL;
    cfg->log_rotate_mb = WATCHDOG_DEFAULT_LOG_ROTATE_MB;
    cfg->status_update = 1;
    cfg->log_enable = 1;
    cfg->recovery_enable = 1;
    cfg->max_restart = WATCHDOG_DEFAULT_MAX_RESTART;
    cfg->restart_window_sec = WATCHDOG_DEFAULT_RESTART_WINDOW;
    cfg->cooldown_sec = WATCHDOG_DEFAULT_COOLDOWN_SEC;
    cfg->adbd_sleep_sec = WATCHDOG_DEFAULT_ADBD_SLEEP;
    cfg->log_heartbeat_sec = WATCHDOG_DEFAULT_LOG_HEARTBEAT_SEC;
    cfg->fault_close_wait = WATCHDOG_DEFAULT_FAULT_CLOSE_WAIT;
    cfg->fault_hold_sec = WATCHDOG_DEFAULT_FAULT_HOLD_SEC;
    cfg->adb_proto_check = WATCHDOG_DEFAULT_ADB_PROTO_CHECK;
    cfg->adb_proto_interval_sec = WATCHDOG_DEFAULT_ADB_PROTO_INTERVAL;
    cfg->adb_proto_timeout_ms = WATCHDOG_DEFAULT_ADB_PROTO_TIMEOUT;
}

static void config_parse_line(char *line, watchdog_config_t *cfg)
{
    char *eq;
    char *key;
    char *val;
    long n;

    if (line == NULL || cfg == NULL) {
        return;
    }
    util_trim(line);
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
        return;
    }
    eq = strchr(line, '=');
    if (eq == NULL) {
        return;
    }
    *eq = '\0';
    key = line;
    val = eq + 1;
    util_trim(key);
    util_trim(val);

    if (strcmp(key, "interval") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 86400L) cfg->interval = (int)n;
    } else if (strcmp(key, "log_rotate_mb") == 0) {
        n = strtol(val, NULL, 10);
        if (n >= 0L && n < 1024L) cfg->log_rotate_mb = (int)n;
    } else if (strcmp(key, "status_update") == 0) {
        cfg->status_update = (strtol(val, NULL, 10) != 0L) ? 1 : 0;
    } else if (strcmp(key, "recovery_enable") == 0) {
        cfg->recovery_enable = (strtol(val, NULL, 10) != 0L) ? 1 : 0;
    } else if (strcmp(key, "max_restart") == 0 || strcmp(key, "MAX_RESTART") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 100L) cfg->max_restart = (int)n;
    } else if (strcmp(key, "restart_window_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 86400L) cfg->restart_window_sec = (int)n;
    } else if (strcmp(key, "cooldown_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 86400L) cfg->cooldown_sec = (int)n;
    } else if (strcmp(key, "adbd_sleep_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 60L) cfg->adbd_sleep_sec = (int)n;
    } else if (strcmp(key, "log_heartbeat_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n >= 0L && n < 86400L) cfg->log_heartbeat_sec = (int)n;
    } else if (strcmp(key, "fault_close_wait") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 100L) cfg->fault_close_wait = (int)n;
    } else if (strcmp(key, "fault_hold_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 3600L) cfg->fault_hold_sec = (int)n;
    } else if (strcmp(key, "adb_proto_check") == 0) {
        cfg->adb_proto_check = (strtol(val, NULL, 10) != 0L) ? 1 : 0;
    } else if (strcmp(key, "adb_proto_interval_sec") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 86400L) cfg->adb_proto_interval_sec = (int)n;
    } else if (strcmp(key, "adb_proto_timeout_ms") == 0) {
        n = strtol(val, NULL, 10);
        if (n >= 100L && n <= 10000L) cfg->adb_proto_timeout_ms = (int)n;
    }
    /* no_client_timeout intentionally ignored (unsafe in older versions). */
}

int config_load(const char *path, watchdog_config_t *cfg)
{
    FILE *fp;
    char line[256];
    const char *use_path;

    if (cfg == NULL) {
        return -1;
    }
    config_set_defaults(cfg);
    use_path = (path != NULL) ? path : WATCHDOG_CONF_PATH;
    fp = fopen(use_path, "r");
    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        config_parse_line(line, cfg);
    }
    fclose(fp);
    return 0;
}

int config_ensure_directory(void)
{
    return util_mkdir_p(WATCHDOG_DIR_PATH);
}
