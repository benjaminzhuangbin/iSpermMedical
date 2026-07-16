/**
 * @file config.h
 * @brief Configuration for Nexus ADB Watchdog Version 1.5 (diagnostics).
 *
 * Version 1.5 is monitor / data-collection only. No recovery actions.
 */

#ifndef NEXUS_WATCHDOG_CONFIG_H
#define NEXUS_WATCHDOG_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/** Default path of the configuration file on the device. */
#define WATCHDOG_CONF_PATH          "/data/local/watchdog/watchdog.conf"

/** Runtime working directory. */
#define WATCHDOG_DIR_PATH           "/data/local/watchdog"

/** Status file path (overwritten each cycle). */
#define WATCHDOG_STATUS_PATH        "/data/local/watchdog/watchdog.status"

/** Log file path (appended each cycle). */
#define WATCHDOG_LOG_PATH           "/data/local/watchdog/watchdog.log"

/** Rotated log path (previous log). */
#define WATCHDOG_LOG_ROTATED_PATH   "/data/local/watchdog/watchdog.log.1"

/** PID file path. */
#define WATCHDOG_PID_PATH           "/data/local/watchdog/watchdog.pid"

/** Expected adbd executable path. */
#define WATCHDOG_ADBD_PATH          "/sbin/adbd"

/** ADB TCP port to monitor. */
#define WATCHDOG_ADB_PORT           5555

/** Default check interval in seconds (Version 1.5). */
#define WATCHDOG_DEFAULT_INTERVAL   1

/** Default log rotate threshold in megabytes. */
#define WATCHDOG_DEFAULT_LOG_ROTATE_MB  10

/**
 * Runtime configuration.
 */
typedef struct watchdog_config {
    int interval;         /**< Seconds between monitor cycles (default 1). */
    int log_rotate_mb;    /**< Rotate log when size exceeds this many MB. */
    int status_update;    /**< 1 = write status file each cycle, 0 = disable. */
    int log_enable;       /**< 1 = append log each cycle (always on for 1.5). */
} watchdog_config_t;

/**
 * Load configuration from path.
 * Accepts Version 1.5 keys (interval, log_rotate_mb, status_update)
 * and Version 1 keys (CHECK_INTERVAL, LOG_ENABLE, STATUS_ENABLE) for
 * forward/backward friendliness.
 *
 * @param path  Absolute path to watchdog.conf (may be NULL for default).
 * @param cfg   Output configuration (must not be NULL).
 * @return 0 on success, -1 on hard failure.
 */
int config_load(const char *path, watchdog_config_t *cfg);

/**
 * Apply built-in Version 1.5 defaults to cfg.
 *
 * @param cfg  Configuration to initialize (must not be NULL).
 */
void config_set_defaults(watchdog_config_t *cfg);

/**
 * Ensure the watchdog working directory exists.
 *
 * @return 0 on success, -1 on failure.
 */
int config_ensure_directory(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_CONFIG_H */
