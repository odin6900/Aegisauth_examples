// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "Aegis",
    platforms: [.macOS(.v12), .iOS(.v15), .tvOS(.v15), .watchOS(.v8)],
    products: [
        .library(name: "Aegis", targets: ["Aegis"]),
        .executable(name: "AegisQuickstart", targets: ["AegisQuickstart"]),
    ],
    targets: [
        .target(name: "Aegis"),
        .executableTarget(name: "AegisQuickstart", dependencies: ["Aegis"], path: "Examples/Quickstart"),
    ]
)