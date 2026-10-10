package com.nexus.adbwatchdog;

import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/**
 * Out-of-band (OOB) UDP control channel on port 5556.
 * Provides an independent recovery path when Android adbd enters a zombie/offline state
 * and standard TCP ADB commands cannot reach adbd.
 *
 * Protocol:
 * 1. PC sends UDP datagram to 192.168.31.11:5556 containing: "RECOVER_ADBD"
 * 2. Watchdog responds with: "ACK:RECOVER_ADBD:PID=<pid>"
 * 3. Watchdog invokes RecoveryEngine.triggerOobRecovery() with a 15-second debounce.
 */
public final class OobRecoveryServer {

    private static final String TAG = "OobRecoveryServer";
    public static final int DEFAULT_PORT = 5556;

    private final int port;
    private final RecoveryEngine recoveryEngine;
    private final WatchdogLogger logger;
    private volatile boolean running;
    private DatagramSocket socket;
    private Thread serverThread;

    public OobRecoveryServer(int port, RecoveryEngine recoveryEngine, WatchdogLogger logger) {
        this.port = port;
        this.recoveryEngine = recoveryEngine;
        this.logger = logger;
    }

    public synchronized void start() {
        if (running) {
            return;
        }
        running = true;
        serverThread = new Thread(new Runnable() {
            @Override
            public void run() {
                runLoop();
            }
        }, "oob-recovery-udp-5556");
        serverThread.setDaemon(true);
        serverThread.start();
        Log.i(TAG, "OOB Recovery UDP server started on port " + port);
    }

    public synchronized void stop() {
        running = false;
        if (socket != null && !socket.isClosed()) {
            try {
                socket.close();
            } catch (Throwable ignored) {
            }
        }
        if (serverThread != null) {
            serverThread.interrupt();
            serverThread = null;
        }
        Log.i(TAG, "OOB Recovery UDP server stopped");
    }

    private void runLoop() {
        byte[] buf = new byte[1024];
        try {
            socket = new DatagramSocket(port);
            while (running) {
                try {
                    DatagramPacket packet = new DatagramPacket(buf, buf.length);
                    socket.receive(packet);

                    String msg = new String(packet.getData(), packet.getOffset(), packet.getLength(), "UTF-8").trim();
                    InetAddress clientAddr = packet.getAddress();
                    int clientPort = packet.getPort();

                    if (msg.contains("RECOVER_ADBD")) {
                        Log.w(TAG, "Received OOB RECOVER_ADBD from " + clientAddr.getHostAddress() + ":" + clientPort);
                        if (logger != null) {
                            logger.event("OOB RECOVERY REQUEST",
                                    "source=" + clientAddr.getHostAddress() + ":" + clientPort,
                                    "msg=" + msg);
                        }

                        // Send instant ACK back to PC
                        String ack = "ACK:RECOVER_ADBD:TIME=" + System.currentTimeMillis() + "\n";
                        byte[] ackBytes = ack.getBytes("UTF-8");
                        DatagramPacket ackPacket = new DatagramPacket(ackBytes, ackBytes.length, clientAddr, clientPort);
                        try {
                            socket.send(ackPacket);
                        } catch (Throwable t) {
                            Log.e(TAG, "Failed to send ACK to " + clientAddr, t);
                        }

                        // Trigger recovery on adbd
                        if (recoveryEngine != null) {
                            recoveryEngine.triggerOobRecovery("UDP_5556:" + clientAddr.getHostAddress(), logger);
                        }
                    } else if (msg.contains("PING")) {
                        String ack = "PONG:NEXUS_WATCHDOG_2.4\n";
                        byte[] ackBytes = ack.getBytes("UTF-8");
                        DatagramPacket ackPacket = new DatagramPacket(ackBytes, ackBytes.length, clientAddr, clientPort);
                        socket.send(ackPacket);
                    }
                } catch (Throwable t) {
                    if (!running) {
                        break;
                    }
                    Log.w(TAG, "Error processing UDP packet: " + t.getMessage());
                }
            }
        } catch (Throwable t) {
            if (running) {
                Log.e(TAG, "OOB UDP socket failed on port " + port, t);
            }
        } finally {
            if (socket != null && !socket.isClosed()) {
                try {
                    socket.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }
}
