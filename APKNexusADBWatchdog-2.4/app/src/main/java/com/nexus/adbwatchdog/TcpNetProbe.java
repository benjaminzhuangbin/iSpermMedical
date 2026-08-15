package com.nexus.adbwatchdog;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Parse {@code /proc/net/tcp} for local port 5555 (same approach as native 2.4).
 * Requires root to read reliably on some builds.
 */
public final class TcpNetProbe {

    public static final class Stats {
        public boolean listening;
        public int established;
        public int closeWait;
        public int timeWait;
        public int synRecv;
        public final List<String> clientIps = new ArrayList<String>();
    }

    private static final int TCP_ESTABLISHED = 0x01;
    private static final int TCP_SYN_RECV = 0x03;
    private static final int TCP_TIME_WAIT = 0x06;
    private static final int TCP_CLOSE_WAIT = 0x08;
    private static final int TCP_LISTEN = 0x0A;

    private static final Pattern LINE = Pattern.compile(
            "^\\s*\\d+:\\s*([0-9A-Fa-f]+):([0-9A-Fa-f]+)\\s+([0-9A-Fa-f]+):([0-9A-Fa-f]+)\\s+([0-9A-Fa-f]+)");

    private TcpNetProbe() {
    }

    public static Stats collect(int port) {
        Stats stats = new Stats();
        RootShell.Result r = RootShell.execSu("cat /proc/net/tcp", 8);
        if (!r.ok() && r.stdout.isEmpty()) {
            return stats;
        }
        parse(r.stdout, port, stats);
        // Also try tcp6 if present (rare for this product LAN)
        RootShell.Result r6 = RootShell.execSu("cat /proc/net/tcp6 2>/dev/null", 5);
        if (!r6.stdout.isEmpty()) {
            parse(r6.stdout, port, stats);
        }
        return stats;
    }

    private static void parse(String text, int port, Stats stats) {
        Set<String> uniq = new LinkedHashSet<String>();
        String[] lines = text.split("\n");
        for (String line : lines) {
            Matcher m = LINE.matcher(line);
            if (!m.find()) {
                continue;
            }
            int lport = Integer.parseInt(m.group(2), 16);
            if (lport != port) {
                continue;
            }
            int st = Integer.parseInt(m.group(5), 16);
            long rip = Long.parseLong(m.group(3), 16);
            if (st == TCP_LISTEN) {
                stats.listening = true;
            } else if (st == TCP_ESTABLISHED) {
                stats.established++;
                String ip = formatIpv4(rip);
                if (!"0.0.0.0".equals(ip)) {
                    uniq.add(ip);
                }
            } else if (st == TCP_CLOSE_WAIT) {
                stats.closeWait++;
            } else if (st == TCP_TIME_WAIT) {
                stats.timeWait++;
            } else if (st == TCP_SYN_RECV) {
                stats.synRecv++;
            }
        }
        stats.clientIps.clear();
        stats.clientIps.addAll(uniq);
    }

    /** /proc/net/tcp stores IPv4 little-endian hex. */
    private static String formatIpv4(long ipLe) {
        int b0 = (int) (ipLe & 0xFF);
        int b1 = (int) ((ipLe >> 8) & 0xFF);
        int b2 = (int) ((ipLe >> 16) & 0xFF);
        int b3 = (int) ((ipLe >> 24) & 0xFF);
        return String.format(Locale.US, "%d.%d.%d.%d", b0, b1, b2, b3);
    }

    public static String joinClients(List<String> ips) {
        if (ips == null || ips.isEmpty()) {
            return "";
        }
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < ips.size(); i++) {
            if (i > 0) sb.append(',');
            sb.append(ips.get(i));
        }
        return sb.toString();
    }
}
