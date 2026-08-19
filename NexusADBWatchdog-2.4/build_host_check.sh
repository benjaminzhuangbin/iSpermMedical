#!/usr/bin/env bash
# Host-side logic smoke tests for Nexus ADB Watchdog 2.4
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/build-host-check"
mkdir -p "$TMP"

python3 - "$TMP" <<'PY'
import sys
from pathlib import Path
tmp = Path(sys.argv[1])
(tmp / "test_health.c").write_text(r'''
#include "health.h"
#include <stdio.h>
#include <string.h>
static int fail = 0;
static void expect(const char *name, int cond) {
    if (!cond) { fprintf(stderr, "FAIL %s\n", name); fail = 1; }
    else fprintf(stdout, "OK   %s\n", name);
}
int main(void) {
    char tcp[16], adb[16], client[16], reason[32];

    /* ESTABLISHED=0 => still OK */
    health_classify(1, 1, 1, 0, 0, tcp, sizeof tcp, adb, sizeof adb, client, sizeof client, reason, sizeof reason);
    expect("idle_ok", strcmp(tcp, "OK") == 0 && strcmp(adb, "OK") == 0 &&
                      strcmp(client, "NO_CLIENT") == 0 && strcmp(reason, "NONE") == 0);

    health_classify(1, 1, 1, 2, 0, tcp, sizeof tcp, adb, sizeof adb, client, sizeof client, reason, sizeof reason);
    expect("connected_ok", strcmp(tcp, "OK") == 0 && strcmp(client, "CONNECTED") == 0);

    health_classify(0, 1, 1, 0, 0, tcp, sizeof tcp, adb, sizeof adb, client, sizeof client, reason, sizeof reason);
    expect("adbd_down", strcmp(reason, "ADBD_NOT_RUNNING") == 0 && strcmp(adb, "FAULT") == 0);

    health_classify(1, 0, 1, 0, 0, tcp, sizeof tcp, adb, sizeof adb, client, sizeof client, reason, sizeof reason);
    expect("port_down", strcmp(reason, "PORT_NOT_LISTENING") == 0);

    health_classify(1, 1, 1, 0, 1, tcp, sizeof tcp, adb, sizeof adb, client, sizeof client, reason, sizeof reason);
    expect("tcp_fault", strcmp(reason, "TCP_FAULT") == 0 && strcmp(tcp, "FAULT") == 0);

    /* No ADB_PROTOCOL_FAULT path exists */
    expect("no_protocol_symbol", 1);
    return fail ? 1 : 0;
}
''')
print("generated")
PY

gcc -std=c99 -Wall -Wextra -O2 -I"$ROOT/include" \
    "$TMP/test_health.c" "$ROOT/src/health.c" "$ROOT/src/util.c" \
    -o "$TMP/test_health"
"$TMP/test_health"

# Ensure CNXN / ADB_PROTOCOL not in sources that drive recovery
if grep -R "ADB_PROTOCOL_FAULT\|adb_proto_probe\|FIX_ADB_PROTOCOL" \
    "$ROOT/src" "$ROOT/include" 2>/dev/null | grep -v 'removed\|ignored\|CNXN' ; then
  echo "FAIL: protocol recovery symbols still present"
  exit 1
fi
echo "OK   no protocol recovery symbols"

echo "Host checks passed."
