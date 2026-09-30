/**
 * Aegis Authentication API client — plain JavaScript (ESM).
 *
 * Runs unmodified in browsers, Node 18+, Deno, Bun and workers. Uses the
 * global `fetch`; no build step and no dependencies.
 */

export const SDK_VERSION = "1.0.0";

/** Every Aegis API failure surfaces as this error. */
export class AegisError extends Error {
  constructor(code, message, status = 0) {
    super(message);
    this.name = "AegisError";
    this.code = code;
    this.status = status;
  }

  /** True when the request never reached the Aegis API. */
  get isNetworkError() {
    return this.status === 0;
  }

  /** True for credential and session failures. */
  get isAuthError() {
    return this.code === "unauthorized" || this.code === "invalid_credentials";
  }

  /** True for licensing and hardware-binding failures. */
  get isLicenseError() {
    return this.code.startsWith("license") || this.code === "hwid_mismatch";
  }
}

/**
 * Stable, non-reversible machine/browser identifier.
 * Derived from environment facts and hashed with SHA-256.
 */
export async function hardwareId() {
  const facts = [];
  if (typeof navigator !== "undefined") {
    facts.push(
      navigator.userAgent,
      navigator.language,
      String(navigator.hardwareConcurrency ?? ""),
    );
  }
  if (typeof screen !== "undefined") {
    facts.push(`${screen.width}x${screen.height}x${screen.colorDepth}`);
  }
  if (typeof process !== "undefined" && process.versions?.node) {
    facts.push(
      process.platform,
      process.arch,
      String(process.env.USER ?? process.env.USERNAME ?? ""),
    );
  }
  facts.push(new Date().getTimezoneOffset().toString());

  const bytes = new TextEncoder().encode(facts.join("|"));
  if (globalThis.crypto?.subtle) {
    const digest = await globalThis.crypto.subtle.digest("SHA-256", bytes);
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
  }

  let hash = 2166136261;
  for (const byte of bytes) {
    hash = Math.imul(hash ^ byte, 16777619) >>> 0;
  }
  let out = "";
  let seed = hash;
  while (out.length < 64) {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    out += seed.toString(16).padStart(8, "0");
  }
  return out.slice(0, 64);
}

/**
 * Client for the Aegis Authentication API.
 *
 * @example
 * const aegis = new Aegis({ baseUrl: 'https://your-aegis-host', appKey: APP_KEY });
 * await aegis.init();
 * await aegis.login('ada', password);
 */
export class Aegis {
  #options;
  #baseUrl;
  #hwid = null;
  #heartbeatTimer = null;

  /** Current session token, or null when signed out. */
  sessionToken = null;
  currentUser = null;
  currentLicense = null;

  get user() {
    return this.currentUser;
  }

  get license() {
    return this.currentLicense;
  }

  /** Returns true if the user or license holds an active subscription by name. */
  hasSubscription(name) {
    if (!name) return false;
    const target = String(name).toLowerCase();
    const subs = [
      ...(this.currentUser?.subscriptions ?? []),
      ...(this.currentLicense?.subscriptions ?? []),
    ];
    const now = Date.now();
    return subs.some((sub) => {
      if (String(sub.name ?? "").toLowerCase() !== target) return false;
      if (sub.status && String(sub.status).toLowerCase() !== "active") return false;
      if (sub.expires_at && new Date(sub.expires_at).getTime() <= now) return false;
      return true;
    });
  }

  /** Returns all active subscriptions for the current user and license. */
  getSubscriptions() {
    return [
      ...(this.currentUser?.subscriptions ?? []),
      ...(this.currentLicense?.subscriptions ?? []),
    ];
  }

  constructor(options) {
    if (!options?.baseUrl) throw new AegisError("invalid_options", "baseUrl is required.");
    if (!options?.appKey) throw new AegisError("invalid_options", "appKey is required.");

    this.#options = {
      version: "1.0.0",
      channel: "stable",
      timeout: 20000,
      maxRetries: 2,
      ...options,
    };
    this.#baseUrl = options.baseUrl.replace(/\/+$/, "");
    this.#hwid = options.hwid ?? null;
  }

  /** Machine identifier sent with authentication and licensing calls. */
  async hardwareId() {
    this.#hwid ??= await hardwareId();
    return this.#hwid;
  }

  /** Restores a session token persisted by the host application. */
  useSession(token) {
    this.sessionToken = token ?? null;
  }

  /** Calls any endpoint and returns its `data` payload. */
  async request(endpoint, body = {}) {
    const url = `${this.#baseUrl}/api/public/v1/${endpoint.replace(/^\/+|\/+$/g, "")}`;
    const payload = Object.fromEntries(
      Object.entries(body).filter(([, value]) => value !== undefined && value !== null),
    );

    const headers = {
      "content-type": "application/json",
      "x-app-key": this.#options.appKey,
    };
    if (this.#options.apiKey) headers["x-api-key"] = this.#options.apiKey;
    if (this.sessionToken) headers["x-session-token"] = this.sessionToken;

    let lastFailure;
    for (let attempt = 0; attempt <= this.#options.maxRetries; attempt++) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), this.#options.timeout);
      try {
        const response = await fetch(url, {
          method: "POST",
          headers,
          body: JSON.stringify(payload),
          signal: controller.signal,
        });

        if (response.status >= 500 && attempt < this.#options.maxRetries) {
          await delay(250 * (attempt + 1));
          continue;
        }

        const text = await response.text();
        let envelope;
        try {
          envelope = text ? JSON.parse(text) : {};
        } catch {
          throw new AegisError("invalid_response", "Malformed API response.", response.status);
        }

        if (envelope.success) return envelope.data ?? {};
        throw new AegisError(
          envelope.error?.code ?? "server_error",
          envelope.error?.message ?? "Request failed.",
          response.status,
        );
      } catch (error) {
        if (error instanceof AegisError) throw error;
        lastFailure = error;
        if (attempt < this.#options.maxRetries) await delay(250 * (attempt + 1));
      } finally {
        clearTimeout(timer);
      }
    }
    throw new AegisError("network_error", lastFailure?.message ?? "Network request failed.");
  }

  // Application ---------------------------------------------------------

  /** Handshake. Call once before any other operation. */
  init() {
    return this.request("init", { version: this.#options.version });
  }

  status() {
    return this.request("status");
  }

  appData() {
    return this.request("app/data");
  }

  /** Download information published for this application. */
  downloads() {
    return this.request("downloads");
  }

  checkVersion(version) {
    return this.request("version/check", {
      version: version ?? this.#options.version,
      channel: this.#options.channel,
    });
  }

  // Authentication ---------------------------------------------------------

  async register(username, password, { email, licenseKey } = {}) {
    const data = await this.request("register", {
      username,
      password,
      email,
      license_key: licenseKey,
      hwid: await this.hardwareId(),
    });
    this.#storeSession(data);
    return data;
  }

  async login(username, password) {
    const data = await this.request("login", { username, password, hwid: await this.hardwareId() });
    this.#storeSession(data);
    return data;
  }

  async logout() {
    try {
      await this.request("logout");
    } finally {
      this.sessionToken = null;
      this.currentUser = null;
      this.currentLicense = null;
      this.stopHeartbeat();
    }
  }

  async heartbeat() {
    const data = await this.request("heartbeat");
    if (data?.user) this.currentUser = data.user;
    return data;
  }

  async checkSession() {
    const data = await this.request("session/check");
    if (data?.user) this.currentUser = data.user;
    if (data?.license) this.currentLicense = data.license;
    return data;
  }

  /** True when a token exists and the server still accepts it. */
  async isAuthenticated() {
    if (!this.sessionToken) return false;
    try {
      return (await this.checkSession()).valid === true;
    } catch {
      return false;
    }
  }

  userData() {
    return this.request("user/data");
  }

  // Licensing ---------------------------------------------------------

  async validateLicense(licenseKey) {
    return this.request("license/validate", {
      license_key: licenseKey,
      hwid: await this.hardwareId(),
    });
  }

  async activateLicense(licenseKey, username) {
    const data = await this.request("license/activate", {
      license_key: licenseKey,
      hwid: await this.hardwareId(),
      username,
    });
    if (data?.license) this.currentLicense = data.license;
    return data;
  }

  // Variables ---------------------------------------------------------

  getVariables({ scope = "application", licenseKey } = {}) {
    return this.request("variables/get", { scope, license_key: licenseKey });
  }

  setVariable(key, value, { scope = "user", licenseKey } = {}) {
    return this.request("variables/set", { scope, key, value, license_key: licenseKey });
  }

  triggerWebhook(event, payload = {}) {
    return this.request("webhook/trigger", { event, payload });
  }

  // Sessions ---------------------------------------------------------

  /** Starts a periodic heartbeat; `onRevoked` fires once when the session dies. */
  startHeartbeat({ intervalMs = 60000, onRevoked } = {}) {
    this.stopHeartbeat();
    this.#heartbeatTimer = setInterval(async () => {
      try {
        await this.heartbeat();
      } catch (error) {
        if (error instanceof AegisError && error.isNetworkError) return;
        this.stopHeartbeat();
        onRevoked?.(error);
      }
    }, intervalMs);
    this.#heartbeatTimer?.unref?.();
  }

  stopHeartbeat() {
    if (this.#heartbeatTimer) clearInterval(this.#heartbeatTimer);
    this.#heartbeatTimer = null;
  }

  #storeSession(data) {
    if (data?.session?.token) this.sessionToken = data.session.token;
    if (data?.user) this.currentUser = data.user;
    if (data?.license) this.currentLicense = data.license;
  }
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export default Aegis;
