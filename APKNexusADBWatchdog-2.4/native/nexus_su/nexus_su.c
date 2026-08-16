/**
 * nexus_su — setuid-root helper for Nexus ADB Watchdog APK
 *
 * Why this exists (RK3288 / Android 5.1.1):
 *   Stock /system/xbin/su allows shell (uid 2000) but rejects app UIDs:
 *     "su: uid 10053 not allowed to su"
 *   Wrapping with sh does not help — the check is on the caller's real UID.
 *
 * Factory installs this binary to /system/xbin/nexus_su (or /system/bin),
 * owned by root:root with mode 06755 (setuid). Any process that execs it
 * gets euid=0 via the kernel setuid bit; this helper then setuid(0) and
 * runs the requested command. It does NOT implement an app-UID allowlist.
 *
 * Usage (same shape as su -c):
 *   nexus_su -c 'id'
 *   nexus_su -c 'setprop ctl.start adbd'
 *
 * Security note: intended only for closed medical-device firmware where
 * the manufacturer controls the system image. Not a general SuperSU.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

static void drop_to_root(void) {
    /* Kernel already raised euid if the binary is setuid-root.
     * Force ruid/euid/suid and gids to 0 so child commands see uid=0(root). */
    if (setgid(0) != 0) {
        fprintf(stderr, "nexus_su: setgid(0) failed errno=%d\n", errno);
        _exit(127);
    }
    if (setuid(0) != 0) {
        fprintf(stderr, "nexus_su: setuid(0) failed errno=%d (is binary setuid-root on /system?)\n",
                errno);
        _exit(127);
    }
}

int main(int argc, char **argv) {
    drop_to_root();

    if (argc == 1) {
        /* Prove elevation: print id and exit. */
        execl("/system/bin/sh", "sh", "-c", "id", (char *) NULL);
        fprintf(stderr, "nexus_su: exec sh failed errno=%d\n", errno);
        return 127;
    }

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        /* Pass the whole command string to sh -c (same as su -c). */
        execl("/system/bin/sh", "sh", "-c", argv[2], (char *) NULL);
        fprintf(stderr, "nexus_su: exec sh -c failed errno=%d\n", errno);
        return 127;
    }

    fprintf(stderr, "usage: nexus_su -c <command>\n");
    return 1;
}
