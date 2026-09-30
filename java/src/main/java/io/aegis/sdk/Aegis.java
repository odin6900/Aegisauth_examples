package io.aegis.sdk;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Client for the Aegis Authentication API. Dependency-free (JDK 17+). */
public class Aegis implements AutoCloseable {
    private final String baseUrl;
    private final String appKey;
    private final String apiKey;
    private final String version;
    private final String channel;
    private final String hwid;
    private final int maxRetries;
    private final HttpClient http;
    private final Duration timeout;

    private volatile String sessionToken;
    private volatile Map<String, Object> currentUser;
    private volatile Map<String, Object> currentLicense;
    private ScheduledExecutorService heartbeat;

    public Aegis(AegisOptions options) {
        if (options.baseUrl == null || options.baseUrl.isBlank())
            throw new AegisException("invalid_options", "baseUrl is required.", 0);
        if (options.appKey == null || options.appKey.isBlank())
            throw new AegisException("invalid_options", "appKey is required.", 0);

        this.baseUrl = options.baseUrl.replaceAll("/+$", "");
        this.appKey = options.appKey;
        this.apiKey = options.apiKey;
        this.version = options.version;
        this.channel = options.channel;
        this.hwid = options.hwid != null ? options.hwid : HardwareId.get();
        this.maxRetries = options.maxRetries;
        this.timeout = options.timeout;
        this.http = HttpClient.newBuilder().connectTimeout(options.timeout).build();
    }

    public String hwid() { return hwid; }

    public String sessionToken() { return sessionToken; }

    /** Restores a session token persisted by the host application. */
    public void useSession(String token) { this.sessionToken = token; }

    // ---------------- transport ----------------

    /** Calls any endpoint and returns its {@code data} payload. */
    public Map<String, Object> request(String endpoint, Map<String, Object> body) {
        String url = baseUrl + "/api/public/v1/" + endpoint.replaceAll("^/+|/+$", "");
        String payload = Json.write(body == null ? Map.of() : body);

        HttpRequest.Builder builder = HttpRequest.newBuilder(URI.create(url))
                .timeout(timeout)
                .header("content-type", "application/json")
                .header("x-app-key", appKey)
                .header("x-timestamp", String.valueOf(System.currentTimeMillis() / 1000))
                .header("user-agent", "aegis-java-sdk/1.0.0")
                .POST(HttpRequest.BodyPublishers.ofString(payload));
        if (apiKey != null) builder.header("x-api-key", apiKey);
        String token = sessionToken;
        if (token != null) builder.header("x-session-token", token);
        HttpRequest request = builder.build();

        Exception lastError = null;
        for (int attempt = 0; attempt <= maxRetries; attempt++) {
            try {
                HttpResponse<String> response = http.send(request, HttpResponse.BodyHandlers.ofString());
                if (response.statusCode() >= 500 && attempt < maxRetries) {
                    sleep(250L * (attempt + 1));
                    continue;
                }
                Map<String, Object> envelope = Json.parseObject(response.body());
                if (!Boolean.TRUE.equals(envelope.get("success"))) {
                    Object error = envelope.get("error");
                    String code = "server_error";
                    String message = "Request failed.";
                    if (error instanceof Map<?, ?> map) {
                        if (map.get("code") != null) code = String.valueOf(map.get("code"));
                        if (map.get("message") != null) message = String.valueOf(map.get("message"));
                    }
                    throw new AegisException(code, message, response.statusCode());
                }
                Object data = envelope.get("data");
                @SuppressWarnings("unchecked")
                Map<String, Object> result = data instanceof Map ? (Map<String, Object>) data : new LinkedHashMap<>();
                return result;
            } catch (AegisException exception) {
                throw exception;
            } catch (Exception exception) {
                lastError = exception;
                if (attempt < maxRetries) sleep(250L * (attempt + 1));
            }
        }
        throw new AegisException("network_error",
                lastError != null ? String.valueOf(lastError.getMessage()) : "Network request failed.", 0, lastError);
    }

    // ---------------- application ----------------

    /** Handshake. Call once before any other operation. */
    public Map<String, Object> init() {
        return request("init", Map.of("version", version));
    }

    public Map<String, Object> status() { return request("status", Map.of()); }

    public Map<String, Object> appData() { return request("app/data", Map.of()); }

    public Map<String, Object> checkVersion(String buildVersion) {
        return request("version/check", Map.of("version", buildVersion == null ? version : buildVersion, "channel", channel));
    }

    // ---------------- authentication ----------------

    public Map<String, Object> register(String username, String password, String email, String licenseKey) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("username", username);
        body.put("password", password);
        body.put("email", email);
        body.put("license_key", licenseKey);
        body.put("hwid", hwid);
        Map<String, Object> result = request("register", body);
        storeSession(result);
        return result;
    }

    public Map<String, Object> login(String username, String password) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("username", username);
        body.put("password", password);
        body.put("hwid", hwid);
        Map<String, Object> result = request("login", body);
        storeSession(result);
        return result;
    }

    public void logout() {
        stopHeartbeat();
        try {
            request("logout", Map.of());
        } finally {
            sessionToken = null;
            currentUser = null;
            currentLicense = null;
        }
    }

    public Map<String, Object> heartbeat() {
        Map<String, Object> res = request("heartbeat", Map.of());
        if (res.get("user") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> u = (Map<String, Object>) map;
            currentUser = u;
        }
        return res;
    }

    public Map<String, Object> checkSession() {
        Map<String, Object> res = request("session/check", Map.of());
        if (res.get("user") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> u = (Map<String, Object>) map;
            currentUser = u;
        }
        if (res.get("license") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> lic = (Map<String, Object>) map;
            currentLicense = lic;
        }
        return res;
    }

    /** True when a token exists and the server still accepts it. */
    public boolean isAuthenticated() {
        if (sessionToken == null) return false;
        try {
            return Boolean.TRUE.equals(checkSession().get("valid"));
        } catch (AegisException exception) {
            return false;
        }
    }

    public Map<String, Object> user() { return currentUser; }

    public Map<String, Object> license() { return currentLicense; }

    /** Returns true if the active user or license holds a specific active subscription (case-insensitive). */
    public boolean hasSubscription(String name) {
        if (name == null || name.isBlank()) return false;
        String target = name.trim().toLowerCase();
        java.util.List<Object> allSubs = new java.util.ArrayList<>();
        if (currentUser != null && currentUser.get("subscriptions") instanceof java.util.List<?> list) {
            allSubs.addAll(list);
        }
        if (currentLicense != null && currentLicense.get("subscriptions") instanceof java.util.List<?> list) {
            allSubs.addAll(list);
        }
        for (Object item : allSubs) {
            if (item instanceof Map<?, ?> map) {
                String subName = String.valueOf(map.get("name"));
                if (subName.equalsIgnoreCase(target)) {
                    Object status = map.get("status");
                    if (status != null && !"active".equalsIgnoreCase(String.valueOf(status))) {
                        continue;
                    }
                    return true;
                }
            }
        }
        return false;
    }

    public Map<String, Object> userData() { return request("user/data", Map.of()); }

    // ---------------- licensing ----------------

    public Map<String, Object> validateLicense(String licenseKey) {
        return request("license/validate", Map.of("license_key", licenseKey, "hwid", hwid));
    }

    public Map<String, Object> activateLicense(String licenseKey, String username) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("license_key", licenseKey);
        body.put("hwid", hwid);
        body.put("username", username);
        Map<String, Object> res = request("license/activate", body);
        if (res.get("license") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> lic = (Map<String, Object>) map;
            currentLicense = lic;
        }
        return res;
    }

    // ---------------- variables ----------------

    public Map<String, Object> getVariables(String scope, String licenseKey) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("scope", scope == null ? "application" : scope);
        body.put("license_key", licenseKey);
        return request("variables/get", body);
    }

    public Map<String, Object> setVariable(String key, String value, String scope, String licenseKey) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("scope", scope == null ? "user" : scope);
        body.put("key", key);
        body.put("value", value);
        body.put("license_key", licenseKey);
        return request("variables/set", body);
    }

    public Map<String, Object> triggerWebhook(String event, Map<String, Object> payload) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("event", event);
        body.put("payload", payload == null ? Map.of() : payload);
        return request("webhook/trigger", body);
    }

    // ---------------- heartbeat loop ----------------

    /** Starts a background heartbeat until the session is revoked or stopped. */
    public synchronized void startHeartbeat(Duration interval, Consumer<String> onRevoked) {
        stopHeartbeat();
        long seconds = interval == null ? 60 : Math.max(5, interval.getSeconds());
        heartbeat = Executors.newSingleThreadScheduledExecutor(runnable -> {
            Thread thread = new Thread(runnable, "aegis-heartbeat");
            thread.setDaemon(true);
            return thread;
        });
        heartbeat.scheduleAtFixedRate(() -> {
            try {
                heartbeat();
            } catch (AegisException exception) {
                if (exception.isNetworkError()) return;
                sessionToken = null;
                if (onRevoked != null) onRevoked.accept(exception.getMessage());
                stopHeartbeat();
            }
        }, seconds, seconds, TimeUnit.SECONDS);
    }

    public synchronized void stopHeartbeat() {
        if (heartbeat != null) {
            heartbeat.shutdownNow();
            heartbeat = null;
        }
    }

    @Override
    public void close() {
        stopHeartbeat();
    }

    private void storeSession(Map<String, Object> result) {
        Object session = result.get("session");
        if (session instanceof Map<?, ?> map && map.get("token") != null) {
            sessionToken = String.valueOf(map.get("token"));
        }
        if (result.get("user") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> u = (Map<String, Object>) map;
            currentUser = u;
        }
        if (result.get("license") instanceof Map<?, ?> map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> lic = (Map<String, Object>) map;
            currentLicense = lic;
        }
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
        }
    }
}