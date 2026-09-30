package io.aegis.sdk.examples

import io.aegis.sdk.Aegis
import io.aegis.sdk.AegisException
import io.aegis.sdk.AegisOptions
import io.aegis.sdk.bool
import io.aegis.sdk.obj
import io.aegis.sdk.str
import java.time.Duration

/** Runnable sample: `./gradlew run` or execute this main from your IDE. */
fun main() {
    val aegis = Aegis(
        AegisOptions(
            baseUrl = System.getenv("AEGIS_BASE_URL") ?: "http://localhost:8080",
            appKey = System.getenv("AEGIS_APP_KEY") ?: "",
            version = "1.0.0",
        ),
    )

    aegis.use {
        try {
            val info = it.init()
            println("initialized: ${info.str("status", "ok")}")

            if (info.obj("version").bool("update_required")) {
                println("mandatory update: ${info.obj("version").str("latest")}")
                return
            }

            val auth = it.login(
                System.getenv("AEGIS_USERNAME") ?: "demo",
                System.getenv("AEGIS_PASSWORD") ?: "demo-password",
            )
            println("signed in as ${auth.obj("user").str("username")}")

            it.setVariable("last_seen", java.time.Instant.now().toString())
            it.startHeartbeat(Duration.ofSeconds(60)) { reason -> println("session ended: $reason") }

            println("authenticated: ${it.isAuthenticated()}")
            it.logout()
            println("signed out.")
        } catch (error: AegisException) {
            System.err.println("Aegis error [${error.code}] ${error.message}")
            kotlin.system.exitProcess(1)
        }
    }
}