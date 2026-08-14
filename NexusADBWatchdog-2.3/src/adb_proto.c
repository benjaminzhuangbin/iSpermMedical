/**
 * @file adb_proto.c
 * @brief Localhost ADB CNXN protocol health check for RK3288 / Android 5.1.1.
 *
 * Wire format matches historical AOSP adb (amessage + payload checksum).
 * No shell utilities (wc/tail/netstat) are used.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "adb_proto.h"
#include "config.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <poll.h>
#endif

#define A_CNXN  0x4e584e43u  /* 'CNXN' */
#define A_AUTH  0x48545541u  /* 'AUTH' */
#define A_VERSION 0x01000000u
#define A_MAXDATA 4096u

#define ADB_PROTO_INJECT_FLAG "/data/local/watchdog/.adb_protocol_fault"

#pragma pack(push, 1)
typedef struct {
    unsigned int command;
    unsigned int arg0;
    unsigned int arg1;
    unsigned int data_length;
    unsigned int data_check;
    unsigned int magic;
} adb_amessage_t;
#pragma pack(pop)

static unsigned int adb_checksum(const unsigned char *data, unsigned int len)
{
    unsigned int sum = 0U;
    unsigned int i;
    if (data == NULL) {
        return 0U;
    }
    for (i = 0U; i < len; i++) {
        sum += (unsigned int)data[i];
    }
    return sum;
}

static int adb_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

static int adb_poll_fd(int fd, short events, int timeout_ms)
{
#if defined(__linux__)
    struct pollfd pfd;
    int rc;

    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    rc = poll(&pfd, 1, timeout_ms);
    if (rc <= 0) {
        return rc;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }
    if (pfd.revents & events) {
        return 1;
    }
    return 0;
#else
    (void)fd;
    (void)events;
    (void)timeout_ms;
    return -1;
#endif
}

static int adb_connect_localhost(unsigned int port, int timeout_ms)
{
    int fd;
    struct sockaddr_in addr;
    int rc;
    int soerr;
    socklen_t solen;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (adb_set_nonblock(fd) != 0) {
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == 0) {
        return fd;
    }
    if (errno != EINPROGRESS && errno != EALREADY && errno != EWOULDBLOCK) {
        close(fd);
        return -1;
    }

    rc = adb_poll_fd(fd, POLLOUT, timeout_ms);
    if (rc <= 0) {
        close(fd);
        return -1;
    }

    soerr = 0;
    solen = (socklen_t)sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &solen) != 0 || soerr != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int adb_send_all(int fd, const void *buf, unsigned int len, int timeout_ms)
{
    const unsigned char *p = (const unsigned char *)buf;
    unsigned int sent = 0U;

    while (sent < len) {
        ssize_t n;
        int pr = adb_poll_fd(fd, POLLOUT, timeout_ms);
        if (pr <= 0) {
            return -1;
        }
        n = send(fd, p + sent, (size_t)(len - sent), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += (unsigned int)n;
    }
    return 0;
}

static int adb_recv_exact(int fd, void *buf, unsigned int len, int timeout_ms)
{
    unsigned char *p = (unsigned char *)buf;
    unsigned int got = 0U;

    while (got < len) {
        ssize_t n;
        int pr = adb_poll_fd(fd, POLLIN, timeout_ms);
        if (pr <= 0) {
            return -1;
        }
        n = recv(fd, p + got, (size_t)(len - got), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        got += (unsigned int)n;
    }
    return 0;
}

int adb_proto_probe(unsigned int port, int timeout_ms)
{
    int fd;
    adb_amessage_t msg;
    adb_amessage_t rsp;
    const char *banner = "host::";
    unsigned int banner_len;
    unsigned char payload[64];
    unsigned int i;

    if (timeout_ms < 100) {
        timeout_ms = 100;
    }
    if (timeout_ms > 10000) {
        timeout_ms = 10000;
    }
    if (port == 0U || port > 65535U) {
        return -1;
    }

    banner_len = (unsigned int)strlen(banner) + 1U; /* include NUL */
    if (banner_len > sizeof(payload)) {
        return -1;
    }
    for (i = 0U; i < banner_len; i++) {
        payload[i] = (unsigned char)banner[i];
    }

    fd = adb_connect_localhost(port, timeout_ms);
    if (fd < 0) {
        return 0; /* connect failed => protocol/service unhealthy */
    }

    memset(&msg, 0, sizeof(msg));
    msg.command = A_CNXN;
    msg.arg0 = A_VERSION;
    msg.arg1 = A_MAXDATA;
    msg.data_length = banner_len;
    msg.data_check = adb_checksum(payload, banner_len);
    msg.magic = msg.command ^ 0xffffffffu;

    if (adb_send_all(fd, &msg, (unsigned int)sizeof(msg), timeout_ms) != 0 ||
        adb_send_all(fd, payload, banner_len, timeout_ms) != 0) {
        close(fd);
        return 0;
    }

    memset(&rsp, 0, sizeof(rsp));
    if (adb_recv_exact(fd, &rsp, (unsigned int)sizeof(rsp), timeout_ms) != 0) {
        close(fd);
        return 0;
    }
    close(fd);

    /* Validate magic and accept CNXN or AUTH as healthy protocol response. */
    if (rsp.magic != (rsp.command ^ 0xffffffffu)) {
        return 0;
    }
    if (rsp.command == A_CNXN || rsp.command == A_AUTH) {
        return 1;
    }
    return 0;
}

int adb_proto_inject_fault_set(void)
{
    FILE *fp = fopen(ADB_PROTO_INJECT_FLAG, "w");
    if (fp == NULL) {
        return -1;
    }
    (void)fputs("1\n", fp);
    (void)fclose(fp);
    return 0;
}

int adb_proto_inject_fault_clear(void)
{
    (void)unlink(ADB_PROTO_INJECT_FLAG);
    return 0;
}

int adb_proto_inject_fault_active(void)
{
    FILE *fp = fopen(ADB_PROTO_INJECT_FLAG, "r");
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}
