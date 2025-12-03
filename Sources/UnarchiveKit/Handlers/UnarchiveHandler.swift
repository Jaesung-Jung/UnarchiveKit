//
//  UnarchiveHandler.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/2/25.
//

import Foundation

protocol UnarchiveHandler: Actor, Sendable {
  nonisolated var url: URL { get }

  func entries() throws -> [UnarchiveEntry]
  func extract(_ entry: UnarchiveEntry, upToCount count: Int?) throws -> Data
}
