import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.time.Instant

// Configuration Placeholders
const val AUTH_URL = "https://auth.example.com"
const val APP_KEY = "YOUR_APP_KEY"
const val APP_NAME = "My Application"
const val APP_VERSION = "1.0.0"

const val USERNAME = "YOUR_USERNAME"
const val PASSWORD = "YOUR_PASSWORD"
const val MODULE_NAME = "minecraft"

class AegisAuthClient(
    private val authUrl: String,
    private val appKey: String,
    private val appName: String,
    private val appVersion: String
) {
    private val client = HttpClient.newHttpClient()
    private val baseUrl = "${authUrl.trimEnd('/')}/api/public/v1/"
    val hwid = getHardwareId()
    var sessionToken: String? = null

    private fun getHardwareId(): String {
        return try {
            val seed = "${System.getProperty("os.name")}-${System.getProperty("user.name")}-${Runtime.getRuntime().availableProcessors()}"
            val md = MessageDigest.getInstance("SHA-256")
            val hash = md.digest(seed.toByteArray(StandardCharsets.UTF_8))
            hash.joinToString("") { "%02x".format(it) }
        } catch (e: Exception) {
            "kotlin-fallback-hwid"
        }
    }

    private fun post(endpoint: String, jsonBody: String): String {
        val uri = URI.create("$baseUrl$endpoint")
        val builder = HttpRequest.newBuilder()
            .uri(uri)
            .header("Content-Type", "application/json")
            .header("x-app-key", appKey)
            .header("x-timestamp", Instant.now().epochSecond.toString())
            .header("User-Agent", "AegisAuth-Kotlin/$appVersion")
            .POST(HttpRequest.BodyPublishers.ofString(jsonBody))

        sessionToken?.let {
            builder.header("x-session-token", it)
        }

        val response = client.send(builder.build(), HttpResponse.BodyHandlers.ofString())
        return response.body()
    }

    private fun extractJsonString(json: String, key: String): String? {
        val regex = "\"$key\"\\s*:\\s*\"([^\"]+)\"".toRegex()
        return regex.find(json)?.groupValues?.get(1)
    }

    fun init(): Boolean {
        println("[*] Initializing $appName (v$appVersion)...")
        println("    HWID: $hwid")

        val res = post("init", "{\"version\":\"$appVersion\"}")
        if (!res.contains("\"success\":true")) {
            println("[-] Init failed: $res")
            return false
        }

        println("[+] Handshake successful.")
        return true
    }

    fun login(user: String, pass: String): Boolean {
        println("\n[*] Logging in as '$user'...")
        val payload = "{\"username\":\"$user\",\"password\":\"$pass\",\"hwid\":\"$hwid\"}"
        val res = post("login", payload)

        if (!res.contains("\"success\":true")) {
            val err = extractJsonString(res, "message") ?: res
            println("[-] Login failed: $err")
            return false
        }

        sessionToken = extractJsonString(res, "token")
        val preview = sessionToken?.let { if (it.length > 12) it.substring(0, 12) + "..." else it }
        println("[+] Login successful! Session token: $preview")
        return true
    }

    fun requestModuleToken(module: String): String? {
        println("\n[*] Requesting signed download token for module '$module'...")
        val res = post("module/token", "{\"module\":\"$module\"}")

        if (!res.contains("\"success\":true")) {
            val err = extractJsonString(res, "message") ?: res
            println("[-] Module token request failed: $err")
            return null
        }

        val key = extractJsonString(res, "key")
        val downloadUrl = extractJsonString(res, "download_url")

        println("[+] Signed Token Received!")
        println("    Target Asset: $key")
        println("    Download URL: $downloadUrl")
        return downloadUrl
    }

    fun logout() {
        if (sessionToken != null) {
            println("\n[*] Logging out...")
            post("logout", "{}")
            sessionToken = null
            println("[+] Logged out cleanly.")
        }
    }
}

fun main() {
    val client = AegisAuthClient(AUTH_URL, APP_KEY, APP_NAME, APP_VERSION)

    if (!client.init()) return
    if (!client.login(USERNAME, PASSWORD)) return

    val downloadUrl = client.requestModuleToken(MODULE_NAME)
    if (downloadUrl != null) {
        println("\n[+] Binary payload authorized for AetherVault silent streaming!")
    }

    client.logout()
}
