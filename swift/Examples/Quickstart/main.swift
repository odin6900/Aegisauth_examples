import Aegis
import Foundation

/// Runnable sample: `swift run AegisQuickstart`
func env(_ key: String, _ fallback: String) -> String {
    let value = ProcessInfo.processInfo.environment[key]
    return (value?.isEmpty == false) ? value! : fallback
}

let aegis = AegisClient(options: AegisOptions(
    baseUrl: env("AEGIS_BASE_URL", "http://localhost:8080"),
    appKey: env("AEGIS_APP_KEY", ""),
    version: "1.0.0"
))

do {
    let info = try await aegis.initialize()
    print("initialized: \(info["status"] ?? "ok")")

    if let version = info["version"] as? [String: Any], version["update_required"] as? Bool == true {
        print("mandatory update: \(version["latest"] ?? "")")
        exit(0)
    }

    let auth = try await aegis.login(
        username: env("AEGIS_USERNAME", "demo"),
        password: env("AEGIS_PASSWORD", "demo-password")
    )
    let user = auth["user"] as? [String: Any]
    print("signed in as \(user?["username"] ?? "?")")

    try await aegis.setVariable("last_seen", value: ISO8601DateFormatter().string(from: Date()))
    await aegis.startHeartbeat(interval: 60) { reason in print("session ended: \(reason)") }

    print("authenticated: \(await aegis.isAuthenticated())")
    try await aegis.logout()
    print("signed out.")
} catch let error as AegisError {
    FileHandle.standardError.write(Data("\(error)\n".utf8))
    exit(1)
}