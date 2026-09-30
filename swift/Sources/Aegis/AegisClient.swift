import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif

/// Client for the Aegis Authentication API.
/// Pure Foundation, no third-party dependencies. All calls are `async`.
public actor AegisClient {
    public static let sdkVersion = "1.0.0"

    private let options: AegisOptions
    private let baseUrl: String
    private let session: URLSession
    private var heartbeatTask: Task<Void, Never>?

    /// Machine identifier sent with authentication and licensing calls.
    public let hardwareId: String

    /// Current session token, or `nil` when signed out.
    public private(set) var sessionToken: String?

    public init(options: AegisOptions) {
        precondition(!options.baseUrl.isEmpty, "baseUrl is required.")
        precondition(!options.appKey.isEmpty, "appKey is required.")
        self.options = options
        self.baseUrl = options.baseUrl.hasSuffix("/") ? String(options.baseUrl.dropLast()) : options.baseUrl
        self.hardwareId = options.hwid ?? HardwareId.current()

        let configuration = URLSessionConfiguration.default
        configuration.timeoutIntervalForRequest = options.timeout
        self.session = URLSession(configuration: configuration)
    }

    /// Restores a session token persisted by the host application.
    public func useSession(_ token: String?) {
        sessionToken = token
    }

    /// Calls any endpoint and returns its `data` payload.
    @discardableResult
    public func request(_ endpoint: String, body: [String: Any] = [:]) async throws -> [String: Any] {
        let path = endpoint.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        guard let url = URL(string: "\(baseUrl)/api/public/v1/\(path)") else {
            throw AegisError(code: "invalid_options", message: "Invalid baseUrl.")
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.timeoutInterval = options.timeout
        request.setValue("application/json", forHTTPHeaderField: "content-type")
        request.setValue("aegis-swift-sdk/\(Self.sdkVersion)", forHTTPHeaderField: "user-agent")
        request.setValue(options.appKey, forHTTPHeaderField: "x-app-key")
        if let apiKey = options.apiKey { request.setValue(apiKey, forHTTPHeaderField: "x-api-key") }
        if let token = sessionToken { request.setValue(token, forHTTPHeaderField: "x-session-token") }
        request.httpBody = try JSONSerialization.data(withJSONObject: body.compactMapValues { $0 })

        var lastFailure: Error?
        for attempt in 0...options.maxRetries {
            do {
                let (data, response) = try await session.data(for: request)
                let status = (response as? HTTPURLResponse)?.statusCode ?? 0
                if status >= 500, attempt < options.maxRetries {
                    try? await Task.sleep(nanoseconds: UInt64(250_000_000 * (attempt + 1)))
                    continue
                }
                guard let envelope = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                    throw AegisError(code: "invalid_response", message: "Malformed API response.", status: status)
                }
                if envelope["success"] as? Bool == true {
                    return envelope["data"] as? [String: Any] ?? [:]
                }
                let error = envelope["error"] as? [String: Any] ?? [:]
                throw AegisError(
                    code: error["code"] as? String ?? "server_error",
                    message: error["message"] as? String ?? "Request failed.",
                    status: status
                )
            } catch let error as AegisError {
                throw error
            } catch {
                lastFailure = error
                if attempt < options.maxRetries {
                    try? await Task.sleep(nanoseconds: UInt64(250_000_000 * (attempt + 1)))
                }
            }
        }
        throw AegisError(code: "network_error", message: lastFailure?.localizedDescription ?? "Network request failed.")
    }

    // MARK: - Application

    /// Handshake. Call once before any other operation.
    @discardableResult
    public func initialize() async throws -> [String: Any] {
        try await request("init", body: ["version": options.version])
    }

    public func status() async throws -> [String: Any] { try await request("status") }

    public func appData() async throws -> [String: Any] { try await request("app/data") }

    /// Download information published for this application.
    public func downloads() async throws -> [String: Any] { try await request("downloads") }

    public func checkVersion(_ version: String? = nil) async throws -> [String: Any] {
        try await request("version/check", body: ["version": version ?? options.version, "channel": options.channel])
    }

    // MARK: - Authentication

    @discardableResult
    public func register(
        username: String,
        password: String,
        email: String? = nil,
        licenseKey: String? = nil
    ) async throws -> [String: Any] {
        let data = try await request("register", body: [
            "username": username, "password": password,
            "email": email as Any, "license_key": licenseKey as Any, "hwid": hardwareId,
        ])
        storeSession(data)
        return data
    }

    @discardableResult
    public func login(username: String, password: String) async throws -> [String: Any] {
        let data = try await request("login", body: ["username": username, "password": password, "hwid": hardwareId])
        storeSession(data)
        return data
    }

    public func logout() async throws {
        defer { sessionToken = nil }
        _ = try await request("logout")
    }

    @discardableResult
    public func heartbeat() async throws -> [String: Any] { try await request("heartbeat") }

    public func checkSession() async throws -> [String: Any] { try await request("session/check") }

    /// True when a token exists and the server still accepts it.
    public func isAuthenticated() async -> Bool {
        guard sessionToken != nil else { return false }
        return ((try? await checkSession())?["valid"] as? Bool) == true
    }

    public func userData() async throws -> [String: Any] { try await request("user/data") }

    // MARK: - Licensing

    public func validateLicense(_ licenseKey: String) async throws -> [String: Any] {
        try await request("license/validate", body: ["license_key": licenseKey, "hwid": hardwareId])
    }

    public func activateLicense(_ licenseKey: String, username: String? = nil) async throws -> [String: Any] {
        try await request("license/activate", body: [
            "license_key": licenseKey, "hwid": hardwareId, "username": username as Any,
        ])
    }

    // MARK: - Variables

    public func variables(scope: String = "application", licenseKey: String? = nil) async throws -> [String: Any] {
        try await request("variables/get", body: ["scope": scope, "license_key": licenseKey as Any])
    }

    @discardableResult
    public func setVariable(
        _ key: String,
        value: String,
        scope: String = "user",
        licenseKey: String? = nil
    ) async throws -> [String: Any] {
        try await request("variables/set", body: [
            "scope": scope, "key": key, "value": value, "license_key": licenseKey as Any,
        ])
    }

    @discardableResult
    public func triggerWebhook(event: String, payload: [String: Any] = [:]) async throws -> [String: Any] {
        try await request("webhook/trigger", body: ["event": event, "payload": payload])
    }

    // MARK: - Sessions

    /// Starts a background heartbeat; `onRevoked` fires once when the session dies.
    public func startHeartbeat(interval: TimeInterval = 60, onRevoked: @Sendable @escaping (String) -> Void = { _ in }) {
        stopHeartbeat()
        heartbeatTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: UInt64(interval * 1_000_000_000))
                guard let self, !Task.isCancelled else { return }
                do {
                    _ = try await self.heartbeat()
                } catch let error as AegisError {
                    await self.stopHeartbeat()
                    onRevoked(error.message)
                    return
                } catch {
                    // transient transport failure — retry on the next tick
                }
            }
        }
    }

    public func stopHeartbeat() {
        heartbeatTask?.cancel()
        heartbeatTask = nil
    }

    private func storeSession(_ data: [String: Any]) {
        if let session = data["session"] as? [String: Any], let token = session["token"] as? String {
            sessionToken = token
        }
    }
}