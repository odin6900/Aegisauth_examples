package io.aegis.sdk;

import java.time.Duration;

/** Configuration for {@link Aegis}. */
public class AegisOptions {
    public String baseUrl;
    public String appKey;
    public String apiKey;
    public String version = "1.0.0";
    public String channel = "stable";
    public String hwid;
    public Duration timeout = Duration.ofSeconds(20);
    public int maxRetries = 2;

    public AegisOptions(String baseUrl, String appKey) {
        this.baseUrl = baseUrl;
        this.appKey = appKey;
    }

    public AegisOptions version(String value) { this.version = value; return this; }
    public AegisOptions channel(String value) { this.channel = value; return this; }
    public AegisOptions apiKey(String value) { this.apiKey = value; return this; }
    public AegisOptions hwid(String value) { this.hwid = value; return this; }
    public AegisOptions timeout(Duration value) { this.timeout = value; return this; }
    public AegisOptions maxRetries(int value) { this.maxRetries = value; return this; }
}