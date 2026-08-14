/**
 * @file network.c
 * @brief Parse /proc/net/tcp for full TCP state diagnostics on a local port.
 *
 * Android 5.1 / Linux /proc/net/tcp line format:
 *   sl  local_address rem_address   st ...
 *   0:  00000000:15B3 6400A8C0:D1A2 01 ...
 *
 * Does NOT invoke netstat or any shell utility.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "network.h"

#include <stdio.h>
#include <string.h>

/** Linux TCP states (hex in /proc/net/tcp). */
#define TCP_ESTABLISHED  0x01
#define TCP_SYN_SENT     0x02
#define TCP_SYN_RECV     0x03
#define TCP_FIN_WAIT1    0x04
#define TCP_FIN_WAIT2    0x05
#define TCP_TIME_WAIT    0x06
#define TCP_CLOSE        0x07
#define TCP_CLOSE_WAIT   0x08
#define TCP_LAST_ACK     0x09
#define TCP_LISTEN       0x0A
#define TCP_CLOSING      0x0B

/**
 * Format a /proc/net/tcp IPv4 value as dotted decimal.
 */
void network_format_ipv4(unsigned int ip_le, char *buf, unsigned int buflen)
{
    unsigned int b0;
    unsigned int b1;
    unsigned int b2;
    unsigned int b3;

    if (buf == NULL || buflen == 0U) {
        return;
    }

    b0 = ip_le & 0xFFU;
    b1 = (ip_le >> 8) & 0xFFU;
    b2 = (ip_le >> 16) & 0xFFU;
    b3 = (ip_le >> 24) & 0xFFU;

    (void)snprintf(buf, buflen, "%u.%u.%u.%u", b0, b1, b2, b3);
}

/**
 * Parse one /proc/net/tcp data line.
 *
 * @return 0 on success, -1 if not a data row.
 */
static int network_parse_tcp_line(const char *line,
                                  unsigned int *local_ip,
                                  unsigned int *local_port,
                                  unsigned int *remote_ip,
                                  unsigned int *remote_port,
                                  unsigned int *state)
{
    const char *p;
    unsigned int lip = 0U;
    unsigned int lport = 0U;
    unsigned int rip = 0U;
    unsigned int rport = 0U;
    unsigned int st = 0U;
    int n;

    if (line == NULL || local_ip == NULL || local_port == NULL ||
        remote_ip == NULL || remote_port == NULL || state == NULL) {
        return -1;
    }

    p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (p[0] == 's' && p[1] == 'l') {
        return -1;
    }

    n = sscanf(p, "%*d: %x:%x %x:%x %x",
               &lip, &lport, &rip, &rport, &st);
    if (n != 5) {
        return -1;
    }

    *local_ip = lip;
    *local_port = lport;
    *remote_ip = rip;
    *remote_port = rport;
    *state = st;
    return 0;
}

/**
 * Append unique client IP to comma-separated clients buffer.
 */
static void network_add_client_ip(network_tcp_stats_t *stats, unsigned int rip)
{
    char ipstr[NETWORK_IP_STR_LEN];
    char *p;
    size_t cur_len;
    size_t ip_len;

    if (stats == NULL) {
        return;
    }

    network_format_ipv4(rip, ipstr, (unsigned int)sizeof(ipstr));

    /* Skip 0.0.0.0 */
    if (strcmp(ipstr, "0.0.0.0") == 0) {
        return;
    }

    /* Already present? */
    p = stats->clients;
    while (*p != '\0') {
        char token[NETWORK_IP_STR_LEN];
        size_t i = 0U;

        while (*p != '\0' && *p != ',' && i + 1U < sizeof(token)) {
            token[i++] = *p++;
        }
        token[i] = '\0';
        if (*p == ',') {
            p++;
        }
        if (strcmp(token, ipstr) == 0) {
            return;
        }
    }

    ip_len = strlen(ipstr);
    cur_len = strlen(stats->clients);

    if (cur_len == 0U) {
        if (ip_len + 1U > sizeof(stats->clients)) {
            return;
        }
        memcpy(stats->clients, ipstr, ip_len + 1U);
    } else {
        if (cur_len + 1U + ip_len + 1U > sizeof(stats->clients)) {
            return;
        }
        stats->clients[cur_len] = ',';
        memcpy(stats->clients + cur_len + 1U, ipstr, ip_len + 1U);
    }
}

/**
 * Record an ESTABLISHED endpoint for cumulative tracking.
 */
static void network_add_endpoint(network_tcp_stats_t *stats,
                                 unsigned int rip,
                                 unsigned int rport)
{
    int i;

    if (stats == NULL) {
        return;
    }

    if (stats->endpoint_count >= NETWORK_MAX_ENDPOINTS) {
        return;
    }

    for (i = 0; i < stats->endpoint_count; i++) {
        if (stats->endpoints[i].ip == rip &&
            stats->endpoints[i].port == rport) {
            return;
        }
    }

    stats->endpoints[stats->endpoint_count].ip = rip;
    stats->endpoints[stats->endpoint_count].port = rport;
    stats->endpoint_count++;
}

/**
 * Parse /proc/net/tcp and fill stats for the given local port.
 */
int network_collect_tcp_stats(unsigned int port, network_tcp_stats_t *stats)
{
    FILE *fp;
    char  line[512];

    if (stats == NULL) {
        return -1;
    }

    memset(stats, 0, sizeof(*stats));

    fp = fopen("/proc/net/tcp", "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        unsigned int lip = 0U;
        unsigned int lport = 0U;
        unsigned int rip = 0U;
        unsigned int rport = 0U;
        unsigned int st = 0U;

        if (network_parse_tcp_line(line, &lip, &lport, &rip, &rport, &st) != 0) {
            continue;
        }

        if (lport != port) {
            continue;
        }

        switch (st) {
        case TCP_LISTEN:
            stats->listening = 1;
            break;
        case TCP_ESTABLISHED:
            stats->established++;
            network_add_client_ip(stats, rip);
            network_add_endpoint(stats, rip, rport);
            break;
        case TCP_TIME_WAIT:
            stats->time_wait++;
            break;
        case TCP_CLOSE_WAIT:
            stats->close_wait++;
            break;
        case TCP_SYN_RECV:
            stats->syn_recv++;
            break;
        case TCP_FIN_WAIT1:
            stats->fin_wait1++;
            break;
        case TCP_FIN_WAIT2:
            stats->fin_wait2++;
            break;
        case TCP_LAST_ACK:
            stats->last_ack++;
            break;
        case TCP_CLOSING:
            stats->closing++;
            break;
        case TCP_CLOSE:
            stats->close_state++;
            break;
        default:
            break;
        }
    }

    fclose(fp);
    return 0;
}

/**
 * Count endpoints in current that were absent from previous.
 */
int network_count_new_endpoints(const network_endpoint_t *previous,
                                int previous_count,
                                const network_endpoint_t *current,
                                int current_count)
{
    int i;
    int j;
    int neu = 0;

    if (current == NULL || current_count <= 0) {
        return 0;
    }

    if (previous == NULL || previous_count <= 0) {
        return current_count;
    }

    for (i = 0; i < current_count; i++) {
        int found = 0;
        for (j = 0; j < previous_count; j++) {
            if (previous[j].ip == current[i].ip &&
                previous[j].port == current[i].port) {
                found = 1;
                break;
            }
        }
        if (!found) {
            neu++;
        }
    }

    return neu;
}
