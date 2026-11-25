// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "CalcCore",
    platforms: [
        .iOS(.v16), .macOS(.v13),
    ],
    products: [
        .library(name: "CalcCore", targets: ["CalcCore"]),
    ],
    targets: [
        .target(name: "CalcCore"),
        .testTarget(name: "CalcCoreTests", dependencies: ["CalcCore"]),
    ]
)
