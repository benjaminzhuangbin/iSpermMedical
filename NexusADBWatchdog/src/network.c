/**
 * @file network.c
 * @brief Parse /proc/net/tcp for LISTEN and ESTABLISHED on a given port.
 *
 * Android 5.1 / Linux /proc/net/tcp line format (fields):
 *   sl  local_address rem_address   st ...
 *   0:  00000000:15B3 00000000:0000 0A ...
 *
 * local_address = IPv4 (little-endian hex) : port (hex)
 * st            = TCP state (hex)
 *   01 = ESTABLISHED
 *   0A = LISTEN
 */

#include "network.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Linux TCP state: ESTABLISHED */
#define TCP_STATE_ESTABLISHED  0x01

/** Linux TCP state: LISTEN */
#define TCP_STATE_LISTEN       0x0A

/**
 * Parse one /proc/net/tcp data line.
 *
 * @param line         Input line.
 * @param local_ip     Output local IPv4 (host order uint32).
 * @param local_port   Output local port (host order).
 * @param state        Output TCP state.
 * @return 0 on success, -1 if line is not a data row.
 */
static int network_parse_tcp_line(const char *line,
                                  unsigned int *local_ip,
                                  unsigned int *local_port,
                                  unsigned int *state)
{
    const char *p;
    unsigned int lip = 0U;
    unsigned int lport = 0U;
    unsigned int st = 0U;
    int n;

    if (line == NULL || local_ip == NULL || local_port == NULL || state == NULL) {
        return -1;
    }

    /* Skip leading spaces. */
    p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    /* Header line starts with "sl". */
    if (p[0] == 's' && p[1] == 'l') {
        return -1;
    }

    /*
     * Typical:
     *   0: 0100007F:0035 00000000:0000 0A ...
     * Use sscanf with hex conversions.
     */
    n = sscanf(p, "%*d: %x:%x %*x:%*x %x", &lip, &lport, &st);
    if (n != 3) {
        return -1;
    }

    *local_ip = lip;
    *local_port = lport;
    *state = st;
    return 0;
}

/**
 * Scan /proc/net/tcp (and optionally /proc/net/tcp6 for IPv4-mapped).
 * For Android 5.1 ADB TCP we primarily need IPv4 /proc/net/tcp.
 *
 * @param port              Host-order port of interest.
 * @param out_listening     Set to 1 if any LISTEN on port (prefer 0.0.0.0).
 * @param out_established   Count of ESTABLISHED with local port == port.
 * @return 0 on success, -1 if /proc/net/tcp cannot be opened.
 */
static int network_scan_proc_tcp(unsigned int port,
                                 int *out_listening,
                                 int *out_established)
{
    FILE *fp;
    char  line[512];
    int   listening = 0;
    int   wildcard_listen = 0;
    int   established = 0;

    fp = fopen("/proc/net/tcp", "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        unsigned int lip = 0U;
        unsigned int lport = 0U;
        unsigned int st = 0U;

        if (network_parse_tcp_line(line, &lip, &lport, &st) != 0) {
            continue;
        }

        if (lport != port) {
            continue;
        }

        if (st == TCP_STATE_LISTEN) {
            listening = 1;
            if (lip == 0U) {
                /* 0.0.0.0:port — matches project requirement. */
                wildcard_listen = 1;
            }
        } else if (st == TCP_STATE_ESTABLISHED) {
            established++;
        }
    }

    fclose(fp);

    if (out_listening != NULL) {
        /*
         * Prefer reporting LISTEN when wildcard bind is present.
         * Also accept any LISTEN on the port (device may bind a specific IP).
         */
        *out_listening = (wildcard_listen || listening) ? 1 : 0;
    }
    if (out_established != NULL) {
        *out_established = established;
    }

    return 0;
}

/**
 * Check whether the port is listening.
 */
int network_is_port_listening(unsigned int port)
{
    int listening = 0;
    int established = 0;

    if (network_scan_proc_tcp(port, &listening, &established) != 0) {
        return -1;
    }
    return listening;
}

/**
 * Count ESTABLISHED connections on the local port.
 */
int network_count_established(unsigned int port)
{
    int listening = 0;
    int established = 0;

    if (network_scan_proc_tcp(port, &listening, &established) != 0) {
        return -1;
    }
    return established;
}
