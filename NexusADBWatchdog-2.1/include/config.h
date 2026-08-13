/**
 * @file config.h
 * @brief Configuration for Nexus ADB Watchdog Version 2.1.
 *
 * V2.1: ESTABLISHED=0 is NOT a fault. Recovery uses cooldown, never permanent latch.
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
#define WATCHDOG_INJECT_PATH        "/data/local/watchdog/watchdog.inject"
#define WATCHDOG_ADBD_PATH          "/sbin/adbd"
#define WATCHDOG_ADB_PORT           5555
#define WATCHDOG_ADB_PORT_STR       "5555"

#define WATCHDOG_DEFAULT_INTERVAL           5
#define WATCHDOG_DEFAULT_LOG_ROTATE_MB      10
#define WATCHDOG_DEFAULT_MAX_RESTART        3
#define WATCHDOG_DEFAULT_RESTART_WINDOW     600
#define WATCHDOG_DEFAULT_COOLDOWN_SEC       60
#define WATCHDOG_DEFAULT_ADBD_SLEEP         3
#define WATCHDOG_DEFAULT_LOG_HEARTBEAT_SEC  300

typedef struct watchdog_config {
    int interval;
    int log_rotate_mb;
    int status_update;
    int log_enable;

    int recovery_enable;
    int max_restart;
    int restart_window_sec;
    int cooldown_sec;          /**< After max_restart, pause then auto-resume. */
    int adbd_sleep_sec;
    int log_heartbeat_sec;     /**< Periodic healthy log; 0 = only on change/recovery. */
} watchdog_config_t;

int  config_load(const char *path, watchdog_config_t *cfg);
void config_set_defaults(watchdog_config_t *cfg);
int  config_ensure_directory(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_CONFIG_H */
