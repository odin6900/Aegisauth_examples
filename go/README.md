# AegisAuth — Go Example

Standard library Go client for **AegisAuth** demonstrating:
1. Application initialization & update enforcement
2. Machine-specific HWID generation
3. User login and active subscription checks
4. Requesting ephemeral signed download tokens for **AetherVault**
5. Session termination

Zero external dependencies (`net/http`, `crypto/sha256`, `encoding/json`).

---

## 🚀 Running the Example

```bash
go run main.go
```

---

## ⚙️ Configuration

Open `main.go` and replace the placeholder constants:

```go
const (
    AuthURL    = "https://auth.example.com"
    AppKey     = "YOUR_APP_KEY"
    AppName    = "My Application"
    AppVersion = "1.0.0"

    Username   = "YOUR_USERNAME"
    Password   = "YOUR_PASSWORD"
    ModuleName = "minecraft"
)
```
