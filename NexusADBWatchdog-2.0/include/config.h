/**
 * @file config.h
 * @brief Configuration for Nexus ADB Watchdog Version 2.0 (auto-recovery).
 */

#ifndef NEXUS_WATCHDOG_CONFIG_H
#define NEXUS_WATCHDOG_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define WATCHDOG_CONF_PATH          "/data/local/watchdog/watchdog.conf"
#define WATCHDOG_DIR_PATH           "/data/local/watchdog"
#define WATCHDOG_STATUS_PATH        "/data/local/watchdog/watchdog.status"
#define WATCHDOG_LOG_PATH           "/data/local/watchdog/watchdog.log"
#define WATCHDOG_LOG_ROTATED_PATH   "/data/local/watchdog/watchdog.log.1"
#define WATCHDOG_ERROR_PATH         "/data/local/watchdog/watchdog.error"
#define WATCHDOG_PID_PATH           "/data/local/watchdog/watchdog.pid"
#define WATCHDOG_ADBD_PATH          "/sbin/adbd"
#define WATCHDOG_ADB_PORT           5555

/** Default monitor interval (seconds). */
#define WATCHDOG_DEFAULT_INTERVAL           5

/** Default log rotate threshold (MB). */
#define WATCHDOG_DEFAULT_LOG_ROTATE_MB      10

/** Default seconds with ESTABLISHED=0 before Case C recovery. */
#define WATCHDOG_DEFAULT_NO_CLIENT_TIMEOUT  60

/** Default max recoveries inside the time window. */
#define WATCHDOG_DEFAULT_MAX_RESTART        3

/** Default restart rate-limit window (seconds) = 10 minutes. */
#define WATCHDOG_DEFAULT_RESTART_WINDOW     600

/** Default sleep between stop adbd and start adbd (seconds). */
#define WATCHDOG_DEFAULT_ADBD_SLEEP         3

/**
 * Runtime configuration for Version 2.0.
 */
typedef struct watchdog_config {
    int interval;              /**< Seconds between monitor cycles. */
    int log_rotate_mb;         /**< Rotate log when size exceeds this MB. */
    int status_update;         /**< 1 = write status file each cycle. */
    int log_enable;            /**< 1 = append log each cycle. */

    int recovery_enable;       /**< 1 = allow automatic recovery. */
    int no_client_timeout;     /**< Seconds ESTABLISHED=0 before Case C. */
    int max_restart;           /**< Max recoveries inside restart_window_sec. */
    int restart_window_sec;    /**< Rate-limit window in seconds. */
    int adbd_sleep_sec;        /**< Sleep between stop and start adbd. */
} watchdog_config_t;

int  config_load(const char *path, watchdog_config_t *cfg);
void config_set_defaults(watchdog_config_t *cfg);
int  config_ensure_directory(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_CONFIG_H */
