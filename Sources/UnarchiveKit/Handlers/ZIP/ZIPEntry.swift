//
//  ZIPEntry.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/2/25.
//

import Foundation

struct ZIPEntry: Sendable, Hashable {
  let name: String
  let isDirectory: Bool
  let isSymbolicLink: Bool
  let compressedSize: Int64
  let uncompressedSize: Int64
  let creationDate: Date
  let modifiedDate: Date
  let accessedDate: Date
  let offset: Int64
  let crc: UInt32
}
