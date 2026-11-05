// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "UntruncGUI",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "UntruncGUI",
            path: "Sources/UntruncGUI",
            resources: [
                .copy("untrunc"),
                .copy("AppIcon.icns")
            ]
        )
    ]
)
