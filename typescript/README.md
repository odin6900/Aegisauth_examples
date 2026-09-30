# AegisAuth — TypeScript Example

Modern, strictly typed TypeScript client for **AegisAuth** demonstrating:
- App initialization & handshake
- HWID generation
- User login & subscription verification
- Ephemeral signed token generation for **AetherVault**
- Clean logout

---

## 🚀 Running the Example

```bash
npm install
npm start
```

Or run directly with `npx tsx`:
```bash
npx tsx index.ts
```

---

## ⚙️ Configuration

Open `index.ts` and set your variables:

```typescript
const AUTH_URL = "https://auth.example.com";
const APP_KEY = "YOUR_APP_KEY";
const APP_NAME = "My Application";
const APP_VERSION = "1.0.0";

const USERNAME = "YOUR_USERNAME";
const PASSWORD = "YOUR_PASSWORD";
const MODULE_NAME = "minecraft";
```
