/**
 * @file main.c
 * @brief Entry point for Nexus ADB Watchdog Version 2.0 (auto-recovery).
 *
 * Manual start:
 *   nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &
 *
 * Version 2.0 may start/stop adbd and set persist.adb.tcp.port when recovering.
 * Recovery is rate-limited (default MAX_RESTART=3 / 10 minutes).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"
#include "logger.h"
#include "process.h"
#include "watchdog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define WATCHDOG_VERSION "2.0.0"

static void main_on_signal(int signo)
{
    (void)signo;
    watchdog_request_stop();
}

static void main_install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = main_on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGINT, &sa, NULL);
    signal(SIGHUP, SIG_IGN);
}

static void main_print_usage(const char *argv0)
{
    fprintf(stderr,
            "Nexus ADB Watchdog %s (TCP ADB auto-recovery)\n"
            "Usage: %s [-c conf] [-f]\n"
            "  -c conf   Path to watchdog.conf (default: %s)\n"
            "  -f        Foreground\n"
            "  -h        Help\n"
            "\n"
            "Typical start:\n"
            "  nohup /data/local/watchdog/watchdog >/dev/null 2>&1 &\n",
            WATCHDOG_VERSION,
            (argv0 != NULL) ? argv0 : "watchdog",
            WATCHDOG_CONF_PATH);
}

int main(int argc, char **argv)
{
    watchdog_config_t cfg;
    const char *conf_path = WATCHDOG_CONF_PATH;
    int foreground = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            main_print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-f") == 0) {
            foreground = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                main_print_usage(argv[0]);
                return 1;
            }
            conf_path = argv[++i];
        } else {
            main_print_usage(argv[0]);
            return 1;
        }
    }

    (void)foreground;

    if (config_ensure_directory() != 0) {
        fprintf(stderr, "watchdog: cannot create %s\n", WATCHDOG_DIR_PATH);
        return 1;
    }

    if (config_load(conf_path, &cfg) != 0) {
        fprintf(stderr, "watchdog: failed to load config %s\n", conf_path);
        return 1;
    }

    main_install_signals();

    if (process_write_pid_file(WATCHDOG_PID_PATH) != 0) {
        fprintf(stderr, "watchdog: warning: cannot write PID file\n");
    }

    (void)logger_append_line(WATCHDOG_LOG_PATH,
                             "======= Nexus ADB Watchdog 2.0 started =======");

    (void)watchdog_run_loop(&cfg);

    (void)logger_append_line(WATCHDOG_LOG_PATH,
                             "======= Nexus ADB Watchdog 2.0 stopped =======");
    process_remove_pid_file(WATCHDOG_PID_PATH);

    return 0;
}
