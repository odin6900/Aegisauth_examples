import Foundation

/// Every Aegis API failure surfaces as this error.
public struct AegisError: Error, CustomStringConvertible {
    public let code: String
    public let message: String
    public let status: Int

    public init(code: String, message: String, status: Int = 0) {
        self.code = code
        self.message = message
        self.status = status
    }

    public var isNetworkError: Bool { status == 0 }
    public var isAuthError: Bool { code == "unauthorized" || code == "invalid_credentials" }
    public var isLicenseError: Bool { code.hasPrefix("license") || code == "hwid_mismatch" }

    public var description: String { "Aegis error [\(code)] \(message)" }
}