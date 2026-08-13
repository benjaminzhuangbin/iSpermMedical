/**
 * @file recovery.h
 * @brief Auto-recovery engine for Version 2.2 (cooldown rate-limit).
 *
 * CASE A: adbd missing
 * CASE B: 5555 not LISTEN
 * CASE C: clear Android-side TCP fault (wrong prop / sustained CLOSE_WAIT)
 *
 * ESTABLISHED=0 is NEVER a recovery trigger.
 * max_restart => cooldown => auto resume (never permanent disable).
 */

#ifndef NEXUS_WATCHDOG_RECOVERY_H
#define NEXUS_WATCHDOG_RECOVERY_H

#include "config.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECOVERY_ACTION_NONE              "NONE"
#define RECOVERY_ACTION_START_ADBD        "START_ADBD"
#define RECOVERY_ACTION_RESTART_ADBD      "RESTART_ADBD"
#define RECOVERY_ACTION_FIX_TCP_PORT      "FIX_TCP_PORT"
#define RECOVERY_ACTION_FIX_TCP_FAULT     "FIX_TCP_FAULT"
#define RECOVERY_ACTION_COOLDOWN          "RECOVERY_COOLDOWN"
#define RECOVERY_ACTION_RESUMED           "RECOVERY_RESUMED"

typedef struct recovery_state {
    int     enabled_runtime;     /**< 0 only while cooling down (internal). */
    int     restart_count;
    time_t  window_start;
    time_t  cooldown_until;
    char    last_action[32];
    int     recoveries_total;
    int     in_cooldown;

    /** Sustained CLOSE_WAIT fault tracking (Case C). */
    time_t  socket_fault_since;
} recovery_state_t;

void recovery_init(recovery_state_t *st, const watchdog_config_t *cfg);

/**
 * @param socket_fault 1 if CLOSE_WAIT fault held long enough
 * @return 1 if recovery action taken
 */
int recovery_evaluate(recovery_state_t *st,
                      const watchdog_config_t *cfg,
                      int adbd_ok,
                      int port_ok,
                      int prop_ok,
                      int socket_fault);

long recovery_cooldown_remaining(const recovery_state_t *st);

/**
 * Update socket-fault timer; return 1 when hold time reached.
 */
int recovery_update_socket_fault(recovery_state_t *st,
                                 const watchdog_config_t *cfg,
                                 int close_wait);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_RECOVERY_H */
