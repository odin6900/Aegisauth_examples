import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Instant;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class AegisAuthExample {

    // Configuration Placeholders
    private static final String AUTH_URL = "https://auth.example.com";
    private static final String APP_KEY = "YOUR_APP_KEY";
    private static final String APP_NAME = "My Application";
    private static final String APP_VERSION = "1.0.0";

    private static final String USERNAME = "YOUR_USERNAME";
    private static final String PASSWORD = "YOUR_PASSWORD";
    private static final String MODULE_NAME = "minecraft";

    private static final HttpClient client = HttpClient.newHttpClient();
    private static String sessionToken = null;
    private static String hwid = null;

    public static void main(String[] args) throws Exception {
        System.out.println("=== AegisAuth Java Client [" + APP_NAME + " v" + APP_VERSION + "] ===");
        hwid = getHardwareId();
        System.out.println("[*] HWID: " + hwid);

        if (!init()) return;
        if (!login(USERNAME, PASSWORD)) return;

        String downloadUrl = requestModuleToken(MODULE_NAME);
        if (downloadUrl != null) {
            System.out.println("\n[+] Binary payload authorized for AetherVault silent streaming!");
        }

        logout();
    }

    private static String getHardwareId() {
        try {
            String seed = System.getProperty("os.name") + "-" +
                          System.getProperty("os.arch") + "-" +
                          System.getProperty("user.name") + "-" +
                          Runtime.getRuntime().availableProcessors();
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] hash = md.digest(seed.getBytes(StandardCharsets.UTF_8));
            StringBuilder sb = new StringBuilder();
            for (byte b : hash) {
                sb.append(String.format("%02x", b));
            }
            return sb.toString();
        } catch (Exception e) {
            return "fallback-hwid-java";
        }
    }

    private static String post(String endpoint, String jsonPayload) throws Exception {
        String url = AUTH_URL.replaceAll("/+$", "") + "/api/public/v1/" + endpoint;
        HttpRequest.Builder builder = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .header("Content-Type", "application/json")
                .header("x-app-key", APP_KEY)
                .header("x-timestamp", String.valueOf(Instant.now().getEpochSecond()))
                .header("User-Agent", "AegisAuth-Java/" + APP_VERSION)
                .POST(HttpRequest.BodyPublishers.ofString(jsonPayload));

        if (sessionToken != null) {
            builder.header("x-session-token", sessionToken);
        }

        HttpResponse<String> response = client.send(builder.build(), HttpResponse.BodyHandlers.ofString());
        return response.body();
    }

    private static String extractJson(String json, String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return matcher.group(1);
        }
        return null;
    }

    private static boolean init() throws Exception {
        System.out.println("\n[*] Initializing application...");
        String res = post("init", "{\"version\":\"" + APP_VERSION + "\"}");

        if (!res.contains("\"success\":true")) {
            System.out.println("[-] Init failed. Response: " + res);
            return false;
        }

        System.out.println("[+] Application initialized successfully.");
        return true;
    }

    private static boolean login(String user, String pass) throws Exception {
        System.out.println("\n[*] Logging in as '" + user + "'...");
        String payload = String.format("{\"username\":\"%s\",\"password\":\"%s\",\"hwid\":\"%s\"}", user, pass, hwid);
        String res = post("login", payload);

        if (!res.contains("\"success\":true")) {
            String err = extractJson(res, "message");
            System.out.println("[-] Login failed: " + (err != null ? err : res));
            return false;
        }

        sessionToken = extractJson(res, "token");
        String preview = (sessionToken != null && sessionToken.length() > 12) ? sessionToken.substring(0, 12) + "..." : sessionToken;
        System.out.println("[+] Login successful! Session token: " + preview);
        return true;
    }

    private static String requestModuleToken(String module) throws Exception {
        System.out.println("\n[*] Requesting signed download token for module '" + module + "'...");
        String payload = String.format("{\"module\":\"%s\"}", module);
        String res = post("module/token", payload);

        if (!res.contains("\"success\":true")) {
            String err = extractJson(res, "message");
            System.out.println("[-] Module token request failed: " + (err != null ? err : res));
            return null;
        }

        String key = extractJson(res, "key");
        String downloadUrl = extractJson(res, "download_url");

        System.out.println("[+] Signed Token Received!");
        System.out.println("    Target Asset: " + key);
        System.out.println("    Download URL: " + downloadUrl);
        return downloadUrl;
    }

    private static void logout() throws Exception {
        if (sessionToken != null) {
            System.out.println("\n[*] Logging out...");
            post("logout", "{}");
            sessionToken = null;
            System.out.println("[+] Logged out cleanly.");
        }
    }
}
