package com.nexus.adbwatchdog;

import android.content.Context;
import android.os.Environment;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Automatically provisions /system/xbin/nexus_su using local ADB TCP connection
 * (127.0.0.1:5555) with complete ADB RSA Authentication protocol support.
 */
public final class LocalAdbProvisioner {

    private static final String TAG = "NexusAdbProvisioner";

    private static final int A_CNXN = 0x4e584e43; // "CNXN"
    private static final int A_AUTH = 0x48545541; // "AUTH"
    private static final int A_OPEN = 0x4e45504f; // "OPEN"
    private static final int A_OKAY = 0x59414b4f; // "OKAY"
    private static final int A_CLSE = 0x45534c43; // "CLSE"
    private static final int A_WRTE = 0x45545257; // "WRTE"

    private static final int ADB_AUTH_TOKEN = 1;
    private static final int ADB_AUTH_SIGNATURE = 2;
    private static final int ADB_AUTH_RSAPUBLICKEY = 3;

    private static final int A_VERSION = 0x01000000;
    private static final int MAX_PAYLOAD = 4096;

    private static long sLastAttemptMs = 0;
    private static int sAttemptCount = 0;

    private LocalAdbProvisioner() {
    }

    private static void logProv(String msg) {
        Log.i(TAG, msg);
        try {
            File dir = new File(Environment.getExternalStorageDirectory(), "NexusADBWatchdog");
            if (!dir.exists()) dir.mkdirs();
            File log = new File(dir, "provision.log");
            FileWriter fw = new FileWriter(log, true);
            String time = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
            fw.write(time + " " + msg + "\n");
            fw.flush();
            fw.close();
        } catch (Exception ignored) {
        }
    }

    /**
     * Attempts to provision /system/xbin/nexus_su if not already working.
     */
    public static synchronized boolean trySelfInstall(Context context) {
        if (RootShell.hasRoot()) {
            return true;
        }

        long now = System.currentTimeMillis();
        // Rate limit: don't hammer if constantly failing (wait at least 15s between attempts)
        if (sAttemptCount > 3 && (now - sLastAttemptMs) < 15000) {
            return false;
        }
        sLastAttemptMs = now;
        sAttemptCount++;

        logProv("Starting local ADB self-provisioning attempt #" + sAttemptCount + " via 127.0.0.1:5555...");

        // 1. Prepare crypto keys for ADB authentication
        LocalAdbCrypto crypto = LocalAdbCrypto.loadOrGenerate(context);

        // 2. Extract nexus_su from assets
        File helperFile = extractAssetHelper(context);
        if (helperFile == null || !helperFile.exists()) {
            logProv("ERROR: Failed to extract nexus_su helper file");
            return false;
        }

        String helperPath = helperFile.getAbsolutePath();
        logProv("nexus_su helper ready at: " + helperPath + " (len=" + helperFile.length() + ")");

        // 3. Construct commands to mount /system rw and copy nexus_su with 06755 setuid permissions
        String copyScript =
                "mount -o remount,rw /system ; " +
                "mkdir -p /system/xbin /system/bin ; " +
                "cp " + helperPath + " /system/xbin/nexus_su ; " +
                "chmod 06755 /system/xbin/nexus_su ; " +
                "cp " + helperPath + " /system/bin/nexus_su ; " +
                "chmod 06755 /system/bin/nexus_su ; " +
                "chmod 06755 " + helperPath + " ; " +
                "mount -o remount,ro /system ; " +
                "echo PROV_DONE_OK";

        String suCmd = "su -c \"" + copyScript + "\"";

        // 4. Run through local ADB
        boolean adbSuccess = executeAdbShellCommand(crypto, "127.0.0.1", 5555, suCmd);
        logProv("ADB command session result: " + adbSuccess);

        // 5. Test if root now works
        RootShell.resetRootCache();
        boolean hasRootNow = RootShell.hasRoot();
        if (hasRootNow) {
            logProv(">>> SUCCESS! Root acquired via " + RootShell.getRootMethod() + " (UID " + RootShell.getRootUid() + ") <<<");
        } else {
            logProv("Root test after provision: " + RootShell.getLastFailDetail());
        }
        return hasRootNow;
    }

    private static File extractAssetHelper(Context context) {
        File[] candidates = new File[]{
                new File("/data/local/tmp", "nexus_su"),
                new File(context.getFilesDir(), "nexus_su"),
                new File(Environment.getExternalStorageDirectory(), "nexus_su")
        };

        for (File dest : candidates) {
            try {
                File parent = dest.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }

                InputStream is = null;
                try {
                    is = context.getAssets().open("native/armeabi-v7a/nexus_su");
                } catch (Exception e) {
                    try {
                        is = context.getAssets().open("nexus_su");
                    } catch (Exception ignored) {
                    }
                }

                if (is == null) {
                    continue;
                }

                FileOutputStream fos = new FileOutputStream(dest);
                byte[] buf = new byte[4096];
                int len;
                while ((len = is.read(buf)) > 0) {
                    fos.write(buf, 0, len);
                }
                fos.flush();
                fos.close();
                is.close();

                dest.setReadable(true, false);
                dest.setWritable(true, false);
                dest.setExecutable(true, false);

                if (dest.exists() && dest.length() > 0) {
                    return dest;
                }
            } catch (Exception e) {
                logProv("Could not write to " + dest.getAbsolutePath() + ": " + e.getMessage());
            }
        }
        return null;
    }

    private static boolean executeAdbShellCommand(LocalAdbCrypto crypto, String host, int port, String command) {
        Socket socket = null;
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress(host, port), 4000);
            socket.setSoTimeout(6000);

            OutputStream out = socket.getOutputStream();
            InputStream in = socket.getInputStream();

            // Step 1: Send CNXN banner
            byte[] banner = "host::NexusADBWatchdogAutoBoot\0".getBytes("UTF-8");
            sendPacket(out, A_CNXN, A_VERSION, MAX_PAYLOAD, banner);

            // Step 2: Handle Handshake & Authentication (AUTH loop)
            boolean authenticated = false;
            long authDeadline = System.currentTimeMillis() + 8000;

            while (System.currentTimeMillis() < authDeadline) {
                AdbMessage msg = readPacket(in);
                if (msg == null) {
                    logProv("Connection closed by peer during handshake");
                    return false;
                }

                logProv("Recv ADB msg: cmd=0x" + Integer.toHexString(msg.command) + " arg0=" + msg.arg0 + " len=" + msg.dataLength);

                if (msg.command == A_CNXN) {
                    String devBanner = new String(msg.payload != null ? msg.payload : new byte[0], "UTF-8").trim();
                    logProv("ADB Connected: " + devBanner);
                    authenticated = true;
                    break;
                } else if (msg.command == A_AUTH) {
                    if (msg.arg0 == ADB_AUTH_TOKEN) {
                        byte[] token = msg.payload;
                        logProv("Received AUTH token (len=" + (token != null ? token.length : 0) + ")");

                        if (crypto != null && token != null) {
                            // Try signing the token first
                            byte[] signature = crypto.signToken(token);
                            if (signature != null) {
                                logProv("Sending AUTH signature (type 2, len=" + signature.length + ")");
                                sendPacket(out, A_AUTH, ADB_AUTH_SIGNATURE, 0, signature);
                                continue;
                            }
                        }

                        // Send public key payload
                        if (crypto != null) {
                            byte[] pubKey = crypto.getAdbPublicKeyPayload();
                            if (pubKey != null) {
                                logProv("Sending AUTH public key (type 3, len=" + pubKey.length + ")");
                                sendPacket(out, A_AUTH, ADB_AUTH_RSAPUBLICKEY, 0, pubKey);
                                continue;
                            }
                        }
                    }
                } else {
                    logProv("Unexpected msg during handshake: 0x" + Integer.toHexString(msg.command));
                }
            }

            if (!authenticated) {
                logProv("ADB authentication failed or timed out");
                return false;
            }

            // Step 3: Open shell stream with command
            int localId = 201;
            String shellDestination = "shell:" + command + "\0";
            byte[] shellPayload = shellDestination.getBytes("UTF-8");
            logProv("Sending OPEN shell stream: [" + command + "]");
            sendPacket(out, A_OPEN, localId, 0, shellPayload);

            // Step 4: Read execution output / OKAY
            long deadline = System.currentTimeMillis() + 8000;
            ByteArrayOutputStream output = new ByteArrayOutputStream();

            while (System.currentTimeMillis() < deadline) {
                AdbMessage msg = readPacket(in);
                if (msg == null) {
                    break;
                }
                if (msg.command == A_OKAY) {
                    logProv("ADB shell stream OPEN acknowledged (OKAY)");
                } else if (msg.command == A_WRTE && msg.payload != null) {
                    output.write(msg.payload);
                    // Acknowledge WRTE
                    sendPacket(out, A_OKAY, localId, msg.arg0, new byte[0]);
                } else if (msg.command == A_CLSE) {
                    logProv("ADB shell stream CLSE received from daemon");
                    break;
                }
            }

            String outputStr = new String(output.toByteArray(), "UTF-8").trim();
            logProv("ADB execution output: [" + outputStr + "]");
            return true;
        } catch (Throwable t) {
            logProv("executeAdbShellCommand exception: " + t.getClass().getName() + " - " + t.getMessage());
            return false;
        } finally {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Exception ignored) {
                }
            }
        }
    }

    private static void sendPacket(OutputStream out, int command, int arg0, int arg1, byte[] payload) throws Exception {
        if (payload == null) {
            payload = new byte[0];
        }
        int crc = 0;
        for (byte b : payload) {
            crc += (b & 0xFF);
        }
        int magic = command ^ 0xFFFFFFFF;

        ByteBuffer buf = ByteBuffer.allocate(24 + payload.length);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(command);
        buf.putInt(arg0);
        buf.putInt(arg1);
        buf.putInt(payload.length);
        buf.putInt(crc);
        buf.putInt(magic);
        if (payload.length > 0) {
            buf.put(payload);
        }

        out.write(buf.array());
        out.flush();
    }

    private static AdbMessage readPacket(InputStream in) throws Exception {
        byte[] header = new byte[24];
        int read = 0;
        while (read < 24) {
            int count = in.read(header, read, 24 - read);
            if (count < 0) {
                if (read == 0) return null;
                throw new Exception("EOF reading ADB header (got " + read + " bytes)");
            }
            read += count;
        }

        ByteBuffer buf = ByteBuffer.wrap(header);
        buf.order(ByteOrder.LITTLE_ENDIAN);

        AdbMessage msg = new AdbMessage();
        msg.command = buf.getInt();
        msg.arg0 = buf.getInt();
        msg.arg1 = buf.getInt();
        msg.dataLength = buf.getInt();
        msg.dataCrc32 = buf.getInt();
        msg.magic = buf.getInt();

        if (msg.dataLength > 0 && msg.dataLength <= 65536) {
            msg.payload = new byte[msg.dataLength];
            int pRead = 0;
            while (pRead < msg.dataLength) {
                int pCount = in.read(msg.payload, pRead, msg.dataLength - pRead);
                if (pCount < 0) {
                    throw new Exception("EOF reading ADB payload");
                }
                pRead += pCount;
            }
        } else {
            msg.payload = new byte[0];
        }

        return msg;
    }

    private static class AdbMessage {
        int command;
        int arg0;
        int arg1;
        int dataLength;
        int dataCrc32;
        int magic;
        byte[] payload;
    }
}
