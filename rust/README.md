# AegisAuth — Rust Example

Async Rust client for **AegisAuth** using `tokio`, `reqwest`, and `serde`.

---

## 🚀 Running the Example

```bash
cargo run
```

---

## ⚙️ Configuration

Open `src/main.rs` and update the constants:

```rust
const AUTH_URL: &str = "https://auth.example.com";
const APP_KEY: &str = "YOUR_APP_KEY";
const APP_NAME: &str = "My Application";
const APP_VERSION: &str = "1.0.0";

const USERNAME: &str = "YOUR_USERNAME";
const PASSWORD: &str = "YOUR_PASSWORD";
const MODULE_NAME: &str = "minecraft";
```
