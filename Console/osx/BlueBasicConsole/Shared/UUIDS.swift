//
//  Uuids.swift
//  BlueBasicConsole
//
//  Created by tim on 9/25/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import Foundation
import CoreBluetooth

struct UUIDS {
  
  static let commsServiceUUID = myUuids["commsService"]!
  static let inputCharacteristicUUID = myUuids["inputCharacteristic"]!
  static let outputCharacteristicUUID = myUuids["outputCharacteristic"]!
  
  static let oadServiceUUID = myUuids["oadService"]!
  static let imgIdentityUUID = myUuids["imgIdentity"]!
  static let imgBlockUUID = myUuids["imgBlock"]!
  
  static let deviceInfoServiceUUID = myUuids["deviceInfoService"]!
  static let firmwareRevisionUUID = myUuids["firmwareRevision"]!
  static let systemIdUUID = myUuids["systemId"]!
  static let batteryServiceUUID = myUuids["batteryService"]!
  
}

let myUuids = [
  
  "commsService" : CBUUID(string: "25FB9E91-1616-448D-B5A3-F70A64BDA73A"),
  "inputCharacteristic" : CBUUID(string: "C3FBC9E2-676B-9FB5-3749-2F471DCF07B2"),
  "outputCharacteristic" : CBUUID(string: "D6AF9B3C-FE92-1CB2-F74B-7AFB7DE57E6D"),
  "oadService" : CBUUID(string: "F000FFC0-0451-4000-B000-000000000000"),
  "imgIdentity" : CBUUID(string: "F000FFC1-0451-4000-B000-000000000000"),
  "imgBlock" : CBUUID(string: "F000FFC2-0451-4000-B000-000000000000"),
  "deviceInfoService" : CBUUID(string: "180A"),
  "firmwareRevision" : CBUUID(string: "2A26"),
  "systemId" : CBUUID(string: "2A23"),
  "batteryService" : CBUUID(string: "AA021474-780D-439F-AF20-6B46446A610E")
  
]
