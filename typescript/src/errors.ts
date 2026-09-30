/** Every Aegis API failure surfaces as this error. */
export class AegisError extends Error {
  readonly code: string;
  readonly status: number;

  constructor(code: string, message: string, status = 0, options?: { cause?: unknown }) {
    super(message, options);
    this.name = "AegisError";
    this.code = code;
    this.status = status;
  }

  get isNetworkError(): boolean {
    return this.status === 0;
  }

  get isAuthError(): boolean {
    return this.code === "unauthorized" || this.code === "invalid_credentials";
  }

  get isLicenseError(): boolean {
    return this.code.startsWith("license") || this.code === "hwid_mismatch";
  }
}
