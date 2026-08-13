/**
 * @file recovery.c
 * @brief Auto-recovery for Nexus ADB Watchdog 2.1.
 *
 * Key V2.1 changes vs 2.0:
 * - ESTABLISHED=0 is NEVER a recovery trigger.
 * - max_restart trips COOLDOWN then auto-resumes (no permanent latch).
 * - CASE C = wrong persist.adb.tcp.port (not "no client").
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
    if (st == NULL) {
        return;
    }

    memset(st, 0, sizeof(*st));
    st->enabled = (cfg != NULL && cfg->recovery_enable) ? 1 : 0;
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
    if (now == (time_t)-1) {
        return 0;
    }
    if (now >= st->cooldown_until) {
        return 0;
    }
    return (long)(st->cooldown_until - now);
}

/**
 * Enter cooldown instead of permanent disable.
 */
static void recovery_enter_cooldown(recovery_state_t *st, const watchdog_config_t *cfg)
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

    st->enabled = 0;
    st->in_cooldown = 1;
    st->cooldown_until = now + (time_t)cd;
    util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN, sizeof(st->last_action));

    recovery_log_block("RECOVERY COOLDOWN",
                       "max_restart reached; will auto-resume",
                       "");
}

/**
 * If cooldown expired, resume recovery and reset window.
 * @return 1 if just resumed this call.
 */
static int recovery_tick_cooldown(recovery_state_t *st, const watchdog_config_t *cfg)
{
    time_t now;

    if (st == NULL || !st->in_cooldown) {
        return 0;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return 0;
    }

    if (now < st->cooldown_until) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN, sizeof(st->last_action));
        st->enabled = 0;
        return 0;
    }

    /* Auto resume */
    st->in_cooldown = 0;
    st->cooldown_until = 0;
    st->restart_count = 0;
    st->window_start = now;
    st->enabled = (cfg != NULL && cfg->recovery_enable) ? 1 : 0;
    util_strlcpy(st->last_action, RECOVERY_ACTION_RESUMED, sizeof(st->last_action));
    recovery_log_block("RECOVERY RESUMED",
                       "cooldown finished; new recovery window",
                       "");
    return 1;
}

/**
 * @return 1 if another recovery attempt is allowed now.
 */
static int recovery_rate_allow(recovery_state_t *st, const watchdog_config_t *cfg)
{
    time_t now;
    int window;
    int max_r;

    if (st == NULL || cfg == NULL) {
        return 0;
    }
    if (!cfg->recovery_enable || !st->enabled || st->in_cooldown) {
        return 0;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return 0;
    }

    window = cfg->restart_window_sec;
    if (window < 1) {
        window = WATCHDOG_DEFAULT_RESTART_WINDOW;
    }
    max_r = cfg->max_restart;
    if (max_r < 1) {
        max_r = WATCHDOG_DEFAULT_MAX_RESTART;
    }

    if ((now - st->window_start) >= (time_t)window) {
        st->window_start = now;
        st->restart_count = 0;
    }

    if (st->restart_count >= max_r) {
        return 0;
    }
    return 1;
}

static void recovery_note_attempt(recovery_state_t *st, const watchdog_config_t *cfg)
{
    int max_r;

    if (st == NULL || cfg == NULL) {
        return;
    }

    st->restart_count++;
    st->recoveries_total++;

    max_r = cfg->max_restart;
    if (max_r < 1) {
        max_r = WATCHDOG_DEFAULT_MAX_RESTART;
    }

    if (st->restart_count >= max_r) {
        recovery_enter_cooldown(st, cfg);
    }
}

static int recovery_set_tcp_port(void)
{
    int rc;

    rc = property_set_str("persist.adb.tcp.port", WATCHDOG_ADB_PORT_STR);
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
                      int prop_ok)
{
    int acted = 0;

    if (st == NULL || cfg == NULL) {
        return 0;
    }

    /* Cooldown auto-resume first. */
    (void)recovery_tick_cooldown(st, cfg);

    if (!cfg->recovery_enable) {
        st->enabled = 0;
        /* Keep last_action unless in cooldown messaging. */
        if (!st->in_cooldown) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
        }
        return 0;
    }

    if (st->in_cooldown) {
        util_strlcpy(st->last_action, RECOVERY_ACTION_COOLDOWN, sizeof(st->last_action));
        st->enabled = 0;
        return 0;
    }

    st->enabled = 1;

    /*
     * Healthy service path (client optional):
     *   adbd OK + port LISTEN + prop OK
     * ESTABLISHED is intentionally ignored here.
     */
    if (adbd_ok && port_ok && prop_ok) {
        if (strcmp(st->last_action, RECOVERY_ACTION_COOLDOWN) != 0 &&
            strcmp(st->last_action, RECOVERY_ACTION_RESUMED) != 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
        } else if (strcmp(st->last_action, RECOVERY_ACTION_RESUMED) == 0) {
            /* Keep RESUMED for one status cycle then clear next healthy cycle. */
            util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
        }
        return 0;
    }

    /* ----- CASE A: adbd missing ----- */
    if (!adbd_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            recovery_enter_cooldown(st, cfg);
            return 0;
        }

        recovery_log_block("ADBD DOWN", "START_ADBD", "");
        (void)recovery_set_tcp_port();
        if (recovery_start_adbd() == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_START_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("START_ADBD", "requested", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_START_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("START_ADBD FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        return 1;
    }

    /* ----- CASE B: adbd up, 5555 not LISTEN ----- */
    if (!port_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            recovery_enter_cooldown(st, cfg);
            return 0;
        }

        recovery_log_block("PORT 5555 DOWN", "RESTART_ADBD", "");
        if (recovery_restart_adbd(cfg->adbd_sleep_sec) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("RESTART_ADBD", "requested", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("RESTART_ADBD FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        return 1;
    }

    /* ----- CASE C: persist.adb.tcp.port != 5555 ----- */
    if (!prop_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            recovery_enter_cooldown(st, cfg);
            return 0;
        }

        recovery_log_block("TCP PORT PROP WRONG", "FIX_TCP_PORT", "RESTART_ADBD");
        (void)recovery_set_tcp_port();
        if (recovery_restart_adbd(cfg->adbd_sleep_sec) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_FIX_TCP_PORT,
                         sizeof(st->last_action));
            recovery_log_block("FIX_TCP_PORT", "adbd restarted", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_FIX_TCP_PORT,
                         sizeof(st->last_action));
            recovery_log_block("FIX_TCP_PORT FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        return 1;
    }

    (void)acted;
    return 0;
}
