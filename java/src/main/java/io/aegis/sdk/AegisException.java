package io.aegis.sdk;

/** Every Aegis API failure surfaces as this exception. */
public class AegisException extends RuntimeException {
    private final String code;
    private final int status;

    public AegisException(String code, String message, int status) {
        this(code, message, status, null);
    }

    public AegisException(String code, String message, int status, Throwable cause) {
        super(message, cause);
        this.code = code;
        this.status = status;
    }

    public String getCode() {
        return code;
    }

    public int getStatus() {
        return status;
    }

    /** Transport-level failure (no HTTP status). */
    public boolean isNetworkError() {
        return status == 0;
    }

    /** Rejected credentials or session. */
    public boolean isAuthError() {
        return "unauthorized".equals(code) || "invalid_credentials".equals(code);
    }

    /** Licensing or hardware-binding failure. */
    public boolean isLicenseError() {
        return code != null && (code.startsWith("license") || code.equals("hwid_mismatch"));
    }
}