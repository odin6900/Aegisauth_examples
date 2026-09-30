/**
 * AegisAuth TypeScript Example
 * Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
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

interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

function getHardwareId(): string {
  const seed = `${os.hostname()}-${os.arch()}-${os.platform()}-${os.cpus()[0]?.model || ""}`;
  return crypto.createHash("sha256").update(seed).digest("hex");
}

class AegisAuthClient {
  private baseUrl: string;
  private hwid: string;
  private sessionToken: string | null = null;

  constructor(
    private authUrl: string,
    private appKey: string,
    private appName: string,
    private appVersion: string
  ) {
    this.baseUrl = `${authUrl.replace(/\/+$/, "")}/api/public/v1/`;
    this.hwid = getHardwareId();
  }

  private async post<T>(endpoint: string, payload: Record<string, any>): Promise<ApiResponse<T>> {
    const url = `${this.baseUrl}${endpoint}`;
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
      "x-app-key": this.appKey,
      "x-timestamp": Math.floor(Date.now() / 1000).toString(),
      "User-Agent": `AegisAuth-TypeScript/${this.appVersion}`,
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
      return (await res.json()) as ApiResponse<T>;
    } catch (err: any) {
      return {
        success: false,
        error: { code: "network_error", message: err.message },
      };
    }
  }

  async init(): Promise<boolean> {
    console.log(`[*] Initializing ${this.appName} (v${this.appVersion})...`);
    console.log(`    HWID: ${this.hwid}`);

    const res = await this.post<{ version: { update_required: boolean; current: string } }>("init", {
      version: this.appVersion,
    });

    if (!res.success) {
      console.log(`[-] Init failed: ${res.error?.message}`);
      return false;
    }

    console.log("[+] Handshake successful.");
    return true;
  }

  async login(username: string, pass: string): Promise<boolean> {
    console.log(`\n[*] Logging in as '${username}'...`);

    const res = await this.post<{
      token: string;
      user: { username: string };
      license: { status: string; expires_at: string | null };
    }>("login", {
      username,
      password: pass,
      hwid: this.hwid,
    });

    if (!res.success) {
      console.log(`[-] Login failed: ${res.error?.message}`);
      return false;
    }

    this.sessionToken = res.data!.token;
    console.log(`[+] Logged in! Session token: ${this.sessionToken.slice(0, 12)}...`);
    console.log(`    License: ${res.data!.license?.status ?? "active"}`);
    return true;
  }

  async requestModuleToken(moduleName: string): Promise<string | null> {
    console.log(`\n[*] Requesting signed download token for module '${moduleName}'...`);

    const res = await this.post<{
      key: string;
      download_url: string;
      expires_in_seconds: number;
    }>("module/token", { module: moduleName });

    if (!res.success) {
      console.log(`[-] Module token request failed: ${res.error?.message}`);
      return null;
    }

    console.log("[+] Signed Token Received!");
    console.log(`    Target Asset: ${res.data!.key}`);
    console.log(`    Download URL: ${res.data!.download_url}`);
    console.log(`    Expires In  : ${res.data!.expires_in_seconds}s`);
    return res.data!.download_url;
  }

  async logout(): Promise<void> {
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
