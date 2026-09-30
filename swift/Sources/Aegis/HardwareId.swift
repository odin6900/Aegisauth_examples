import Foundation
#if canImport(CryptoKit)
import CryptoKit
#endif

/// Stable, non-reversible machine identifier.
public enum HardwareId {
    public static func current() -> String {
        let info = ProcessInfo.processInfo
        let facts = [
            info.hostName,
            info.operatingSystemVersionString,
            info.processorCount.description,
            info.environment["USER"] ?? info.environment["USERNAME"] ?? "",
        ].joined(separator: "|")

        let data = Data(facts.utf8)
        #if canImport(CryptoKit)
        return SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
        #else
        var hash: UInt64 = 1469598103934665603
        for byte in data {
            hash = (hash ^ UInt64(byte)) &* 1099511628211
        }
        var out = ""
        var seed = hash
        while out.count < 64 {
            seed = seed &* 6364136223846793005 &+ 1442695040888963407
            out += String(format: "%016lx", seed)
        }
        return String(out.prefix(64))
        #endif
    }
}