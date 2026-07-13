/**
 * @file config.c
 * @brief Configuration file parser for Nexus ADB Watchdog.
 */

#include "config.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Apply built-in defaults to cfg.
 */
void config_set_defaults(watchdog_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->check_interval = WATCHDOG_DEFAULT_INTERVAL;
    cfg->log_enable     = 1;
    cfg->status_enable  = 1;
}

/**
 * Parse a KEY=VALUE line into cfg.
 *
 * @param line  Single configuration line.
 * @param cfg   Configuration to update.
 */
static void config_parse_line(char *line, watchdog_config_t *cfg)
{
    char *eq;
    char *key;
    char *val;
    long  n;

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

    if (strcmp(key, "CHECK_INTERVAL") == 0) {
        n = strtol(val, NULL, 10);
        if (n > 0L && n < 86400L) {
            cfg->check_interval = (int)n;
        }
    } else if (strcmp(key, "LOG_ENABLE") == 0) {
        n = strtol(val, NULL, 10);
        cfg->log_enable = (n != 0L) ? 1 : 0;
    } else if (strcmp(key, "STATUS_ENABLE") == 0) {
        n = strtol(val, NULL, 10);
        cfg->status_enable = (n != 0L) ? 1 : 0;
    }
    /* Unknown keys ignored for forward compatibility. */
}

/**
 * Load configuration from path.
 */
int config_load(const char *path, watchdog_config_t *cfg)
{
    FILE *fp;
    char  line[256];
    const char *use_path;

    if (cfg == NULL) {
        return -1;
    }

    config_set_defaults(cfg);

    use_path = (path != NULL) ? path : WATCHDOG_CONF_PATH;
    fp = fopen(use_path, "r");
    if (fp == NULL) {
        /* Missing config is not fatal; defaults already applied. */
        return 0;
    }

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        config_parse_line(line, cfg);
    }

    fclose(fp);
    return 0;
}

/**
 * Ensure the watchdog working directory exists.
 */
int config_ensure_directory(void)
{
    return util_mkdir_p(WATCHDOG_DIR_PATH);
}
