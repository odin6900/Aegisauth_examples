export interface AegisOptions {
  /** Base URL of the Aegis deployment, e.g. https://your-aegis-host */
  baseUrl: string;
  /** Application public key (sent as x-app-key). */
  appKey: string;
  /** Optional server-side API key for privileged endpoints. */
  apiKey?: string;
  /** Client build version reported to init/version check. */
  version?: string;
  /** Release channel used by version checks. */
  channel?: string;
  /** Hardware id. Auto-derived when omitted. */
  hwid?: string;
  /** Per-request timeout in milliseconds. */
  timeoutMs?: number;
  /** Retries for transient network/5xx failures. */
  maxRetries?: number;
  /** Custom fetch implementation (defaults to global fetch). */
  fetch?: typeof fetch;
}

export interface AegisSubscription {
  name: string;
  status: "active" | "paused" | "expired" | string;
  expires_at?: string | null;
  duration_days?: number | null;
  duration_minutes?: number | null;
}

export interface AegisUser {
  id: string;
  username: string;
  email?: string | null;
  status?: string;
  hwid?: string | null;
  created_at?: string;
  last_login_at?: string | null;
  expires_at?: string | null;
  subscriptions?: AegisSubscription[];
}

export interface AegisLicense {
  key?: string;
  status?: string;
  expires_at?: string | null;
  duration_days?: number | null;
  duration_minutes?: number | null;
  activations?: number;
  max_activations?: number;
  level?: number;
  subscriptions?: AegisSubscription[];
}

export interface AegisSession {
  token: string;
  expires_at?: string;
}

export interface AegisVersionInfo {
  latest?: string;
  current?: string;
  update_available?: boolean;
  update_required?: boolean;
  download_url?: string | null;
  changelog?: string | null;
}

export interface AegisInitResult {
  application?: Record<string, unknown>;
  version?: AegisVersionInfo;
  status?: string;
}

export interface AegisAuthResult {
  user: AegisUser;
  license?: AegisLicense | null;
  session?: AegisSession;
}

export interface AegisLicenseResult {
  valid?: boolean;
  activated?: boolean;
  status?: string;
  license?: AegisLicense | null;
}

export interface AegisSessionCheck {
  valid?: boolean;
  alive?: boolean;
  user?: AegisUser;
  license?: AegisLicense | null;
  expires_at?: string;
}

export interface AegisVariables {
  scope: string;
  variables: Record<string, string>;
}

export interface AegisEnvelope<T> {
  success: boolean;
  data?: T;
  error?: { code: string; message: string };
  timestamp: string;
}
