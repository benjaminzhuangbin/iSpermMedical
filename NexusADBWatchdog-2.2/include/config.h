/**
 * @file config.h
 * @brief Configuration for Nexus ADB Watchdog Version 2.2 (production).
 */

#ifndef NEXUS_WATCHDOG_CONFIG_H
#define NEXUS_WATCHDOG_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define WATCHDOG_VERSION_STR        "Nexus ADB Watchdog 2.2"
#define WATCHDOG_VERSION_NUM        "2.2.0"

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
/** CLOSE_WAIT sockets on :5555 at/above this => TCP_FAULT candidate. */
#define WATCHDOG_DEFAULT_FAULT_CLOSE_WAIT   3
/** Seconds the CLOSE_WAIT fault must persist before Case C recovery. */
#define WATCHDOG_DEFAULT_FAULT_HOLD_SEC     30

typedef struct watchdog_config {
    int interval;
    int log_rotate_mb;
    int status_update;
    int log_enable;

    int recovery_enable;
    int max_restart;
    int restart_window_sec;
    int cooldown_sec;
    int adbd_sleep_sec;
    int log_heartbeat_sec;

    int fault_close_wait;   /**< CLOSE_WAIT threshold for TCP_FAULT. */
    int fault_hold_sec;     /**< Hold time before Case C on socket fault. */
} watchdog_config_t;

int  config_load(const char *path, watchdog_config_t *cfg);
void config_set_defaults(watchdog_config_t *cfg);
int  config_ensure_directory(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_CONFIG_H */
