/**
 * @file network.h
 * @brief TCP diagnostics for ADB port via /proc/net/tcp (no netstat/shell).
 *
 * Version 1.5: full TCP state counters, client IP list, connection tracking.
 */

#ifndef NEXUS_WATCHDOG_NETWORK_H
#define NEXUS_WATCHDOG_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum tracked remote endpoints for cumulative connection counting. */
#define NETWORK_MAX_ENDPOINTS   64

/** Maximum unique client IP strings stored in one snapshot. */
#define NETWORK_MAX_CLIENTS     16

/** Max length of a dotted IPv4 string including NUL. */
#define NETWORK_IP_STR_LEN      16

/** Max length of comma-separated CLIENTS= field. */
#define NETWORK_CLIENTS_BUF_LEN 256

/**
 * One remote TCP endpoint (IPv4 + port) used for connection tracking.
 */
typedef struct network_endpoint {
    unsigned int ip;     /**< IPv4 as stored in /proc/net/tcp (host uint32). */
    unsigned int port;   /**< Remote port (host order). */
} network_endpoint_t;

/**
 * Per-cycle TCP statistics for a local port (typically 5555).
 */
typedef struct network_tcp_stats {
    int listening;          /**< 1 if any LISTEN socket on the port. */

    int established;
    int time_wait;
    int close_wait;
    int syn_recv;
    int fin_wait1;
    int fin_wait2;
    int last_ack;
    int closing;
    int close_state;        /**< TCP state CLOSE (0x07). */

    /** Unique remote client IPs currently ESTABLISHED, comma-separated. */
    char clients[NETWORK_CLIENTS_BUF_LEN];

    /** Current ESTABLISHED endpoints (for cumulative new-connection tracking). */
    network_endpoint_t endpoints[NETWORK_MAX_ENDPOINTS];
    int endpoint_count;
} network_tcp_stats_t;

/**
 * Parse /proc/net/tcp and fill stats for sockets whose local port matches.
 * Does not invoke netstat or any shell utility.
 *
 * @param port   Local TCP port in host byte order (e.g. 5555).
 * @param stats  Output statistics (must not be NULL); cleared on entry.
 * @return 0 on success, -1 if /proc/net/tcp cannot be read.
 */
int network_collect_tcp_stats(unsigned int port, network_tcp_stats_t *stats);

/**
 * Count how many endpoints in 'current' were not present in 'previous'.
 * Used to accumulate TOTAL_CONNECTIONS across cycles.
 *
 * @param previous       Previous cycle endpoints (may be NULL if count==0).
 * @param previous_count Number of previous endpoints.
 * @param current        Current cycle endpoints.
 * @param current_count  Number of current endpoints.
 * @return Number of newly observed endpoints (>= 0).
 */
int network_count_new_endpoints(const network_endpoint_t *previous,
                                int previous_count,
                                const network_endpoint_t *current,
                                int current_count);

/**
 * Format a /proc/net/tcp IPv4 value as dotted decimal into buf.
 *
 * @param ip_le  Address value as parsed from /proc/net/tcp.
 * @param buf    Output buffer.
 * @param buflen Capacity of buf.
 */
void network_format_ipv4(unsigned int ip_le, char *buf, unsigned int buflen);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_NETWORK_H */
