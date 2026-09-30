# AegisAuth — Dart Example

Dart client for **AegisAuth** demonstrating:
1. Application initialization & update enforcement
2. Platform HWID generation using `crypto`
3. User login and active subscription checks
4. Requesting ephemeral signed download tokens for **AetherVault**
5. Session termination

---

## 🚀 Running the Example

```bash
dart pub get
dart run bin/main.dart
```

---

## ⚙️ Configuration

Open `bin/main.dart` and update the constants:

```dart
const String authUrl = "https://auth.example.com";
const String appKey = "YOUR_APP_KEY";
const String appName = "My Application";
const String appVersion = "1.0.0";

const String username = "YOUR_USERNAME";
const String password = "YOUR_PASSWORD";
const String moduleName = "minecraft";
```
