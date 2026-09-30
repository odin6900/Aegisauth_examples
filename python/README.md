# AegisAuth — Python Example

Complete, self-contained Python client for **AegisAuth** demonstrating:
1. Application initialization and version enforcement (`POST /api/public/v1/init`)
2. HWID generation using cryptographic platform identification
3. User login and subscription validation (`POST /api/public/v1/login`)
4. Signed download token generation for **AetherVault** (`POST /api/public/v1/module/token`)
5. Session termination (`POST /api/public/v1/logout`)

Zero external dependencies (uses standard library `urllib` and `hashlib`).

---

## 🚀 Running the Example

```bash
python aegis_auth_example.py
```

---

## ⚙️ Configuration

Open `aegis_auth_example.py` and replace the placeholder constants:

```python
AUTH_URL = "https://auth.example.com"    # AegisAuth server URL
APP_KEY = "YOUR_APP_KEY"                # Application Key from the dashboard
APP_NAME = "My Application"
APP_VERSION = "1.0.0"

USERNAME = "YOUR_USERNAME"
PASSWORD = "YOUR_PASSWORD"
MODULE_NAME = "minecraft"              # Game module / target asset
```
