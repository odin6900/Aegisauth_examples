package io.aegis.sdk;

import java.net.NetworkInterface;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Collections;

/** Stable, non-reversible machine identifier. */
public final class HardwareId {
    private static volatile String cached;

    private HardwareId() {}

    public static String get() {
        if (cached != null) return cached;
        String seed = String.join("|",
                System.getProperty("os.name", ""),
                System.getProperty("os.arch", ""),
                System.getProperty("user.name", ""),
                macAddress());
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(seed.getBytes(StandardCharsets.UTF_8));
            StringBuilder out = new StringBuilder(digest.length * 2);
            for (byte b : digest) out.append(String.format("%02x", b));
            cached = out.toString();
        } catch (Exception exception) {
            cached = Integer.toHexString(seed.hashCode());
        }
        return cached;
    }

    private static String macAddress() {
        try {
            for (NetworkInterface iface : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                byte[] mac = iface.getHardwareAddress();
                if (iface.isLoopback() || mac == null || mac.length == 0) continue;
                StringBuilder out = new StringBuilder();
                for (byte b : mac) out.append(String.format("%02x", b));
                return out.toString();
            }
        } catch (Exception ignored) {
            // fall through
        }
        return "";
    }
}