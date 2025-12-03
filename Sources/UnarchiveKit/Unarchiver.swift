//
//  Unarchiver.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/2/25.
//

import Foundation

public final class Unarchiver: Sendable {
  let handler: any UnarchiveHandler

  public let url: URL
  public let format: Format

  public init(url: URL) throws(UnarchiverError) {
    do {
      let fileHandle = try FileHandle(forReadingFrom: url)
      guard let data = try fileHandle.read(upToCount: 512), let format = Format(data: data) else {
        throw UnarchiverError.unsupportedFormat
      }
      switch format {
      case .zip:
        self.handler = try ZIPHandler(url: url)
      case .rar4, .rar5:
        fatalError("Unimplements")
      case .sevenz:
        fatalError("Unimplements")
      case .tar:
        fatalError("Unimplements")
      }
      self.url = url
      self.format = format
    } catch let error as UnarchiverError {
      throw error
    } catch {
      throw UnarchiverError.fileAccessFailed(underlying: error)
    }
  }

  public func entries() async throws -> [Entry] {
    try await handler.entries()
      .compactMap {
        switch $0 {
        case .zip(let entry):
          return Entry(
            raw: $0,
            path: entry.name,
            type: entry.isDirectory ? .directory : (entry.isSymbolicLink ? .symbolicLink : .file),
            uncompressedSize: entry.uncompressedSize,
            compressedSize: entry.compressedSize
          )
        }
      }
  }

  public func extract(_ entry: Entry, upToCount count: Int? = nil) async throws -> Data {
    try await handler.extract(entry.raw, upToCount: count)
  }
}

// MARK: - Unarchiver.Format

extension Unarchiver {
  public enum Format: Sendable {
    case zip
    case rar4
    case rar5
    case sevenz
    case tar

    init?(data: Data) {
      if data.hasPrefix([0x50, 0x4B, 0x03, 0x04]) || data.hasPrefix([0x50, 0x4B, 0x05, 0x06]) || data.hasPrefix([0x50, 0x4B, 0x07, 0x08]) {
        self = .zip
      } else if data.hasPrefix([0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00]) {
        self = .rar4
      } else if data.hasPrefix([0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00]) {
        self = .rar5
      } else if data.hasPrefix([0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C]) {
        self = .sevenz
      } else if data.subdata(in: 257..<263).hasPrefix([0x75, 0x73, 0x74, 0x61, 0x72, 0x00]) {
        self = .tar
      } else {
        return nil
      }
    }
  }
}

// MARK: - Unarchiver.Entry

extension Unarchiver {
  public struct Entry: Sendable, Hashable {
    let raw: UnarchiveEntry
    public let path: String
    public let type: EntryType
    public let uncompressedSize: Int64
    public let compressedSize: Int64
  }
}

// MARK: - Unarchiver.EntryType

extension Unarchiver {
  public enum EntryType: Sendable {
    case file
    case directory
    case symbolicLink
  }
}

// MARK: - Unarchiver.Info

extension Unarchiver {
  public struct Info: Sendable {
    public let format: Format
    public let entryCount: Int
    public let totalUncompressedSize: Int64
    public let totalCompressedSize: Int64
    public let isEncrypted: Bool
  }
}

// MARK: - DataProtocol (hasPrefix)

extension DataProtocol {
  @inline(__always)
  func hasPrefix(_ bytes: [UInt8]) -> Bool {
    prefix(bytes.count).elementsEqual(bytes)
  }

  @inline(__always)
  func hasPrefix<D: DataProtocol>(_ data: D) -> Bool {
    prefix(data.count).elementsEqual(data)
  }
}
