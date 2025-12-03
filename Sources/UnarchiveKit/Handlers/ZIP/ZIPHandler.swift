//
//  ZIPHandler.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/2/25.
//

import Foundation
import MinizipNG

actor ZIPHandler: UnarchiveHandler {
  let url: URL
  let zip: ZIP

  init(url: URL) throws(UnarchiverError) {
    let path = if #available(macOS 13.0, iOS 16.0, tvOS 16.0, watchOS 9.0, *) {
      url.path(percentEncoded: false)
    } else {
      url.path
    }
    self.url = url
    self.zip = try ZIP(path: path)
  }

  func entries() throws(UnarchiverError) -> [UnarchiveEntry] {
    let handle = zip.handle

    var entries: [UnarchiveEntry] = []
    var result = mz_zip_goto_first_entry(handle)
    while result == MZ_OK {
      var infoPointer: UnsafeMutablePointer<mz_zip_file>? = nil
      guard mz_zip_entry_get_info(handle, &infoPointer) == MZ_OK, let info = infoPointer?.pointee else {
        throw .archiveIterationFailed
      }

      let entry = ZIPEntry(
        name: String(cString: info.filename),
        isDirectory: mz_zip_entry_is_dir(handle) == MZ_OK,
        isSymbolicLink: mz_zip_entry_is_symlink(handle) == MZ_OK,
        compressedSize: info.compressed_size,
        uncompressedSize: info.uncompressed_size,
        creationDate: Date(timeIntervalSince1970: TimeInterval(info.creation_date)),
        modifiedDate: Date(timeIntervalSince1970: TimeInterval(info.modified_date)),
        accessedDate: Date(timeIntervalSince1970: TimeInterval(info.accessed_date)),
        offset: mz_zip_get_entry(handle),
        crc: info.crc
      )
      entries.append(.zip(entry))

      result = mz_zip_goto_next_entry(handle)
    }

    if result != MZ_END_OF_LIST, result != MZ_OK {
      throw .archiveIterationFailed
    }
    return entries
  }

  func extract(_ entry: UnarchiveEntry, upToCount count: Int?) throws(UnarchiverError) -> Data {
    guard case .zip(let entry) = entry else {
      throw .invalidEntryContext
    }
    let handle = zip.handle

    guard mz_zip_goto_first_entry(handle) == MZ_OK else {
      throw .entryNotFound
    }
    guard mz_zip_goto_entry(handle, entry.offset) == MZ_OK else {
      throw .entryNotFound
    }
    guard mz_zip_entry_read_open(handle, 0, nil) == MZ_OK else {
      throw .extractFailed(path: entry.name)
    }
    defer {
      mz_zip_entry_close(handle)
    }

    let uncompressedSize = Int(entry.uncompressedSize)
    let bytesToRead = min(uncompressedSize, count ?? uncompressedSize)
    let bufferSize = 65_536

    var data = Data(capacity: bytesToRead)
    var buffer = [UInt8](repeating: 0, count: bufferSize)

    var remaining = bytesToRead
    while remaining > 0 {
      let toRead = Int32(min(bufferSize, remaining))
      let readCount = mz_zip_entry_read(handle, &buffer, toRead)
      if readCount < 0 {
        throw .extractFailed(path: entry.name)
      }
      if readCount == 0 {
        break // EOF
      }
      data.append(buffer, count: Int(readCount))
      remaining -= Int(readCount)
    }
    return data
  }
}

// MARK: - ZIPHandler.Reader

extension ZIPHandler {
  final class ZIP {
    let handle: UnsafeMutableRawPointer
    let stream: UnsafeMutableRawPointer

    init(path: String) throws(UnarchiverError) {
      var stream: UnsafeMutableRawPointer! = mz_stream_os_create()
      guard mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ) == MZ_OK else {
        mz_stream_os_delete(&stream)
        throw .archiveOpenFailed(path: path)
      }

      var handle: UnsafeMutableRawPointer! = mz_zip_create()
      guard mz_zip_open(handle, stream, MZ_OPEN_MODE_READ) == MZ_OK else {
        mz_zip_delete(&handle)
        mz_stream_os_close(stream)
        mz_stream_os_delete(&stream)
        throw .archiveOpenFailed(path: path)
      }
      self.handle = handle
      self.stream = stream
    }

    deinit {
      var handle: UnsafeMutableRawPointer? = handle
      mz_zip_close(handle)
      mz_zip_delete(&handle)

      var stream: UnsafeMutableRawPointer? = stream
      mz_stream_os_close(stream)
      mz_stream_os_delete(&stream)
    }
  }
}
