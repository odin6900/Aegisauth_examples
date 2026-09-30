import Foundation

/// Client configuration.
public struct AegisOptions {
    public var baseUrl: String
    public var appKey: String
    public var apiKey: String?
    public var version: String
    public var channel: String
    public var hwid: String?
    public var timeout: TimeInterval
    public var maxRetries: Int

    public init(
        baseUrl: String,
        appKey: String,
        apiKey: String? = nil,
        version: String = "1.0.0",
        channel: String = "stable",
        hwid: String? = nil,
        timeout: TimeInterval = 20,
        maxRetries: Int = 2
    ) {
        self.baseUrl = baseUrl
        self.appKey = appKey
        self.apiKey = apiKey
        self.version = version
        self.channel = channel
        self.hwid = hwid
        self.timeout = timeout
        self.maxRetries = maxRetries
    }
}