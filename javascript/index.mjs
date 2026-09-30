/**
 * AegisAuth JavaScript Example (ESM)
 * Zero external dependencies (uses Node.js 18+ native fetch and crypto).
 */

import * as crypto from "node:crypto";
import * as os from "node:os";

// Configuration Placeholders
const AUTH_URL = "https://auth.example.com";
const APP_KEY = "YOUR_APP_KEY";
const APP_NAME = "My Application";
const APP_VERSION = "1.0.0";

const USERNAME = "YOUR_USERNAME";
const PASSWORD = "YOUR_PASSWORD";
const MODULE_NAME = "minecraft";

function getHardwareId() {
  const seed = `${os.hostname()}-${os.arch()}-${os.platform()}-${os.cpus()[0]?.model || ""}`;
  return crypto.createHash("sha256").update(seed).digest("hex");
}

class AegisAuthClient {
  constructor(authUrl, appKey, appName, appVersion) {
    this.baseUrl = `${authUrl.replace(/\/+$/, "")}/api/public/v1/`;
    this.appKey = appKey;
    this.appName = appName;
    this.appVersion = appVersion;
    this.hwid = getHardwareId();
    this.sessionToken = null;
  }

  async post(endpoint, payload) {
    const url = `${this.baseUrl}${endpoint}`;
    const headers = {
      "Content-Type": "application/json",
      "x-app-key": this.appKey,
      "x-timestamp": Math.floor(Date.now() / 1000).toString(),
      "User-Agent": `AegisAuth-JavaScript/${this.appVersion}`,
    };

    if (this.sessionToken) {
      headers["x-session-token"] = this.sessionToken;
    }

    try {
      const res = await fetch(url, {
        method: "POST",
        headers,
        body: JSON.stringify(payload),
      });
      return await res.json();
    } catch (err) {
      return { success: false, error: { message: err.message } };
    }
  }

  async init() {
    console.log(`[*] Initializing ${this.appName} (v${this.appVersion})...`);
    console.log(`    HWID: ${this.hwid}`);

    const res = await this.post("init", { version: this.appVersion });
    if (!res.success) {
      console.log(`[-] Init failed: ${res.error?.message}`);
      return false;
    }

    console.log("[+] Handshake successful.");
    return true;
  }

  async login(username, password) {
    console.log(`\n[*] Logging in as '${username}'...`);

    const res = await this.post("login", {
      username,
      password,
      hwid: this.hwid,
    });

    if (!res.success) {
      console.log(`[-] Login failed: ${res.error?.message}`);
      return false;
    }

    this.sessionToken = res.data.token;
    console.log(`[+] Logged in! Session token: ${this.sessionToken.slice(0, 12)}...`);
    console.log(`    License: ${res.data.license?.status || "active"}`);
    return true;
  }

  async requestModuleToken(moduleName) {
    console.log(`\n[*] Requesting signed download token for module '${moduleName}'...`);

    const res = await this.post("module/token", { module: moduleName });
    if (!res.success) {
      console.log(`[-] Module token request failed: ${res.error?.message}`);
      return null;
    }

    console.log("[+] Signed Token Received!");
    console.log(`    Target Asset: ${res.data.key}`);
    console.log(`    Download URL: ${res.data.download_url}`);
    console.log(`    Expires In  : ${res.data.expires_in_seconds}s`);
    return res.data.download_url;
  }

  async logout() {
    if (this.sessionToken) {
      console.log("\n[*] Logging out...");
      await this.post("logout", {});
      this.sessionToken = null;
      console.log("[+] Logged out cleanly.");
    }
  }
}

async function main() {
  const client = new AegisAuthClient(AUTH_URL, APP_KEY, APP_NAME, APP_VERSION);

  if (!(await client.init())) return;
  if (!(await client.login(USERNAME, PASSWORD))) return;

  const downloadUrl = await client.requestModuleToken(MODULE_NAME);
  if (downloadUrl) {
    console.log("\n[+] Binary payload authorized for AetherVault silent streaming!");
  }

  await client.logout();
}

main().catch(console.error);
