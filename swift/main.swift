import Foundation
import CryptoKit

// Configuration Placeholders
let authUrl = "https://auth.example.com"
let appKey = "YOUR_APP_KEY"
let appName = "My Application"
let appVersion = "1.0.0"

let username = "YOUR_USERNAME"
let password = "YOUR_PASSWORD"
let moduleName = "minecraft"

func getHardwareId() -> String {
    let hostname = ProcessInfo.processInfo.hostName
    let osVer = ProcessInfo.processInfo.operatingSystemVersionString
    let seed = "\(hostname)-\(osVer)"
    let hash = SHA256.hash(data: Data(seed.utf8))
    return hash.compactMap { String(format: "%02x", $0) }.joined()
}

class AegisAuthClient {
    let baseUrl: String
    let hwid: String
    var sessionToken: String? = nil

    init() {
        self.baseUrl = "\(authUrl.trimmingCharacters(in: CharacterSet(charactersIn: "/")))/api/public/v1/"
        self.hwid = getHardwareId()
    }

    func post(endpoint: String, payload: [String: Any]) async -> [String: Any]? {
        guard let url = URL(string: "\(baseUrl)\(endpoint)") else { return nil }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue(appKey, forHTTPHeaderField: "x-app-key")
        request.setValue("\(Int(Date().timeIntervalSince1970))", forHTTPHeaderField: "x-timestamp")
        request.setValue("AegisAuth-Swift/\(appVersion)", forHTTPHeaderField: "User-Agent")

        if let token = sessionToken {
            request.setValue(token, forHTTPHeaderField: "x-session-token")
        }

        request.httpBody = try? JSONSerialization.data(withJSONObject: payload)

        do {
            let (data, _) = try await URLSession.shared.data(for: request)
            return try JSONSerialization.jsonObject(with: data) as? [String: Any]
        } catch {
            print("[-] Network error: \(error.localizedDescription)")
            return nil
        }
    }

    func start() async {
        print("=== AegisAuth Swift Client [\(appName) v\(appVersion)] ===")
        print("[*] HWID: \(hwid)")

        // 1. Handshake
        print("\n[*] Initializing application...")
        guard let initRes = await post(endpoint: "init", payload: ["version": appVersion]),
              let success = initRes["success"] as? Bool, success else {
            print("[-] Init failed")
            return
        }
        print("[+] Application initialized successfully.")

        // 2. Login
        print("\n[*] Logging in as '\(username)'...")
        guard let loginRes = await post(endpoint: "login", payload: [
            "username": username,
            "password": password,
            "hwid": hwid
        ]), let loginSuccess = loginRes["success"] as? Bool, loginSuccess else {
            print("[-] Login failed")
            return
        }

        if let data = loginRes["data"] as? [String: Any] {
            sessionToken = data["token"] as? String
            let preview = sessionToken.map { $0.count > 12 ? "\($0.prefix(12))..." : $0 } ?? ""
            print("[+] Login successful! Session token: \(preview)")
        }

        // 3. Request module token for AetherVault
        print("\n[*] Requesting signed download token for module '\(moduleName)'...")
        guard let tokenRes = await post(endpoint: "module/token", payload: ["module": moduleName]),
              let tokenSuccess = tokenRes["success"] as? Bool, tokenSuccess,
              let data = tokenRes["data"] as? [String: Any] else {
            print("[-] Module token request failed")
            return
        }

        let key = data["key"] as? String ?? ""
        let downloadUrl = data["download_url"] as? String ?? ""
        let exp = data["expires_in_seconds"] as? Int ?? 0

        print("[+] Signed Token Received!")
        print("    Target Asset: \(key)")
        print("    Download URL: \(downloadUrl)")
        print("    Expires In  : \(exp)s")
        print("\n[+] Binary payload authorized for AetherVault silent streaming!")

        // 4. Logout
        if sessionToken != nil {
            print("\n[*] Logging out...")
            _ = await post(endpoint: "logout", payload: [:])
            sessionToken = nil
            print("[+] Logged out cleanly.")
        }
    }
}

let client = AegisAuthClient()
let semaphore = DispatchSemaphore(value: 0)
Task {
    await client.start()
    semaphore.signal()
}
semaphore.wait()
