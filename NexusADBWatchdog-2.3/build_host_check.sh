#!/usr/bin/env bash
# Host-side compile + logic smoke tests for Nexus ADB Watchdog 2.3
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/build-host-check"
mkdir -p "$TMP"

python3 - "$TMP" <<'PY'
import sys
from pathlib import Path
tmp = Path(sys.argv[1])
(tmp / "test_health.c").write_text("""#include \"health.h\"
#include <stdio.h>
#include <string.h>
static int fail = 0;
static void expect(const char *name, int cond) {
    if (!cond) { fprintf(stderr, \"FAIL %s\\n\", name); fail = 1; }
    else fprintf(stdout, \"OK   %s\\n\", name);
}
int main(void) {
    char tcp[16], adb[16], reason[32];
    health_classify(1, 1, 1, 0, 0, 0, 1, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"no_client\", strcmp(tcp, \"NO_CLIENT\") == 0 && strcmp(reason, \"NONE\") == 0 && strcmp(adb, \"OK\") == 0);
    health_classify(1, 1, 1, 2, 0, 0, 1, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"ok_with_client\", strcmp(tcp, \"OK\") == 0 && strcmp(adb, \"OK\") == 0);
    health_classify(0, 1, 1, 0, 0, 0, -1, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"adbd_down\", strcmp(reason, \"ADBD_NOT_RUNNING\") == 0);
    health_classify(1, 0, 1, 0, 0, 0, -1, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"port_down\", strcmp(reason, \"PORT_NOT_LISTENING\") == 0);
    health_classify(1, 1, 1, 0, 0, 0, 0, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"protocol_fault\", strcmp(reason, \"ADB_PROTOCOL_FAULT\") == 0 && strcmp(adb, \"FAULT\") == 0);
    health_classify(1, 1, 1, 1, 0, 1, 1, tcp, sizeof tcp, adb, sizeof adb, reason, sizeof reason);
    expect(\"session_stale\", strcmp(reason, \"SESSION_STALE\") == 0);
    return fail ? 1 : 0;
}
""")
(tmp / "test_proto.c").write_text("""#include \"adb_proto.h\"
#include <stdio.h>
int main(void) {
    int rc = adb_proto_probe(1, 200);
    if (rc != 0 && rc != -1) {
        fprintf(stderr, \"unexpected probe rc=%d\\n\", rc);
        return 1;
    }
    printf(\"OK   adb_proto_probe_closed_port rc=%d\\n\", rc);
    return 0;
}
""")
print("generated")
PY

gcc -std=c99 -Wall -Wextra -O2 -I"$ROOT/include" \
    "$TMP/test_health.c" "$ROOT/src/health.c" "$ROOT/src/util.c" \
    -o "$TMP/test_health"
"$TMP/test_health"

gcc -std=c99 -Wall -Wextra -O2 -I"$ROOT/include" \
    "$TMP/test_proto.c" "$ROOT/src/adb_proto.c" "$ROOT/src/util.c" \
    -o "$TMP/test_proto"
"$TMP/test_proto"

echo "Host checks passed."
