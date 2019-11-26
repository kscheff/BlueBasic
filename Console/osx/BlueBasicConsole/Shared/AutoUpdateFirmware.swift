//
//  AutoUpdateFirmware.swift
//  BlueBasicConsole
//
//  Created by tim on 10/8/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import Foundation
import CoreBluetooth

var _firmwareVersion: String?
var _firmwareBlob: Data?

class AutoUpdateFirmware {
  
//  let baseURL = "https://github.com/aanon4/BlueBasic/raw/master/hex/BlueBasic-"
  let baseURL = "https://github.com/kscheff/BlueBasic/raw/master/hex/BlueBasic-"

  let console: ConsoleProtocol
  let device: Device
  
  init(console: ConsoleProtocol) {
    self.console = console
    self.device = console.current!
  }
  
  func detectUpgrade(_ onComplete: @escaping CompletionHandler) {
    device.services() {
      list in
      let revision = list[UUIDS.deviceInfoServiceUUID]!.characteristics[UUIDS.firmwareRevisionUUID]!
      self.device.read(revision) {
        data in
        if data == nil {
          onComplete(false)
        } else {
          self.fetchLatestVersion(String(data: data!, encoding: String.Encoding.ascii)!) {
            success in
            DispatchQueue.main.async {
              onComplete(success)
            }
          }
        }
      }
    }
  }
  
  func upgrade(_ onComplete: CompletionHandler? = nil) {
    if _firmwareBlob == nil {
      onComplete?(false)
    } else {
      Firmware(console).upgrade(_firmwareBlob!, onComplete: onComplete)
    }
  }
  
  func fetchLatestVersion(_ currentVersion: String, onComplete: @escaping CompletionHandler) {
    if _firmwareVersion == currentVersion {
      onComplete(false)
    } else if _firmwareVersion != nil {
      onComplete(_firmwareBlob != nil)
    } else {
      let session = URLSession(configuration: URLSessionConfiguration.default)

      let versionParts = currentVersion.components(separatedBy: "/")
      (session.dataTask(with: URL(string: baseURL + versionParts[0] + ".version")!, completionHandler: {
        data, response, error in
        if error != nil || data == nil || data!.count < 14 {
          onComplete(false)
        } else {
          let versionInfo = NSString(data: data!, encoding: String.Encoding.ascii.rawValue)!.substring(to: 14)
          if versionInfo <= versionParts[1] {
            _firmwareVersion = currentVersion
            onComplete(false)
          } else {
            (session.dataTask(with: URL(string: self.baseURL + versionParts[0] + ".bin")!, completionHandler: {
              data, response, error in
              if error == nil && data != nil {
                _firmwareBlob = data
                _firmwareVersion = "\(versionParts[0])/\(versionInfo)"
              }
              onComplete(error == nil)
            }) ).resume()
          }
        }
      }) ).resume()
    }
  }
}

