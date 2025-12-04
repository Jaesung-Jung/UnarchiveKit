// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "UnarchiveKit",
  platforms: [
    .macOS(.v13),
    .iOS(.v13),
    .tvOS(.v13),
    .watchOS(.v6),
    .visionOS(.v1)
  ],
  products: [
    .library(
      name: "UnarchiveKit",
      targets: ["UnarchiveKit"]
    ),
  ],
  targets: [
    .target(
      name: "UnarchiveKit",
      dependencies: [
        "minizip",
        "libtar"
      ]
    ),
    .target(
      name: "minizip",
      publicHeadersPath: ".",
      cSettings: [
        .define("HAVE_ZLIB"),
        .define("HAVE_BZIP2"),
        .define("HAVE_WZAES"),
        .define("HAVE_PKCRYPT"),
        .define("ZLIB_COMPAT")
      ],
      linkerSettings: [
        .linkedLibrary("z"),
        .linkedLibrary("bz2")
      ]
    ),
    .target(
      name: "libtar",
      cSettings: [
        .headerSearchPath("."),
        .unsafeFlags([
          "-Wno-format",
          "-Wno-conversion"
        ])
      ]
    ),
    .testTarget(
      name: "UnarchiveKitTests",
      dependencies: ["UnarchiveKit"]
    ),
  ]
)
