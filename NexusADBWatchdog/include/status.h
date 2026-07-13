/**
 * @file status.h
 * @brief Monitor status snapshot and status-file writer.
 *
 * Extensible for Version 2+ (recovery actions, medical app health, heartbeat).
 */

#ifndef NEXUS_WATCHDOG_STATUS_H
#define NEXUS_WATCHDOG_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/** adbd process check result strings. */
#define STATUS_ADBD_OK      "OK"
#define STATUS_ADBD_LOST    "LOST"

/** Port listen check result strings. */
#define STATUS_PORT_LISTEN      "LISTEN"
#define STATUS_PORT_NOT_LISTEN  "NOT_LISTEN"

/**
 * One monitor cycle snapshot.
 * Future versions may append fields; keep binary layout additive.
 */
typedef struct watchdog_status {
    char time_str[32];        /**< "YYYY-MM-DD HH:MM:SS" */
    char adbd[16];            /**< "OK" or "LOST" */
    char port5555[16];        /**< "LISTEN" or "NOT_LISTEN" */
    int  established;         /**< ESTABLISHED count on port 5555 (>=0) */
} watchdog_status_t;

/**
 * Fill time_str with local wall-clock time.
 *
 * @param status  Status object to update (must not be NULL).
 */
void status_set_time_now(watchdog_status_t *status);

/**
 * Overwrite the status file with the current snapshot.
 *
 * @param path    Status file path (may be NULL for default).
 * @param status  Snapshot to write.
 * @return 0 on success, -1 on failure.
 */
int status_write_file(const char *path, const watchdog_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_STATUS_H */
