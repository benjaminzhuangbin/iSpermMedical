/**
 * @file property.h
 * @brief Android system property get/set helpers (Bionic).
 */

#ifndef NEXUS_WATCHDOG_PROPERTY_H
#define NEXUS_WATCHDOG_PROPERTY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read a system property into buf.
 *
 * @param name    Property name.
 * @param buf     Output buffer.
 * @param buflen  Capacity.
 * @return 0 on success (including empty value), -1 on failure.
 */
int property_get_str(const char *name, char *buf, unsigned int buflen);

/**
 * Set a system property.
 *
 * @return 0 on success, -1 on failure.
 */
int property_set_str(const char *name, const char *value);

/**
 * Return 1 if persist.adb.tcp.port equals expected port string ("5555").
 */
int property_adb_tcp_port_ok(const char *expected);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_WATCHDOG_PROPERTY_H */
