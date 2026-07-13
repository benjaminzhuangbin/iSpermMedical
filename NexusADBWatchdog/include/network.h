/**
 * @file network.h
 * @brief TCP listen / established checks for ADB port via /proc/net/tcp.
 *
 * Does not invoke shell utilities. Parses /proc/net/tcp directly.
 */

#ifndef NEXUS_WATCHDOG_NETWORK_H
#define NEXUS_WATCHDOG_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check whether 0.0.0.0:<port> (or any local address) is in LISTEN state.
 *
 * Spec expects listening on 0.0.0.0:5555. Implementation treats a socket
 * whose local port matches and state is LISTEN as listening; prefers
 * wildcard bind (0.0.0.0) when present.
 *
 * @param port  TCP port in host byte order (e.g. 5555).
 * @return 1 if listening, 0 if not, -1 on read/parse error.
 */
int network_is_port_listening(unsigned int port);

/**
 * Count TCP ESTABLISHED connections whose local port equals port.
 *
 * @param port  TCP port in host byte order (e.g. 5555).
 * @return Count (>= 0), or -1 on read/parse error.
 */
int network_count_established(unsigned int port);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_NETWORK_H */
