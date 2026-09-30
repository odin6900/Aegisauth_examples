# AegisAuth — Kotlin Example

Kotlin JVM client for **AegisAuth** demonstrating:
1. Application initialization & update enforcement
2. Platform HWID generation using `MessageDigest`
3. User login and active subscription checks
4. Requesting ephemeral signed download tokens for **AetherVault**
5. Clean logout

---

## 🚀 Running the Example

```bash
kotlinc AegisAuthExample.kt -include-runtime -d AegisAuthExample.jar
java -jar AegisAuthExample.jar
```

---

## ⚙️ Configuration

Open `AegisAuthExample.kt` and update the constants:

```kotlin
const val AUTH_URL = "https://auth.example.com"
const val APP_KEY = "YOUR_APP_KEY"
const val APP_NAME = "My Application"
const val APP_VERSION = "1.0.0"

const val USERNAME = "YOUR_USERNAME"
const val PASSWORD = "YOUR_PASSWORD"
const val MODULE_NAME = "minecraft"
```
