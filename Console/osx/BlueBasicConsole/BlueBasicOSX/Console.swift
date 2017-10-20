//
//  Console.swift
//  BlueBasicConsole
//
//  Created by tim on 9/23/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import Foundation
import Cocoa
import CoreBluetooth

class Console: NSObject, NSTextViewDelegate, DeviceDelegate, ConsoleProtocol {
  
  let statusField: NSTextField
  let console: NSTextView
  var current: Device?
  
  var inputCharacteristic: CBCharacteristic?
  var outputCharacteristic: CBCharacteristic?
  var pending = ""
  var recoveryMode = false
  var wrote = 0
  var written = 0
  var reboot = -1
  
  var delegate: ConsoleDelegate?

  init(console: NSTextView, status: NSTextField) {
    self.statusField = status
    self.console = console
    self.status = "Not connected"
    super.init()
    console.isAutomaticQuoteSubstitutionEnabled = false
    console.font = NSFont(name: "Menlo Regular", size: 11)
    console.delegate = self
  }

  var status: String {
    didSet {
      statusField.stringValue = status
      if isConnected && !isRecoveryMode {
        console.isEditable = true
        console.window?.makeFirstResponder(console)
      } else {
        console.isEditable = false
      }
    }
  }
  
  // Workaround
  @nonobjc
  func setStatus(_ status: String) {
    self.status = status
  }
  
  // Workaround
  func setDelegate(_ delegate: ConsoleDelegate) {
    self.delegate = delegate
  }
  
  var isConnected: Bool {
    get {
      return status == "Connected" || status == "Upgrade available"
    }
  }
  
  var isRecoveryMode: Bool {
    get {
      return recoveryMode
    }
  }
  
  func connectTo(_ device: Device, onConnected: CompletionHandler? = nil) {
    disconnect() {
      success in
      self.status = "Connecting..."
      self.current = device
      device.connect() {
        success in
        if success {
          device.services() {
            list in
            if list[UUIDS.commsServiceUUID] != nil {
              self.inputCharacteristic = list[UUIDS.commsServiceUUID]!.characteristics[UUIDS.inputCharacteristicUUID]
              self.outputCharacteristic = list[UUIDS.commsServiceUUID]!.characteristics[UUIDS.outputCharacteristicUUID]
              self.current!.read(self.inputCharacteristic!) {
                data in
                if data == nil {
                  if list[UUIDS.oadServiceUUID] != nil {
                    self.current!.delegate = self
                    self.recoveryMode = true
                    self.status = "Recovery mode"
                    onConnected?(true)
                  } else {
                    self.status = "Failed"
                    self.disconnect()
                    onConnected?(false)
                  }
                } else {
                  self.status = "Connected"
                  self.current!.delegate = self
                  self.current!.notify(UUIDS.inputCharacteristicUUID, serviceUUID: UUIDS.commsServiceUUID)
                  onConnected?(true)
                }
              }
            } else if list[UUIDS.oadServiceUUID] != nil {
              self.current!.delegate = self
              self.recoveryMode = true
              self.status = "Recovery mode"
              onConnected?(true)
            } else {
              self.status = "Unsupported"
              self.disconnect()
              onConnected?(false)
            }
          }
        } else {
          self.status = "Failed"
          onConnected?(false)
        }
      }
    }
  }
  
  func onWriteComplete(_ success: Bool, uuid: CBUUID) {
    written += 1
    if wrote > written {
      status = String(format: "Sending...%d%%", 100 * written / wrote)
    } else {
      status = "Connected"
      wrote = 0
      written = 0
    }
    delegate?.onWriteComplete(uuid)
    if reboot == written {
      self.append("\nREBOOT\ndisconnecting from console...\n")
      perform(#selector(disconnect), with: nil, afterDelay: 0.1)
      reboot = -1
      wrote = 0
      written = 0
    }
  }
  
  func onNotification(_ success: Bool, uuid: CBUUID, data: Data) {
    switch uuid {
    case UUIDS.inputCharacteristicUUID:
      print (String(data: data, encoding: .ascii))
      if delegate == nil || delegate!.onNotification(uuid, data: data) {
        let str = NSString(data: data, encoding: String.Encoding.ascii.rawValue)!
        console.replaceCharacters(in: NSMakeRange(console.string!.utf16.count, 0), with: str as String)
        console.scrollRangeToVisible(NSMakeRange(console.string!.utf16.count, 0))
        console.needsDisplay = true
      }
    default:
      break
    }
  }
  
  func onDisconnect() {
    if let old = current {
      old.connect()
    }
  }

  func disconnect(_ onDisconnect: CompletionHandler? = nil) {
    if let old = current {
      current = nil
      delegate = nil
      status = "Not connected"
      recoveryMode = false
      old.delegate = nil
      old.disconnect(onDisconnect)
    } else {
      onDisconnect?(true)
    }
  }
  
  func write(_ str: String = "\n") -> Bool{
    for ch in str.characters {
      pending.append(ch)
      if ch == "\n" || pending.utf16.count > 19 {
        if let buf = pending.data(using: String.Encoding.ascii, allowLossyConversion: false) {
          current!.write(buf, characteristic: outputCharacteristic!, type: .withResponse)
          wrote += 1
          if "reboot\n" == pending.lowercased() {
            // check if there is there are writes pending
            if wrote > 1 {
              reboot = wrote - 1 // schedule reboot one buffered write earlier
            } else {
              // in interactive mode we execute the disconnect
              self.append("disconnecting from console...\n")
              perform(#selector(disconnect), with: nil, afterDelay: 0.1)
              wrote = 0
              written = 0
              reboot = -1
            }
          }
        } else {
          let alert = NSAlert()
          alert.messageText = "Could not write \"\(pending)\". Only ASCII characters could be written to the console."
          alert.runModal()
          wrote = 0
          written = 0
          pending = ""
          return false
        }
        pending = ""
      }
    }
    return true
  }
  
  func append(_ str: String) {
    console.textStorage?.append(NSAttributedString(string: str))
    console.scrollToEndOfDocument(nil)
  }
  
  func textView(_ textView: NSTextView, shouldChangeTextIn affectedCharRange: NSRange, replacementString: String?) -> Bool {
    let consoleCount = console.string!.utf16.count
    if current == nil {
      return false
    } else if replacementString!.utf16.count > 0 {
      if write(replacementString!) {
        if affectedCharRange.location == consoleCount {
          return true
        } else {
          textView.replaceCharacters(in: NSMakeRange(consoleCount, 0), with: replacementString!)
          textView.setSelectedRange(NSMakeRange(console.string!.utf16.count, 0))
          return false
        }
      } else {
        return false
      }
    } else if affectedCharRange.location == consoleCount - 1 && pending.utf16.count > 0 {
      pending.remove(at: pending.characters.index(before: pending.endIndex))
      return true
    } else {
      return false
    }
  }

}


