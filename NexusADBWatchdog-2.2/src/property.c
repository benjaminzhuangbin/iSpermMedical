/**
 * @file property.c
 * @brief Android system property helpers.
 */

#include "property.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

#ifdef __ANDROID__
int __system_property_get(const char *name, char *value);
int __system_property_set(const char *name, const char *value);
#else
/* Host stubs */
static int __system_property_get(const char *name, char *value)
{
    (void)name;
    if (value != NULL) {
        value[0] = '\0';
    }
    return 0;
}
static int __system_property_set(const char *name, const char *value)
{
    (void)name;
    (void)value;
    return 0;
}
#endif

int property_get_str(const char *name, char *buf, unsigned int buflen)
{
    char tmp[92]; /* PROP_VALUE_MAX is 92 on Android */
    int n;

    if (name == NULL || buf == NULL || buflen == 0U) {
        return -1;
    }

    tmp[0] = '\0';
    n = __system_property_get(name, tmp);
    (void)n;

    /* Copy with NUL terminate */
    {
        size_t i;
        for (i = 0U; i + 1U < buflen && tmp[i] != '\0'; i++) {
            buf[i] = tmp[i];
        }
        buf[i] = '\0';
    }
    return 0;
}

int property_set_str(const char *name, const char *value)
{
    if (name == NULL || value == NULL) {
        return -1;
    }
    if (__system_property_set(name, value) != 0) {
        return -1;
    }
    return 0;
}

int property_adb_tcp_port_ok(const char *expected)
{
    char val[64];

    if (expected == NULL) {
        expected = WATCHDOG_ADB_PORT_STR;
    }

    if (property_get_str("persist.adb.tcp.port", val, (unsigned int)sizeof(val)) != 0) {
        return 0;
    }

    return (strcmp(val, expected) == 0) ? 1 : 0;
}
