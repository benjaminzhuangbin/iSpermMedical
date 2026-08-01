/**
 * @file recovery.c
 * @brief Automatic TCP ADB recovery for Nexus ADB Watchdog 2.0.
 *
 * Uses Android system properties (Bionic) to control adbd:
 *   persist.adb.tcp.port = 5555
 *   ctl.stop  = adbd
 *   ctl.start = adbd
 *
 * On non-Android host builds, property calls are stubbed (no-op) so the
 * project still compiles for syntax checks.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "recovery.h"
#include "config.h"
#include "logger.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __ANDROID__
/* Bionic libc */
int __system_property_set(const char *name, const char *value);
#else
/* Host stub for compile-only checks. */
static int __system_property_set(const char *name, const char *value)
{
    (void)name;
    (void)value;
    return 0;
}
#endif

/**
 * Append a timestamped event block to the log.
 */
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

    n = snprintf(buf, sizeof(buf),
                 "\n%s\n%s\n%s\n%s\n",
                 time_str,
                 (line1 != NULL) ? line1 : "",
                 (line2 != NULL) ? line2 : "",
                 (line3 != NULL) ? line3 : "");
    if (n > 0) {
        (void)logger_append_event(WATCHDOG_LOG_PATH, buf);
    }
}

/**
 * Write watchdog.error when auto-recovery is disabled by rate limit.
 */
int recovery_write_error_file(const char *reason)
{
    FILE *fp;
    time_t now;
    struct tm tm_now;
    char time_str[32];

    now = time(NULL);
    if (now != (time_t)-1 && localtime_r(&now, &tm_now) != NULL) {
        (void)strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_now);
    } else {
        util_strlcpy(time_str, "1970-01-01 00:00:00", sizeof(time_str));
    }

    fp = fopen(WATCHDOG_ERROR_PATH, "a");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp,
            "TIME=%s\n"
            "ERROR=RECOVERY_DISABLED\n"
            "REASON=%s\n"
            "----------------------------------------\n",
            time_str,
            (reason != NULL) ? reason : "unknown");
    (void)fflush(fp);
    (void)fclose(fp);
    return 0;
}

/**
 * Initialize recovery state.
 */
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

/**
 * Refresh rate-limit window; return 1 if a new recovery is still allowed.
 */
static int recovery_rate_allow(recovery_state_t *st, const watchdog_config_t *cfg)
{
    time_t now;
    int window;
    int max_r;

    if (st == NULL || cfg == NULL) {
        return 0;
    }

    if (!st->enabled || !cfg->recovery_enable) {
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

/**
 * Record a recovery attempt; disable further recovery if limit exceeded.
 */
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
        /*
         * MAX_RESTART reached inside the window. Keep LAST_ACTION as the
         * recovery just performed; disable further attempts. Next cycles
         * will report RECOVERY_DISABLED via recovery_evaluate().
         */
        st->enabled = 0;
        (void)recovery_write_error_file(
            "MAX_RESTART exceeded inside restart_window_sec; auto-recovery stopped");
        recovery_log_block("RECOVERY DISABLED",
                           "MAX_RESTART exceeded",
                           "See watchdog.error");
    }
}

/**
 * Ensure persist.adb.tcp.port=5555.
 */
static int recovery_set_tcp_port(void)
{
    char port_str[16];

    (void)snprintf(port_str, sizeof(port_str), "%d", WATCHDOG_ADB_PORT);
    if (__system_property_set("persist.adb.tcp.port", port_str) != 0) {
        return -1;
    }
    /* Also set service.adb.tcp.port for immediate effect on some builds. */
    (void)__system_property_set("service.adb.tcp.port", port_str);
    return 0;
}

/**
 * Start adbd via ctl.start (Case A).
 */
static int recovery_start_adbd(void)
{
    if (__system_property_set("ctl.start", "adbd") != 0) {
        return -1;
    }
    return 0;
}

/**
 * Full restart: setprop + stop + sleep + start (Case B / C).
 */
static int recovery_restart_adbd(int sleep_sec)
{
    if (sleep_sec < 1) {
        sleep_sec = WATCHDOG_DEFAULT_ADBD_SLEEP;
    }

    (void)recovery_set_tcp_port();

    if (__system_property_set("ctl.stop", "adbd") != 0) {
        return -1;
    }

    (void)sleep((unsigned int)sleep_sec);

    if (__system_property_set("ctl.start", "adbd") != 0) {
        return -1;
    }

    return 0;
}

/**
 * Evaluate monitor results and perform recovery if needed.
 */
int recovery_evaluate(recovery_state_t *st,
                      const watchdog_config_t *cfg,
                      int adbd_ok,
                      int port_ok,
                      int established)
{
    time_t now;
    int timeout;
    int did_action = 0;

    if (st == NULL || cfg == NULL) {
        return -1;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        now = 1;
    }

    /* Healthy path: reset no-client timer; keep last_action unless disabled. */
    if (adbd_ok && port_ok && established > 0) {
        st->no_client_since = 0;
        if (st->enabled && cfg->recovery_enable) {
            if (strcmp(st->last_action, RECOVERY_ACTION_RECOVERY_DISABLED) != 0 &&
                strcmp(st->last_action, RECOVERY_ACTION_RATE_LIMITED) != 0) {
                util_strlcpy(st->last_action, RECOVERY_ACTION_NONE, sizeof(st->last_action));
            }
        }
        return 0;
    }

    if (!cfg->recovery_enable || !st->enabled) {
        if (!st->enabled) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RECOVERY_DISABLED,
                         sizeof(st->last_action));
        }
        return 0;
    }

    /* ----- Case A: adbd missing ----- */
    if (!adbd_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            st->enabled = 0;
            util_strlcpy(st->last_action, RECOVERY_ACTION_RECOVERY_DISABLED,
                         sizeof(st->last_action));
            (void)recovery_write_error_file(
                "MAX_RESTART exceeded; refused START_ADBD");
            recovery_log_block("RECOVERY DISABLED",
                               "rate limit",
                               "See watchdog.error");
            return 0;
        }

        (void)recovery_set_tcp_port();
        recovery_log_block("ADBD LOST", "start adbd", "");
        if (recovery_start_adbd() == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_START_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("start adbd", "ADB TCP recovery requested", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_START_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("start adbd FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        st->no_client_since = 0;
        did_action = 1;
        (void)did_action;
        return 0;
    }

    /* ----- Case B: adbd up but port 5555 not listening ----- */
    if (!port_ok) {
        if (!recovery_rate_allow(st, cfg)) {
            st->enabled = 0;
            util_strlcpy(st->last_action, RECOVERY_ACTION_RECOVERY_DISABLED,
                         sizeof(st->last_action));
            (void)recovery_write_error_file(
                "MAX_RESTART exceeded; refused RESTART_ADBD (port)");
            recovery_log_block("RECOVERY DISABLED",
                               "rate limit",
                               "See watchdog.error");
            return 0;
        }

        recovery_log_block("PORT 5555 NOT LISTEN",
                           "setprop persist.adb.tcp.port 5555",
                           "restart adbd");
        if (recovery_restart_adbd(cfg->adbd_sleep_sec) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("restart adbd", "ADB TCP recovered (requested)", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("restart adbd FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        st->no_client_since = 0;
        did_action = 1;
        return 0;
    }

    /* ----- Case C: adbd + port OK, but no ESTABLISHED client ----- */
    if (established <= 0) {
        timeout = cfg->no_client_timeout;
        if (timeout < 1) {
            timeout = WATCHDOG_DEFAULT_NO_CLIENT_TIMEOUT;
        }

        if (st->no_client_since == 0) {
            st->no_client_since = now;
            /* Do not restart immediately. */
            return 0;
        }

        if ((now - st->no_client_since) < (time_t)timeout) {
            return 0;
        }

        /* Timeout elapsed with ESTABLISHED still 0. */
        if (!recovery_rate_allow(st, cfg)) {
            st->enabled = 0;
            util_strlcpy(st->last_action, RECOVERY_ACTION_RECOVERY_DISABLED,
                         sizeof(st->last_action));
            (void)recovery_write_error_file(
                "MAX_RESTART exceeded; refused RESTART_ADBD (no client)");
            recovery_log_block("RECOVERY DISABLED",
                               "rate limit",
                               "See watchdog.error");
            return 0;
        }

        recovery_log_block("TCP CLIENT NONE",
                           "TCP timeout",
                           "restart adbd");
        if (recovery_restart_adbd(cfg->adbd_sleep_sec) == 0) {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("restart adbd", "ADB TCP recovered (requested)", "");
        } else {
            util_strlcpy(st->last_action, RECOVERY_ACTION_RESTART_ADBD,
                         sizeof(st->last_action));
            recovery_log_block("restart adbd FAILED", "check root / init", "");
        }
        recovery_note_attempt(st, cfg);
        /* Reset timer so we wait another full timeout before next Case C. */
        st->no_client_since = 0;
        did_action = 1;
        (void)did_action;
        return 0;
    }

    return 0;
}
