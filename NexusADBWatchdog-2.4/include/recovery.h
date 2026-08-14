/**
 * @file recovery.h
 * @brief Auto-recovery engine for Version 2.4 (product-stable).
 *
 * CASE A: adbd missing
 * CASE B: 5555 not LISTEN
 * CASE C: wrong prop / sustained CLOSE_WAIT TCP fault
 *
 * ESTABLISHED=0 is NEVER a recovery trigger.
 * Localhost CNXN / PC offline is NEVER a recovery trigger.
 * Rate limit: restart_window_sec + cooldown_sec — NEVER permanent disable.
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
#define RECOVERY_ACTION_RATE_LIMIT        "RATE_LIMIT"
#define RECOVERY_ACTION_SUCCESS           "RECOVERY_SUCCESS"

typedef struct recovery_state {
    int     restart_count;
    time_t  window_start;
    time_t  cooldown_until;   /**< After each recovery action. */
    char    last_action[32];
    int     recoveries_total;
    int     in_cooldown;
    int     pending_verify;   /**< 1 after recovery until health OK. */

    /** Sustained CLOSE_WAIT fault tracking (Case C). */
    time_t  socket_fault_since;
} recovery_state_t;

void recovery_init(recovery_state_t *st, const watchdog_config_t *cfg);

/**
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
 * Update socket-fault timer (CLOSE_WAIT threshold); return 1 when held.
 */
int recovery_update_socket_fault(recovery_state_t *st,
                                 const watchdog_config_t *cfg,
                                 int close_wait);

/**
 * Mark recovery success when service healthy again after an action.
 */
void recovery_note_healthy(recovery_state_t *st);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_RECOVERY_H */
