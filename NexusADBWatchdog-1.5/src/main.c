/**
 * @file main.c
 * @brief Entry point for Nexus ADB Watchdog Version 1.5 (diagnostics).
 *
 * Manual start on device:
 *   nohup /data/local/watchdog/watchdog &
 *
 * Version 1.5 is monitoring / data collection only:
 *   - does NOT restart / stop / start adbd
 *   - does NOT kill processes
 *   - does NOT reboot or change system properties
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

/** Program version string. */
#define WATCHDOG_VERSION "1.5.0"

/**
 * Signal handler: request graceful stop.
 */
static void main_on_signal(int signo)
{
    (void)signo;
    watchdog_request_stop();
}

/**
 * Install SIGTERM / SIGINT handlers.
 */
static void main_install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = main_on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGINT, &sa, NULL);

    /* Ignore SIGHUP so nohup / detach is robust. */
    signal(SIGHUP, SIG_IGN);
}

/**
 * Print brief usage to stderr.
 */
static void main_print_usage(const char *argv0)
{
    fprintf(stderr,
            "Nexus ADB Watchdog %s (diagnostic / data collection)\n"
            "Usage: %s [-c conf] [-f]\n"
            "  -c conf   Path to watchdog.conf (default: %s)\n"
            "  -f        Foreground (keep attached to terminal)\n"
            "  -h        Show this help\n"
            "\n"
            "Typical start:\n"
            "  nohup /data/local/watchdog/watchdog &\n"
            "\n"
            "Version 1.5 does NOT recover adbd. Monitor only.\n",
            WATCHDOG_VERSION,
            (argv0 != NULL) ? argv0 : "watchdog",
            WATCHDOG_CONF_PATH);
}

/**
 * Application entry.
 */
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

    /*
     * Stay in the launching shell job; operators use:
     *   nohup /data/local/watchdog/watchdog &
     */
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
                             "======= Nexus ADB Watchdog 1.5 started =======");

    (void)watchdog_run_loop(&cfg);

    (void)logger_append_line(WATCHDOG_LOG_PATH,
                             "======= Nexus ADB Watchdog 1.5 stopped =======");
    process_remove_pid_file(WATCHDOG_PID_PATH);

    return 0;
}
