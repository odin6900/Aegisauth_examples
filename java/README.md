# AegisAuth — Java Example

Java 11+ client for **AegisAuth** demonstrating:
1. Application initialization & update enforcement
2. Platform HWID generation via `MessageDigest`
3. User login and active subscription checks
4. Requesting ephemeral signed download tokens for **AetherVault**
5. Session termination

Uses built-in `java.net.http.HttpClient` (zero external dependencies).

---

## 🚀 Running the Example

```bash
javac AegisAuthExample.java
java AegisAuthExample
```

---

## ⚙️ Configuration

Open `AegisAuthExample.java` and replace the placeholder constants:

```java
private static final String AUTH_URL = "https://auth.example.com";
private static final String APP_KEY = "YOUR_APP_KEY";
private static final String APP_NAME = "My Application";
private static final String APP_VERSION = "1.0.0";

private static final String USERNAME = "YOUR_USERNAME";
private static final String PASSWORD = "YOUR_PASSWORD";
private static final String MODULE_NAME = "minecraft";
```
