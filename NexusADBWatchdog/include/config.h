/**
 * @file config.h
 * @brief Configuration loading and defaults for Nexus ADB Watchdog.
 *
 * Version 1: interval and feature toggles only.
 * Designed so Version 2+ recovery / heartbeat options can be added
 * without breaking the existing API.
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

/** PID file path. */
#define WATCHDOG_PID_PATH           "/data/local/watchdog/watchdog.pid"

/** Expected adbd executable path. */
#define WATCHDOG_ADBD_PATH          "/sbin/adbd"

/** ADB TCP port to monitor. */
#define WATCHDOG_ADB_PORT           5555

/** Default check interval in seconds. */
#define WATCHDOG_DEFAULT_INTERVAL   10

/**
 * Runtime configuration.
 * Additional fields for future versions should be appended at the end.
 */
typedef struct watchdog_config {
    int check_interval;   /**< Seconds between monitor cycles. */
    int log_enable;       /**< 1 = append to log file, 0 = disable. */
    int status_enable;    /**< 1 = write status file, 0 = disable. */
} watchdog_config_t;

/**
 * Load configuration from path.
 * Missing keys keep defaults. Missing file uses defaults and returns 0.
 *
 * @param path  Absolute path to watchdog.conf (may be NULL for default path).
 * @param cfg   Output configuration (must not be NULL).
 * @return 0 on success, -1 on hard failure.
 */
int config_load(const char *path, watchdog_config_t *cfg);

/**
 * Apply built-in defaults to cfg.
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
