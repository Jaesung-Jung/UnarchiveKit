//
//  UnarchiverError.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/2/25.
//

public enum UnarchiverError: Error, Sendable {
  case fileAccessFailed(underlying: Error)
  case unsupportedFormat

  case archiveOpenFailed(path: String)
  case archiveIterationFailed

  case invalidEntryContext
  case entryNotFound
  case extractFailed(path: String)
}
