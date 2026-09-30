// Runnable quickstart for the Aegis Java SDK.
//
//   mvn -q package
//   java -cp target/aegis-sdk-1.0.0.jar examples/Quickstart.java
import io.aegis.sdk.Aegis;
import io.aegis.sdk.AegisException;
import io.aegis.sdk.AegisOptions;

import java.time.Duration;
import java.util.Map;

public class Quickstart {
    public static void main(String[] args) throws Exception {
        String baseUrl = envOr("AEGIS_BASE_URL", "http://localhost:8080");
        String appKey = envOr("AEGIS_APP_KEY", "");

        try (Aegis aegis = new Aegis(new AegisOptions(baseUrl, appKey).version("1.0.0"))) {
            Map<String, Object> info = aegis.init();
            System.out.println("initialized: " + info.get("status"));

            Object versionInfo = info.get("version");
            if (versionInfo instanceof Map<?, ?> version && Boolean.TRUE.equals(version.get("update_required"))) {
                System.out.println("mandatory update: " + version.get("latest"));
                return;
            }

            Map<String, Object> auth = aegis.login(envOr("AEGIS_USERNAME", "demo"), envOr("AEGIS_PASSWORD", "demo-password"));
            System.out.println("signed in as " + ((Map<?, ?>) auth.get("user")).get("username"));

            // Multi-subscription check example:
            if (aegis.hasSubscription("VIP")) {
                System.out.println("[Subscription] VIP tier active — Premium unlocked!");
            } else {
                System.out.println("[Subscription] Standard tier active.");
            }

            System.out.println("user variables: " + aegis.getVariables("user", null).get("variables"));
            aegis.setVariable("last_seen", java.time.Instant.now().toString(), "user", null);

            aegis.startHeartbeat(Duration.ofSeconds(60), reason -> System.out.println("session revoked: " + reason));
            Thread.sleep(2000);

            aegis.logout();
            System.out.println("signed out.");
        } catch (AegisException error) {
            System.err.printf("Aegis error [%s] %s%n", error.getCode(), error.getMessage());
            System.exit(1);
        }
    }

    private static String envOr(String key, String fallback) {
        String value = System.getenv(key);
        return value == null || value.isBlank() ? fallback : value;
    }
}