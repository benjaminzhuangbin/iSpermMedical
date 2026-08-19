package com.nexus.adbwatchdog;

import android.content.Context;
import android.util.Base64;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

/**
 * Generates and manages 2048-bit RSA keys conforming to the Android ADB authentication protocol.
 */
public final class LocalAdbCrypto {

    private static final String TAG = "NexusAdbCrypto";
    private static final int KEY_SIZE_BITS = 2048;
    private static final int KEY_SIZE_WORDS = KEY_SIZE_BITS / 32; // 64

    private final KeyPair keyPair;

    private LocalAdbCrypto(KeyPair keyPair) {
        this.keyPair = keyPair;
    }

    public static LocalAdbCrypto loadOrGenerate(Context context) {
        File dir = context.getFilesDir();
        File privFile = new File(dir, "adb_key.priv");
        File pubFile = new File(dir, "adb_key.pub");

        try {
            if (privFile.exists() && pubFile.exists()) {
                byte[] privBytes = readFile(privFile);
                byte[] pubBytes = readFile(pubFile);
                KeyFactory kf = KeyFactory.getInstance("RSA");
                PrivateKey priv = kf.generatePrivate(new PKCS8EncodedKeySpec(privBytes));
                java.security.PublicKey pub = kf.generatePublic(new X509EncodedKeySpec(pubBytes));
                return new LocalAdbCrypto(new KeyPair(pub, priv));
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to load existing ADB key pair, generating new one: " + e.getMessage());
        }

        try {
            KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
            kpg.initialize(KEY_SIZE_BITS);
            KeyPair kp = kpg.generateKeyPair();

            writeFile(privFile, kp.getPrivate().getEncoded());
            writeFile(pubFile, kp.getPublic().getEncoded());
            return new LocalAdbCrypto(kp);
        } catch (Exception e) {
            Log.e(TAG, "Error generating RSA keypair for ADB auth", e);
            return null;
        }
    }

    public byte[] signToken(byte[] token) {
        try {
            RSAPrivateKey priv = (RSAPrivateKey) keyPair.getPrivate();
            // ADB uses raw RSA signature with PKCS#1 v1.5 padding (digest prefix is None/preformatted)
            // In standard JCE, "NONEwithRSA" or "SHA1withRSA"
            Signature sig = Signature.getInstance("NONEwithRSA");
            sig.initSign(priv);
            sig.update(token);
            return sig.sign();
        } catch (Exception e) {
            try {
                // Fallback if NONEwithRSA is not supported on this provider
                Signature sig = Signature.getInstance("SHA1withRSA");
                sig.initSign(keyPair.getPrivate());
                sig.update(token);
                return sig.sign();
            } catch (Exception ex) {
                Log.e(TAG, "Failed to sign token", ex);
                return null;
            }
        }
    }

    /**
     * Converts the RSA public key to the custom Android adb public key struct:
     * struct RSAPublicKey {
     *     int len;                  // 64 (for 2048-bit)
     *     uint32_t n0inv;           // -1 / N[0] mod 2^32
     *     uint32_t n[64];           // modulus little-endian
     *     uint32_t rr[64];          // R^2 mod N little-endian (R = 2^2048)
     *     int exponent;             // 65537
     * }
     */
    public byte[] getAdbPublicKeyPayload() {
        try {
            RSAPublicKey pub = (RSAPublicKey) keyPair.getPublic();
            BigInteger n = pub.getModulus();
            BigInteger e = pub.getPublicExponent();

            BigInteger b32 = BigInteger.valueOf(2).pow(32);
            BigInteger n0invBig = b32.subtract(n.modInverse(b32));
            int n0inv = n0invBig.intValue();

            BigInteger r = BigInteger.valueOf(2).pow(KEY_SIZE_BITS);
            BigInteger rr = r.multiply(r).mod(n);

            ByteBuffer buf = ByteBuffer.allocate(4 + 4 + 256 + 256 + 4);
            buf.order(ByteOrder.LITTLE_ENDIAN);
            buf.putInt(KEY_SIZE_WORDS);
            buf.putInt(n0inv);

            byte[] nWords = toLittleEndianWords(n, KEY_SIZE_WORDS);
            buf.put(nWords);

            byte[] rrWords = toLittleEndianWords(rr, KEY_SIZE_WORDS);
            buf.put(rrWords);

            buf.putInt(e.intValue());

            byte[] struct = buf.array();
            String b64 = Base64.encodeToString(struct, Base64.NO_WRAP);
            String adbKeyStr = b64 + " root@nexus_adb_watchdog\0";
            return adbKeyStr.getBytes("UTF-8");
        } catch (Exception ex) {
            Log.e(TAG, "Failed to format ADB public key payload", ex);
            return null;
        }
    }

    private static byte[] toLittleEndianWords(BigInteger val, int numWords) {
        byte[] be = val.toByteArray();
        byte[] le = new byte[numWords * 4];

        int beLen = be.length;
        int beStart = 0;
        if (beLen > numWords * 4 && be[0] == 0) {
            beStart = 1;
            beLen--;
        }

        for (int i = 0; i < beLen && i < numWords * 4; i++) {
            le[i] = be[be.length - 1 - i];
        }
        return le;
    }

    private static byte[] readFile(File f) throws Exception {
        FileInputStream fis = new FileInputStream(f);
        byte[] b = new byte[(int) f.length()];
        int read = 0;
        while (read < b.length) {
            int c = fis.read(b, read, b.length - read);
            if (c < 0) break;
            read += c;
        }
        fis.close();
        return b;
    }

    private static void writeFile(File f, byte[] b) throws Exception {
        FileOutputStream fos = new FileOutputStream(f);
        fos.write(b);
        fos.flush();
        fos.close();
    }
}
