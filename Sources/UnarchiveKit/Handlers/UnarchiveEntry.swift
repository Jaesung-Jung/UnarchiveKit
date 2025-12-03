//
//  UnarchiveEntry.swift
//  UnarchiveKit
//
//  Created by 정재성 on 12/3/25.
//

enum UnarchiveEntry: Sendable, Hashable {
  case zip(ZIPEntry)
}
