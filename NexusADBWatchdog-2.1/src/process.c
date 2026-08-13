/**
 * @file process.c
 * @brief Process scanning via /proc and PID file helpers (Version 1.5).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "process.h"
#include "config.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Return 1 if name is all digits (a PID directory name).
 */
static int process_is_pid_dir(const char *name)
{
    size_t i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    for (i = 0U; name[i] != '\0'; i++) {
        if (name[i] < '0' || name[i] > '9') {
            return 0;
        }
    }
    return 1;
}

/**
 * Read /proc/<pid>/exe symlink target into buf.
 */
static int process_read_exe(const char *pid, char *buf, size_t buflen)
{
    char link_path[96];
    ssize_t n;
    int sn;

    if (pid == NULL || buf == NULL || buflen == 0U) {
        return -1;
    }

    sn = snprintf(link_path, sizeof(link_path), "/proc/%s/exe", pid);
    if (sn <= 0 || (size_t)sn >= sizeof(link_path)) {
        return -1;
    }
    n = readlink(link_path, buf, buflen - 1U);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return 0;
}

/**
 * Read /proc/<pid>/cmdline into buf (NUL bytes replaced with spaces).
 */
static int process_read_cmdline(const char *pid, char *buf, size_t buflen)
{
    char path[96];
    FILE *fp;
    size_t n;
    size_t i;
    int sn;

    if (pid == NULL || buf == NULL || buflen == 0U) {
        return -1;
    }

    sn = snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);
    if (sn <= 0 || (size_t)sn >= sizeof(path)) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    n = fread(buf, 1U, buflen - 1U, fp);
    fclose(fp);

    if (n == 0U) {
        buf[0] = '\0';
        return -1;
    }

    buf[n] = '\0';
    for (i = 0U; i < n; i++) {
        if (buf[i] == '\0') {
            buf[i] = ' ';
        }
    }
    return 0;
}

/**
 * Check whether cmdline or exe matches the expected path / basename.
 */
static int process_matches(const char *exe_path, const char *exe, const char *cmdline)
{
    const char *base;

    if (exe_path == NULL) {
        return 0;
    }

    base = strrchr(exe_path, '/');
    base = (base != NULL) ? (base + 1) : exe_path;

    if (exe != NULL && exe[0] != '\0') {
        if (strcmp(exe, exe_path) == 0) {
            return 1;
        }
        if (strncmp(exe, exe_path, strlen(exe_path)) == 0) {
            return 1;
        }
        if (strstr(exe, base) != NULL) {
            const char *p = strrchr(exe, '/');
            p = (p != NULL) ? (p + 1) : exe;
            if (strncmp(p, base, strlen(base)) == 0) {
                return 1;
            }
        }
    }

    if (cmdline != NULL && cmdline[0] != '\0') {
        if (strstr(cmdline, exe_path) != NULL) {
            return 1;
        }
        if (strncmp(cmdline, base, strlen(base)) == 0) {
            char next = cmdline[strlen(base)];
            if (next == '\0' || next == ' ' || next == '\t') {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Find PID of a process matching exe_path. Returns -1 if not found.
 */
int process_find_pid(const char *exe_path)
{
    DIR *dir;
    struct dirent *ent;
    int found_pid = -1;

    if (exe_path == NULL || exe_path[0] == '\0') {
        return -1;
    }

    dir = opendir("/proc");
    if (dir == NULL) {
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        char exe[256];
        char cmdline[256];
        long pid_val;

        if (!process_is_pid_dir(ent->d_name)) {
            continue;
        }

        exe[0] = '\0';
        cmdline[0] = '\0';

        (void)process_read_exe(ent->d_name, exe, sizeof(exe));
        (void)process_read_cmdline(ent->d_name, cmdline, sizeof(cmdline));

        if (process_matches(exe_path, exe, cmdline)) {
            pid_val = strtol(ent->d_name, NULL, 10);
            if (pid_val > 0L) {
                found_pid = (int)pid_val;
                break;
            }
        }
    }

    closedir(dir);
    return found_pid;
}

/**
 * Check whether a process whose executable path matches exe_path exists.
 */
int process_is_running(const char *exe_path)
{
    int pid = process_find_pid(exe_path);
    return (pid > 0) ? 1 : 0;
}

/**
 * Write current process PID to path.
 */
int process_write_pid_file(const char *path)
{
    FILE *fp;
    const char *use_path;
    int written;

    use_path = (path != NULL) ? path : WATCHDOG_PID_PATH;
    fp = fopen(use_path, "w");
    if (fp == NULL) {
        return -1;
    }

    written = fprintf(fp, "%d\n", (int)getpid());
    (void)fflush(fp);
    (void)fclose(fp);

    return (written > 0) ? 0 : -1;
}

/**
 * Remove PID file if it contains our PID (best effort).
 */
void process_remove_pid_file(const char *path)
{
    FILE *fp;
    const char *use_path;
    int pid_in_file = -1;
    int our_pid;

    use_path = (path != NULL) ? path : WATCHDOG_PID_PATH;
    our_pid = (int)getpid();

    fp = fopen(use_path, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%d", &pid_in_file) != 1) {
            pid_in_file = -1;
        }
        fclose(fp);
    }

    if (pid_in_file == our_pid || pid_in_file < 0) {
        (void)unlink(use_path);
    }
}
