/**
 * @file recovery.h
 * @brief Auto-recovery engine for Version 2.1 (cooldown, no permanent latch).
 *
 * CASE A: adbd missing              -> setprop + start adbd
 * CASE B: port 5555 not LISTEN      -> setprop + stop + sleep + start
 * CASE C: persist.adb.tcp.port wrong-> setprop + restart adbd
 *
 * ESTABLISHED=0 is NEVER a recovery trigger.
 *
 * Rate limit: max_restart inside restart_window_sec -> COOLDOWN -> auto resume.
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
#define RECOVERY_ACTION_COOLDOWN          "RECOVERY_COOLDOWN"
#define RECOVERY_ACTION_RESUMED           "RECOVERY_RESUMED"

typedef struct recovery_state {
    int     enabled;            /**< 0 only while in cooldown. */
    int     restart_count;      /**< Attempts inside current window. */
    time_t  window_start;
    time_t  cooldown_until;     /**< 0 = not in cooldown. */
    char    last_action[32];
    int     recoveries_total;
    int     in_cooldown;        /**< 1 while cooling down. */
} recovery_state_t;

void recovery_init(recovery_state_t *st, const watchdog_config_t *cfg);

/**
 * Evaluate health and recover if needed.
 *
 * @param prop_ok  1 if persist.adb.tcp.port == 5555.
 * @return 1 if a recovery action was taken this cycle, 0 otherwise.
 */
int recovery_evaluate(recovery_state_t *st,
                      const watchdog_config_t *cfg,
                      int adbd_ok,
                      int port_ok,
                      int prop_ok);

/** Remaining cooldown seconds (0 if not cooling down). */
long recovery_cooldown_remaining(const recovery_state_t *st);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_RECOVERY_H */
