package io.aegis.sdk

import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse
import java.security.MessageDigest
import java.time.Duration
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/** Client configuration. */
data class AegisOptions(
    val baseUrl: String,
    val appKey: String,
    val apiKey: String? = null,
    val version: String = "1.0.0",
    val channel: String = "stable",
    val hwid: String? = null,
    val timeout: Duration = Duration.ofSeconds(20),
    val maxRetries: Int = 2,
)

/**
 * Client for the Aegis Authentication API.
 * JDK 17+, zero third-party dependencies. Thread-safe and [AutoCloseable].
 */
class Aegis(private val options: AegisOptions) : AutoCloseable {

    private val baseUrl = options.baseUrl.trimEnd('/')
    private val http: HttpClient = HttpClient.newBuilder().connectTimeout(options.timeout).build()
    private var heartbeatScheduler: ScheduledExecutorService? = null

    /** Machine identifier sent with authentication and licensing calls. */
    val hardwareId: String = options.hwid ?: hardwareId()

    /** Current session token, or null when signed out. */
    @Volatile
    var sessionToken: String? = null
        private set

    init {
        require(options.baseUrl.isNotBlank()) { "baseUrl is required." }
        require(options.appKey.isNotBlank()) { "appKey is required." }
    }

    companion object {
        const val SDK_VERSION = "1.0.0"

        /** Stable, non-reversible machine identifier. */
        fun hardwareId(): String {
            val facts = listOf(
                System.getProperty("os.name").orEmpty(),
                System.getProperty("os.arch").orEmpty(),
                System.getProperty("user.name").orEmpty(),
                runCatching { java.net.InetAddress.getLocalHost().hostName }.getOrDefault("unknown"),
            ).joinToString("|")
            return MessageDigest.getInstance("SHA-256").digest(facts.toByteArray())
                .joinToString("") { "%02x".format(it) }
        }
    }

    /** Restores a session token persisted by the host application. */
    fun useSession(token: String?) {
        sessionToken = token
    }

    /** Calls any endpoint and returns its `data` payload. */
    @Suppress("UNCHECKED_CAST")
    fun request(endpoint: String, body: Map<String, Any?> = emptyMap()): Map<String, Any?> {
        val payload = Json.encode(body.filterValues { it != null })
        val builder = HttpRequest.newBuilder(URI.create("$baseUrl/api/public/v1/${endpoint.trim('/')}"))
            .timeout(options.timeout)
            .header("content-type", "application/json")
            .header("user-agent", "aegis-kotlin-sdk/$SDK_VERSION")
            .header("x-app-key", options.appKey)
            .POST(HttpRequest.BodyPublishers.ofString(payload))
        options.apiKey?.let { builder.header("x-api-key", it) }
        sessionToken?.let { builder.header("x-session-token", it) }
        val httpRequest = builder.build()

        var lastFailure: Exception? = null
        for (attempt in 0..options.maxRetries) {
            try {
                val response = http.send(httpRequest, HttpResponse.BodyHandlers.ofString())
                if (response.statusCode() >= 500 && attempt < options.maxRetries) {
                    Thread.sleep(250L * (attempt + 1))
                    continue
                }
                val envelope = Json.decode(response.body().ifBlank { "{}" }) as? Map<String, Any?>
                    ?: throw AegisException("invalid_response", "Malformed API response.", response.statusCode())
                if (envelope.bool("success")) {
                    return envelope.obj("data")
                }
                val error = envelope.obj("error")
                throw AegisException(
                    error.str("code", "server_error"),
                    error.str("message", "Request failed."),
                    response.statusCode(),
                )
            } catch (error: AegisException) {
                throw error
            } catch (error: Exception) {
                lastFailure = error
                if (attempt < options.maxRetries) Thread.sleep(250L * (attempt + 1))
            }
        }
        throw AegisException("network_error", lastFailure?.message ?: "Network request failed.", 0, lastFailure)
    }

    // Application ---------------------------------------------------------

    /** Handshake. Call once before any other operation. */
    fun init(): Map<String, Any?> = request("init", mapOf("version" to options.version))

    fun status(): Map<String, Any?> = request("status")

    fun appData(): Map<String, Any?> = request("app/data")

    /** Download information published for this application. */
    fun downloads(): Map<String, Any?> = request("downloads")

    fun checkVersion(version: String? = null): Map<String, Any?> =
        request("version/check", mapOf("version" to (version ?: options.version), "channel" to options.channel))

    // Authentication ---------------------------------------------------------

    fun register(
        username: String,
        password: String,
        email: String? = null,
        licenseKey: String? = null,
    ): Map<String, Any?> = request(
        "register",
        mapOf(
            "username" to username, "password" to password, "email" to email,
            "license_key" to licenseKey, "hwid" to hardwareId,
        ),
    ).also(::storeSession)

    fun login(username: String, password: String): Map<String, Any?> = request(
        "login",
        mapOf("username" to username, "password" to password, "hwid" to hardwareId),
    ).also(::storeSession)

    fun logout() {
        try {
            request("logout")
        } finally {
            sessionToken = null
        }
    }

    fun heartbeat(): Map<String, Any?> = request("heartbeat")

    fun checkSession(): Map<String, Any?> = request("session/check")

    /** True when a token exists and the server still accepts it. */
    fun isAuthenticated(): Boolean {
        if (sessionToken == null) return false
        return runCatching { checkSession().bool("valid") }.getOrDefault(false)
    }

    fun userData(): Map<String, Any?> = request("user/data")

    // Licensing ---------------------------------------------------------

    fun validateLicense(licenseKey: String): Map<String, Any?> =
        request("license/validate", mapOf("license_key" to licenseKey, "hwid" to hardwareId))

    fun activateLicense(licenseKey: String, username: String? = null): Map<String, Any?> =
        request("license/activate", mapOf("license_key" to licenseKey, "hwid" to hardwareId, "username" to username))

    // Variables ---------------------------------------------------------

    fun getVariables(scope: String = "application", licenseKey: String? = null): Map<String, Any?> =
        request("variables/get", mapOf("scope" to scope, "license_key" to licenseKey))

    fun setVariable(
        key: String,
        value: String,
        scope: String = "user",
        licenseKey: String? = null,
    ): Map<String, Any?> = request(
        "variables/set",
        mapOf("scope" to scope, "key" to key, "value" to value, "license_key" to licenseKey),
    )

    fun triggerWebhook(event: String, payload: Map<String, Any?> = emptyMap()): Map<String, Any?> =
        request("webhook/trigger", mapOf("event" to event, "payload" to payload))

    // Sessions ---------------------------------------------------------

    /** Starts a daemon heartbeat; [onRevoked] fires once when the session dies. */
    fun startHeartbeat(interval: Duration = Duration.ofSeconds(60), onRevoked: (String) -> Unit = {}) {
        stopHeartbeat()
        val scheduler = Executors.newSingleThreadScheduledExecutor { runnable ->
            Thread(runnable, "aegis-heartbeat").apply { isDaemon = true }
        }
        heartbeatScheduler = scheduler
        scheduler.scheduleAtFixedRate({
            try {
                heartbeat()
            } catch (error: AegisException) {
                stopHeartbeat()
                onRevoked(error.message ?: error.code)
            }
        }, interval.seconds, interval.seconds, TimeUnit.SECONDS)
    }

    fun stopHeartbeat() {
        heartbeatScheduler?.shutdownNow()
        heartbeatScheduler = null
    }

    override fun close() = stopHeartbeat()

    private fun storeSession(data: Map<String, Any?>) {
        val token = data.obj("session").str("token")
        if (token.isNotBlank()) sessionToken = token
    }
}