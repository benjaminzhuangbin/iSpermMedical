/**
 * @file recovery.c
 * @brief Auto-recovery for Nexus ADB Watchdog 2.4.
 *
 * PRODUCT HARD RULE: never restart adbd while TCP ADB service is healthy,
 * and never restart while a live PC/QtScrcpy ESTABLISHED session exists.
 * Cooldown is NOT a periodic restart timer.
 *
 * Never permanently disables recovery. Uses:
 *   - cooldown_sec after each recovery action (only gates retry if STILL fault)
 *   - max_restart within restart_window_sec (count auto-zeros when window ends)
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "recovery.h"
#include "config.h"
#include "logger.h"
#include "property.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void recovery_log_block(const char *line1, const char *line2, const char *line3)
{
    char time_str[32];
    char buf[512];
    time_t now;
    struct tm tm_now;
    int n;

    now = time(NULL);
    if (now != (time_t)-1 && localtime_r(&now, &tm_now) != NULL) {
        (void)strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_now);
    } else {
        util_strlcpy(time_str, "1970-01-01 00:00:00", sizeof(time_str));
    }

    n = snprintf(buf, sizeof(buf), "\n%s\n%s\n%s\n%s\n",
                 time_str,
                 (line1 != NULL) ? line1 : "",
                 (line2 != NULL) ? line2 : "",
                 (line3 != NULL) ? line3 : "");
    if (n > 0) {
        (void)logger_append_event(WATCHDOG_LOG_PATH, buf);
    }
}

void recovery_init(recovery_state_t *st, const watchdog_config_t *cfg)
{
    (void)cfg;
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->window_start = time(NULL);
    if (st->window_start == (time_t)-1) {
        st->window_start = 1;
    }
    util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
}

long recovery_cooldown_remaining(const recovery_state_t *st)
{
    time_t now;

    if (st == NULL || st->cooldown_until == 0) {
        return 0;
    }
    now = time(NULL);
    if (now == (time_t)-1 || now >= st->cooldown_until) {
        return 0;
    }
    return (long)(st->cooldown_until - now);
}

int recovery_update_socket_fault(recovery_state_t *st,
                                 const watchdog_config_t *cfg,
                                 int close_wait)
{
    int thr;
    int hold;
    time_t now;

    if (st == NULL || cfg == NULL) {
        return 0;
    }

    thr = cfg->fault_close_wait;
    if (thr < 1) {
        thr = WATCHDOG_DEFAULT_FAULT_CLOSE_WAIT;
    }
    hold = cfg->fault_hold_sec;
    if (hold < 1) {
        hold = WATCHDOG_DEFAULT_FAULT_HOLD_SEC;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return 0;
    }

    if (close_wait >= thr) {
        if (st->socket_fault_since == 0) {
            st->socket_fault_since = now;
            return 0;
        }
        if ((now - st->socket_fault_since) >= (time_t)hold) {
            return 1;
        }
        return 0;
    }

    st->socket_fault_since = 0;
    return 0;
}

void recovery_note_healthy(recovery_state_t *st)
{
    if (st == NULL) {
        return;
    }
    /* Healthy => end recovery/cooldown immediately. Do NOT wait to re-restart. */
    st->in_cooldown = 0;
    st->cooldown_until = 0;

    if (st->pending_verify) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_SUCCESS, sizeof(st->last_action));
        recovery_log_block("RECOVERY SUCCESS",
                           "ADBD=YES PORT5555=YES Android-side TCP ADB OK",
                           "recovery state cleared; will NOT restart again");
        st->pending_verify = 0;
    } else if (strcmp(st->last_action, RECOVERY_ACTION_RATE_LIMIT) != 0 &&
               strcmp(st->last_action, RECOVERY_ACTION_SUCCESS) != 0) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
    }
}

static void recovery_start_cooldown(recovery_state_t *st, const watchdog_config_t *cfg)
{
    int cd;
    time_t now;

    now = time(NULL);
    if (now == (time_t)-1) {
        now = 1;
    }
    cd = (cfg != NULL) ? cfg->cooldown_sec : WATCHDOG_DEFAULT_COOLDOWN_SEC;
    if (cd < 1) {
        cd = WATCHDOG_DEFAULT_COOLDOWN_SEC;
    }

    st->in_cooldown = 1;
    st->cooldown_until = now + (time_t)cd;
}

static void recovery_tick_window(recovery_state_t *st, const watchdog_config_t *cfg)
{
    time_t now;
    int window;

    if (st == NULL || cfg == NULL) {
        return;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return;
    }
    window = cfg->restart_window_sec > 0 ? cfg->restart_window_sec
                                        : WATCHDOG_DEFAULT_RESTART_WINDOW;
    if ((now - st->window_start) >= (time_t)window) {
        if (st->restart_count > 0) {
            char line[128];
            (void)snprintf(line, sizeof(line),
                           "window=%ds old_count=%d", window, st->restart_count);
            recovery_log_block("RESTART WINDOW RESET",
                               "restart_count cleared; recovery still enabled",
                               line);
        }
        st->window_start = now;
        st->restart_count = 0;
        if (strcmp(st->last_action, RECOVERY_ACTION_RATE_LIMIT) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
            st->cooldown_until = 0;
        }
    }
}

static void recovery_tick_cooldown(recovery_state_t *st)
{
    time_t now;

    if (st == NULL || !st->in_cooldown) {
        return;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return;
    }
    if (now >= st->cooldown_until) {
        st->in_cooldown = 0;
        st->cooldown_until = 0;
        if (strcmp(st->last_action, RECOVERY_ACTION_COOLDOWN) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
        }
    }
}

/**
 * @return 1 allow recovery, 0 deny (cooldown or window rate-limit).
 * Never permanently disables recovery.
 */
static int recovery_rate_allow(recovery_state_t *st, const watchdog_config_t *cfg)
{
    int max_r;
    char line[160];

    if (st == NULL || cfg == NULL) {
        return 0;
    }
    if (!cfg->recovery_enable) {
        return 0;
    }

    recovery_tick_window(st, cfg);
    recovery_tick_cooldown(st);

    if (st->in_cooldown) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN, sizeof(st->last_action));
        return 0;
    }

    max_r = cfg->max_restart > 0 ? cfg->max_restart : WATCHDOG_DEFAULT_MAX_RESTART;
    if (st->restart_count >= max_r) {
        long remain = 0;
        time_t now = time(NULL);
        int window = cfg->restart_window_sec > 0 ? cfg->restart_window_sec
                                                 : WATCHDOG_DEFAULT_RESTART_WINDOW;
        int was_rate_limited =
            (strcmp(st->last_action, RECOVERY_ACTION_RATE_LIMIT) == 0);
        if (now != (time_t)-1 && st->window_start > 0) {
            long elapsed = (long)(now - st->window_start);
            remain = (elapsed < (long)window) ? ((long)window - elapsed) : 0;
        }
        st->cooldown_until = (now != (time_t)-1) ? (now + (time_t)remain) : 0;
        util_strlcpy(st->last_action, RECOVERY_ACTION_RATE_LIMIT, sizeof(st->last_action));
        if (!was_rate_limited) {
            (void)snprintf(line, sizeof(line),
                           "restart_count=%d max=%d window_remain=%lds (no permanent disable)",
                           st->restart_count, max_r, remain);
            recovery_log_block("RATE LIMIT",
                               "waiting for restart_window to expire",
                               line);
        }
        return 0;
    }

    return 1;
}

static void recovery_note_attempt(recovery_state_t *st, const watchdog_config_t *cfg)
{
    if (st == NULL || cfg == NULL) {
        return;
    }
    st->restart_count++;
    st->recoveries_total++;
    st->pending_verify = 1;
    recovery_start_cooldown(st, cfg);
    {
        char line[128];
        (void)snprintf(line, sizeof(line),
                       "restart_count=%d cooldown_sec=%d",
                       st->restart_count, cfg->cooldown_sec);
        recovery_log_block("COOLDOWN ARMED",
                           "post-recovery wait before next attempt",
                           line);
    }
}

static int recovery_set_tcp_port(void)
{
    int rc = property_set_str("persist.adb.tcp.port", WATCHDOG_ADB_PORT_STR);
    (void)property_set_str("service.adb.tcp.port", WATCHDOG_ADB_PORT_STR);
    return rc;
}

static int recovery_start_adbd(void)
{
    return property_set_str("ctl.start", "adbd");
}

static int recovery_restart_adbd(int sleep_sec)
{
    if (sleep_sec < 1) {
        sleep_sec = WATCHDOG_DEFAULT_ADBD_SLEEP;
    }
    (void)recovery_set_tcp_port();
    if (property_set_str("ctl.stop", "adbd") != 0) {
        return -1;
    }
    (void)sleep((unsigned int)sleep_sec);
    if (property_set_str("ctl.start", "adbd") != 0) {
        return -1;
    }
    return 0;
}

int recovery_evaluate(recovery_state_t *st,
                      const watchdog_config_t *cfg,
                      int adbd_ok,
                      int port_ok,
                      int prop_ok,
                      int socket_fault,
                      int established)
{
    int core_up;
    int service_ok;

    if (st == NULL || cfg == NULL) {
        return 0;
    }

    recovery_tick_window(st, cfg);
    recovery_tick_cooldown(st);

    if (!cfg->recovery_enable) {
        if (!st->in_cooldown &&
            strcmp(st->last_action, RECOVERY_ACTION_RATE_LIMIT) != 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
        }
        return 0;
    }

    core_up = (adbd_ok && port_ok) ? 1 : 0;

    /*
     * HARD RULE: live PC/QtScrcpy TCP session => NEVER restart adbd.
     * Protects an active mirror session from Watchdog-induced disconnect.
     */
    if (core_up && established > 0) {
        if (!prop_ok) {
            /* Soft-fix property only; do not stop/start adbd. */
            (void)recovery_set_tcp_port();
        }
        st->socket_fault_since = 0;
        recovery_note_healthy(st);
        return 0;
    }

    /*
     * HARD RULE: adbd + :5555 LISTEN + no clear fault => do nothing.
     * ESTABLISHED=0 is normal idle — not a fault.
     */
    service_ok = (core_up && prop_ok && !socket_fault) ? 1 : 0;
    if (service_ok) {
        recovery_note_healthy(st);
        return 0;
    }

    /*
     * Core service already up: only allow carefully scoped fixes.
     * Never treat cooldown expiry as a reason to restart a healthy listener.
     */
    if (core_up) {
        /* Wrong prop, no live client: setprop + restart once (rate-limited). */
        if (!prop_ok) {
            if (st->in_cooldown) {
                util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN,
                             sizeof(st->last_action));
                return 0;
            }
            if (!recovery_rate_allow(st, cfg)) {
                return 0;
            }
            recovery_log_block("ADB FAULT DETECTED",
                               "FAIL_REASON=TCP_PORT_PROP",
                               "why=persist.adb.tcp.port != 5555; action=FIX_TCP_PORT");
            (void)recovery_set_tcp_port();
            (void)recovery_restart_adbd(cfg->adbd_sleep_sec);
            util_strlcpy(st->last_action, RECOVERY_ACTION_FIX_TCP_PORT,
                         sizeof(st->last_action));
            recovery_log_block("RECOVERY ACTION", "FIX_TCP_PORT / adbd restarted", "");
            recovery_note_attempt(st, cfg);
            return 1;
        }

        /* Sustained CLOSE_WAIT only when no live ESTABLISHED client. */
        if (socket_fault && established == 0) {
            if (st->in_cooldown) {
                util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN,
                             sizeof(st->last_action));
                return 0;
            }
            if (!recovery_rate_allow(st, cfg)) {
                return 0;
            }
            recovery_log_block("ADB FAULT DETECTED",
                               "FAIL_REASON=TCP_FAULT",
                               "why=sustained CLOSE_WAIT; no live client; FIX_TCP_FAULT");
            (void)recovery_restart_adbd(cfg->adbd_sleep_sec);
            util_strlcpy(st->last_action, RECOVERY_ACTION_FIX_TCP_FAULT,
                         sizeof(st->last_action));
            recovery_log_block("RECOVERY ACTION", "FIX_TCP_FAULT / adbd restarted", "");
            st->socket_fault_since = 0;
            recovery_note_attempt(st, cfg);
            return 1;
        }

        /* Core up and remaining conditions are not actionable restarts. */
        recovery_note_healthy(st);
        return 0;
    }

    if (st->in_cooldown) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN, sizeof(st->last_action));
        return 0;
    }

    /* CASE A: adbd missing */
    if (!adbd_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            return 0;
        }
        recovery_log_block("ADB FAULT DETECTED",
                           "FAIL_REASON=ADBD_NOT_RUNNING",
                           "why=adbd process missing; action=START_ADBD");
        (void)recovery_set_tcp_port();
        (void)recovery_start_adbd();
        util_strlcpy(st->last_action, RECOVERY_ACTION_START_ADBD, sizeof(st->last_action));
        recovery_log_block("RECOVERY ACTION", "START_ADBD requested", "");
        recovery_note_attempt(st, cfg);
        return 1;
    }

    /* CASE B: 5555 not LISTEN */
    if (!port_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            return 0;
        }
        recovery_log_block("ADB FAULT DETECTED",
                           "FAIL_REASON=PORT_NOT_LISTENING",
                           "why=TCP 5555 not LISTEN; action=RESTART_ADBD");
        (void)recovery_restart_adbd(cfg->adbd_sleep_sec);
        util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD, sizeof(st->last_action));
        recovery_log_block("RECOVERY ACTION", "RESTART_ADBD requested", "");
        recovery_note_attempt(st, cfg);
        return 1;
    }

    return 0;
}
