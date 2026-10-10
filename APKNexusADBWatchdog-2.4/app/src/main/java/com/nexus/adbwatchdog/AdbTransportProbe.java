package com.nexus.adbwatchdog;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Validates whether the ADB transport protocol layer is actually responsive.
 * Connects locally (127.0.0.1:5555) and performs an ADB CNXN framing probe.
 *
 * Distinguishes:
 * 1. Process exists + TCP 5555 listening (TCP_HEALTH=OK)
 * 2. Real ADB protocol responds to handshake (ADB_TRANSPORT_HEALTH=OK)
 *
 * This detects when adbd enters an "offline / zombie" state after ~20 hours of uptime.
 */
public final class AdbTransportProbe {

    private static final int A_CNXN = 0x4e584e43; // 'CNXN'
    private static final int A_AUTH = 0x48545541; // 'AUTH'
    private static final int A_VERSION = 0x01000000;
    private static final int MAX_PAYLOAD = 4096;

    public static final class ProbeResult {
        public final boolean alive;
        public final String status; // "OK", "TIMEOUT", "REFUSED", "EOF", "ERROR"
        public final long latencyMs;

        public ProbeResult(boolean alive, String status, long latencyMs) {
            this.alive = alive;
            this.status = status;
            this.latencyMs = latencyMs;
        }
    }

    private AdbTransportProbe() {
    }

    /**
     * Sends an ADB CNXN packet to 127.0.0.1:port with a timeout.
     * If adbd responds with A_AUTH or A_CNXN, the transport layer is 100% healthy.
     * If socket times out, hangs, or closes abruptly, adbd transport is dead/offline.
     */
    public static ProbeResult probe(int port, int timeoutMs) {
        long start = System.currentTimeMillis();
        Socket socket = null;
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress("127.0.0.1", port), Math.min(timeoutMs, 2000));
            socket.setSoTimeout(Math.min(timeoutMs, 3000));

            OutputStream out = socket.getOutputStream();
            InputStream in = socket.getInputStream();

            // Construct standard A_CNXN packet
            byte[] banner = "host::NexusWatchdogTransportProbe\0".getBytes("UTF-8");
            ByteBuffer header = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
            header.putInt(A_CNXN);
            header.putInt(A_VERSION);
            header.putInt(MAX_PAYLOAD);
            header.putInt(banner.length);

            int crc = 0;
            for (byte b : banner) {
                crc += (b & 0xFF);
            }
            header.putInt(crc);
            header.putInt(~A_CNXN);

            out.write(header.array());
            out.write(banner);
            out.flush();

            // Read response 24-byte header
            byte[] respHeader = new byte[24];
            int readTotal = 0;
            while (readTotal < 24) {
                int r = in.read(respHeader, readTotal, 24 - readTotal);
                if (r < 0) {
                    return new ProbeResult(false, "EOF", System.currentTimeMillis() - start);
                }
                readTotal += r;
            }

            ByteBuffer respBuf = ByteBuffer.wrap(respHeader).order(ByteOrder.LITTLE_ENDIAN);
            int cmd = respBuf.getInt();

            // Healthy adbd responds with A_AUTH (0x48545541) or A_CNXN (0x4e584e43)
            if (cmd == A_AUTH || cmd == A_CNXN) {
                return new ProbeResult(true, "OK", System.currentTimeMillis() - start);
            }

            return new ProbeResult(false, "UNEXPECTED_CMD_0x" + Integer.toHexString(cmd), System.currentTimeMillis() - start);
        } catch (java.net.SocketTimeoutException e) {
            return new ProbeResult(false, "TIMEOUT", System.currentTimeMillis() - start);
        } catch (java.net.ConnectException e) {
            return new ProbeResult(false, "REFUSED", System.currentTimeMillis() - start);
        } catch (Throwable t) {
            return new ProbeResult(false, "ERROR:" + t.getClass().getSimpleName(), System.currentTimeMillis() - start);
        } finally {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }
}
