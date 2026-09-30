package io.aegis.sdk

/** Every Aegis API failure surfaces as this exception. */
class AegisException(
    val code: String,
    message: String,
    val status: Int = 0,
    cause: Throwable? = null,
) : RuntimeException(message, cause) {

    val isNetworkError: Boolean get() = status == 0
    val isAuthError: Boolean get() = code == "unauthorized" || code == "invalid_credentials"
    val isLicenseError: Boolean get() = code.startsWith("license") || code == "hwid_mismatch"

    override fun toString(): String = "AegisException[$code] ${message.orEmpty()}"
}