import { AegisError } from "./errors";
import { hardwareId } from "./hwid";
import type {
  AegisAuthResult,
  AegisEnvelope,
  AegisInitResult,
  AegisLicense,
  AegisLicenseResult,
  AegisOptions,
  AegisSessionCheck,
  AegisSubscription,
  AegisUser,
  AegisVariables,
  AegisVersionInfo,
} from "./types";

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

/** Client for the Aegis Authentication API. */
export class Aegis {
  readonly baseUrl: string;
  readonly appKey: string;
  readonly version: string;
  readonly channel: string;
  readonly hwid: string;

  private readonly apiKey: string | undefined;
  private readonly timeoutMs: number;
  private readonly maxRetries: number;
  private readonly fetchImpl: typeof fetch;
  private sessionToken: string | null = null;
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null;
  private currentUser: AegisUser | null = null;
  private currentLicense: AegisLicense | null = null;

  constructor(options: AegisOptions) {
    if (!options.baseUrl) throw new AegisError("invalid_options", "baseUrl is required.");
    if (!options.appKey) throw new AegisError("invalid_options", "appKey is required.");

    this.baseUrl = options.baseUrl.replace(/\/+$/, "");
    this.appKey = options.appKey;
    this.apiKey = options.apiKey;
    this.version = options.version ?? "1.0.0";
    this.channel = options.channel ?? "stable";
    this.hwid = options.hwid ?? hardwareId();
    this.timeoutMs = options.timeoutMs ?? 20_000;
    this.maxRetries = options.maxRetries ?? 2;
    this.fetchImpl = options.fetch ?? globalThis.fetch.bind(globalThis);
  }

  /* ---------------- transport ---------------- */

  /** Calls any endpoint and returns its `data` payload. */
  async request<T = unknown>(endpoint: string, body: Record<string, unknown> = {}): Promise<T> {
    const url = `${this.baseUrl}/api/public/v1/${endpoint.replace(/^\/+|\/+$/g, "")}`;
    const headers: Record<string, string> = {
      "content-type": "application/json",
      "x-app-key": this.appKey,
      "x-timestamp": String(Math.floor(Date.now() / 1000)),
    };
    if (this.apiKey) headers["x-api-key"] = this.apiKey;
    if (this.sessionToken) headers["x-session-token"] = this.sessionToken;

    let lastError: unknown = null;

    for (let attempt = 0; attempt <= this.maxRetries; attempt += 1) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), this.timeoutMs);
      try {
        const response = await this.fetchImpl(url, {
          method: "POST",
          headers,
          body: JSON.stringify(body),
          signal: controller.signal,
        });
        const text = await response.text();

        if (response.status >= 500 && attempt < this.maxRetries) {
          await sleep(250 * (attempt + 1));
          continue;
        }

        let envelope: AegisEnvelope<T>;
        try {
          envelope = JSON.parse(text || "{}") as AegisEnvelope<T>;
        } catch (cause) {
          throw new AegisError("invalid_response", "Malformed API response.", response.status, {
            cause,
          });
        }

        if (!envelope.success) {
          throw new AegisError(
            envelope.error?.code ?? "server_error",
            envelope.error?.message ?? "Request failed.",
            response.status,
          );
        }

        return (envelope.data ?? {}) as T;
      } catch (error) {
        if (error instanceof AegisError) throw error;
        lastError = error;
        if (attempt >= this.maxRetries) break;
        await sleep(250 * (attempt + 1));
      } finally {
        clearTimeout(timer);
      }
    }

    throw new AegisError(
      "network_error",
      lastError instanceof Error ? lastError.message : "Network request failed.",
      0,
      { cause: lastError },
    );
  }

  /* ---------------- application ---------------- */

  /** Handshake. Call once before any other operation. */
  init(): Promise<AegisInitResult> {
    return this.request<AegisInitResult>("init", { version: this.version });
  }

  status(): Promise<Record<string, unknown>> {
    return this.request("status");
  }

  appData(): Promise<Record<string, unknown>> {
    return this.request("app/data");
  }

  checkVersion(version = this.version): Promise<AegisVersionInfo> {
    return this.request<AegisVersionInfo>("version/check", { version, channel: this.channel });
  }

  downloads(version = this.version): Promise<{ downloads: unknown[] }> {
    return this.request("downloads", { version });
  }

  /* ---------------- authentication ---------------- */

  async register(input: {
    username: string;
    password: string;
    email?: string;
    licenseKey?: string;
  }): Promise<AegisAuthResult> {
    const result = await this.request<AegisAuthResult>("register", {
      username: input.username,
      password: input.password,
      email: input.email,
      license_key: input.licenseKey,
      hwid: this.hwid,
    });
    if (result.session?.token) this.sessionToken = result.session.token;
    if (result.user) this.currentUser = result.user;
    if (result.license) this.currentLicense = result.license;
    return result;
  }

  async login(input: { username: string; password: string }): Promise<AegisAuthResult> {
    const result = await this.request<AegisAuthResult>("login", {
      username: input.username,
      password: input.password,
      hwid: this.hwid,
    });
    if (result.session?.token) this.sessionToken = result.session.token;
    if (result.user) this.currentUser = result.user;
    if (result.license) this.currentLicense = result.license;
    return result;
  }

  async logout(): Promise<void> {
    this.stopHeartbeat();
    try {
      await this.request("logout");
    } finally {
      this.sessionToken = null;
      this.currentUser = null;
      this.currentLicense = null;
    }
  }

  async heartbeat(): Promise<AegisSessionCheck> {
    const result = await this.request<AegisSessionCheck>("heartbeat");
    if (result.user) this.currentUser = result.user;
    return result;
  }

  async checkSession(): Promise<AegisSessionCheck> {
    const result = await this.request<AegisSessionCheck>("session/check");
    if (result.user) this.currentUser = result.user;
    if (result.license) this.currentLicense = result.license;
    return result;
  }

  /** True when a token exists and the server still accepts it. */
  async isAuthenticated(): Promise<boolean> {
    if (!this.sessionToken) return false;
    try {
      return Boolean((await this.checkSession()).valid);
    } catch {
      return false;
    }
  }

  /** Restores a session token persisted by the host application. */
  useSession(token: string | null): void {
    this.sessionToken = token;
  }

  get session(): string | null {
    return this.sessionToken;
  }

  get user(): AegisUser | null {
    return this.currentUser;
  }

  get license(): AegisLicense | null {
    return this.currentLicense;
  }

  /** Returns true if the user or license holds an active subscription by name. */
  hasSubscription(name: string): boolean {
    if (!name) return false;
    const target = name.toLowerCase();
    const subs: AegisSubscription[] = [
      ...(this.currentUser?.subscriptions ?? []),
      ...(this.currentLicense?.subscriptions ?? []),
    ];
    const now = Date.now();
    return subs.some((sub) => {
      if (sub.name.toLowerCase() !== target) return false;
      if (sub.status && sub.status.toLowerCase() !== "active") return false;
      if (sub.expires_at && new Date(sub.expires_at).getTime() <= now) return false;
      return true;
    });
  }

  /** Returns all active subscriptions for the current user and license. */
  getSubscriptions(): AegisSubscription[] {
    return [
      ...(this.currentUser?.subscriptions ?? []),
      ...(this.currentLicense?.subscriptions ?? []),
    ];
  }

  userData(): Promise<Record<string, unknown>> {
    return this.request("user/data");
  }

  /* ---------------- licensing ---------------- */

  validateLicense(licenseKey: string): Promise<AegisLicenseResult> {
    return this.request<AegisLicenseResult>("license/validate", {
      license_key: licenseKey,
      hwid: this.hwid,
    });
  }

  async activateLicense(licenseKey: string, username?: string): Promise<AegisLicenseResult> {
    const result = await this.request<AegisLicenseResult>("license/activate", {
      license_key: licenseKey,
      hwid: this.hwid,
      username,
    });
    if (result.license) this.currentLicense = result.license;
    return result;
  }

  /* ---------------- variables ---------------- */

  getVariables(
    scope: "application" | "user" | "license" = "application",
    licenseKey?: string,
  ): Promise<AegisVariables> {
    return this.request<AegisVariables>("variables/get", { scope, license_key: licenseKey });
  }

  setVariable(
    key: string,
    value: string,
    scope: "application" | "user" | "license" = "user",
    licenseKey?: string,
  ): Promise<{ saved: boolean }> {
    return this.request("variables/set", { scope, key, value, license_key: licenseKey });
  }

  /* ---------------- webhooks ---------------- */

  triggerWebhook(
    event: string,
    payload: Record<string, unknown> = {},
  ): Promise<{ dispatched: boolean }> {
    return this.request("webhook/trigger", { event, payload });
  }

  /* ---------------- heartbeat loop ---------------- */

  /** Starts a heartbeat loop. Returns a stop function. */
  startHeartbeat(
    options: {
      intervalMs?: number;
      onRevoked?: (reason: string) => void;
      onBeat?: (beat: AegisSessionCheck) => void;
    } = {},
  ) {
    this.stopHeartbeat();
    const intervalMs = options.intervalMs ?? 60_000;
    this.heartbeatTimer = setInterval(async () => {
      try {
        options.onBeat?.(await this.heartbeat());
      } catch (error) {
        if (error instanceof AegisError && error.isNetworkError) return;
        this.stopHeartbeat();
        this.sessionToken = null;
        options.onRevoked?.(error instanceof Error ? error.message : "Session ended.");
      }
    }, intervalMs);
    return () => this.stopHeartbeat();
  }

  stopHeartbeat(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }
}
