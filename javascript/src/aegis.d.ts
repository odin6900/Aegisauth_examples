/** Type declarations for the plain-JavaScript Aegis SDK. */

export declare const SDK_VERSION: string;

export interface AegisOptions {
  /** Base URL of your Aegis deployment, e.g. `https://your-aegis-host`. */
  baseUrl: string;
  /** Public application key from the Aegis dashboard. */
  appKey: string;
  /** Optional server-side API key. Never ship this in a browser bundle. */
  apiKey?: string;
  /** Client version reported to `init` and `version/check`. */
  version?: string;
  /** Release channel used by version checks. */
  channel?: string;
  /** Overrides the automatically derived hardware id. */
  hwid?: string;
  /** Per-request timeout in milliseconds. */
  timeout?: number;
  /** Retries on transport/5xx failures. */
  maxRetries?: number;
}

export declare class AegisError extends Error {
  readonly code: string;
  readonly status: number;
  readonly isNetworkError: boolean;
  readonly isAuthError: boolean;
  readonly isLicenseError: boolean;
  constructor(code: string, message: string, status?: number);
}

export declare function hardwareId(): Promise<string>;

export declare class Aegis {
  sessionToken: string | null;
  constructor(options: AegisOptions);

  hardwareId(): Promise<string>;
  useSession(token: string | null): void;
  request(endpoint: string, body?: Record<string, unknown>): Promise<Record<string, any>>;

  init(): Promise<Record<string, any>>;
  status(): Promise<Record<string, any>>;
  appData(): Promise<Record<string, any>>;
  downloads(): Promise<Record<string, any>>;
  checkVersion(version?: string): Promise<Record<string, any>>;

  register(
    username: string,
    password: string,
    extra?: { email?: string; licenseKey?: string },
  ): Promise<Record<string, any>>;
  login(username: string, password: string): Promise<Record<string, any>>;
  logout(): Promise<void>;
  heartbeat(): Promise<Record<string, any>>;
  checkSession(): Promise<Record<string, any>>;
  isAuthenticated(): Promise<boolean>;
  userData(): Promise<Record<string, any>>;

  validateLicense(licenseKey: string): Promise<Record<string, any>>;
  activateLicense(licenseKey: string, username?: string): Promise<Record<string, any>>;

  getVariables(options?: { scope?: string; licenseKey?: string }): Promise<Record<string, any>>;
  setVariable(
    key: string,
    value: string,
    options?: { scope?: string; licenseKey?: string },
  ): Promise<Record<string, any>>;
  triggerWebhook(event: string, payload?: Record<string, unknown>): Promise<Record<string, any>>;

  startHeartbeat(options?: { intervalMs?: number; onRevoked?: (error: AegisError) => void }): void;
  stopHeartbeat(): void;
}

export default Aegis;
