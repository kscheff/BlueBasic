//
//  DetailViewController.swift
//  BlueBasicIOS
//
//  Created by tim on 10/3/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import UIKit
import CoreBluetooth

var _current: Device?
var _currentOwner: UIViewController?

class DetailViewController: UIViewController, UITextViewDelegate, DeviceDelegate, ConsoleProtocol {

  @IBOutlet weak var console: UITextView!
  @IBOutlet weak var constrainBottom: NSLayoutConstraint!
  
  var inputCharacteristic: CBCharacteristic?
  var outputCharacteristic: CBCharacteristic?
  var pending = ""
  var linebuffer = ""
  
  var keyboardOpen: CGFloat? = nil
  
  var delegate: ConsoleDelegate?
  
  var autoUpgrade: AutoUpdateFirmware?
  var recoveryMode = false
  var wrote = 0
  var written = 0
  var reboot = -1
  
  var detailItem: AnyObject? {
    didSet {
      connectTo(detailItem as! Device) {
        success in
        if success {
          self.autoUpgrade = AutoUpdateFirmware(console: self)
          self.autoUpgrade!.detectUpgrade() {
            needupgrade in
            if needupgrade {
              self.status = "Upgrade available"
              self.navigationItem.rightBarButtonItem = self.buttonUpgrade
            } else {
              self.autoUpgrade = nil
              self.navigationItem.rightBarButtonItem = self.buttonRun
            }
          }

        }
      }
    }
  }
  
  var current: Device? {
    get {
      return _current
    }
    set {
      _currentOwner = self
      _current = newValue
    }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    console.dataDetectorTypes = UIDataDetectorTypes()
    console.delegate = self
    console.layoutManager.allowsNonContiguousLayout = false // Fix scroll jump when keyboard dismissed
    self.navigationItem.title = status
  }
  
  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    NotificationCenter.default.addObserver(self, selector: #selector(DetailViewController.keyboardDidShow(_:)), name: UIResponder.keyboardDidShowNotification, object: nil)
    NotificationCenter.default.addObserver(self, selector: #selector(DetailViewController.keyboardWillHide(_:)), name: UIResponder.keyboardWillHideNotification, object: nil)
  }
  
  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    if detailItem == nil {
      popover?.present(from: (view.window!.rootViewController as! UISplitViewController).displayModeButtonItem, permittedArrowDirections: .any, animated: true)
    }
  }

  override func viewWillDisappear(_ animated: Bool) {
    super.viewWillDisappear(animated)
    NotificationCenter.default.removeObserver(self, name: UIResponder.keyboardDidShowNotification, object: nil)
    NotificationCenter.default.removeObserver(self, name: UIResponder.keyboardWillHideNotification, object: nil)
    resignActive()
  }

  override func didReceiveMemoryWarning() {
    super.didReceiveMemoryWarning()
  }
  
  // MARK: - Short cuts
  
  let buttonUpgrade = UIBarButtonItem(title: "Upgrade", style: .plain, target: self, action: #selector(DetailViewController.upgrade))
  
  let buttonRun = UIBarButtonItem(title: "Run", style: .plain, target: self, action: #selector(DetailViewController.run))
  
  let buttonStop = UIBarButtonItem(title: "Stop", style: .plain, target: self, action: #selector(DetailViewController.stop))
  
  @objc func run() {
    _ = write("run\n")
    navigationItem.rightBarButtonItem = buttonStop
  }
  
  @objc func stop() {
    _ = write("1\n")  // there is no stop command, so we delete line 1
    navigationItem.rightBarButtonItem = buttonRun
  }
  
  // MARK: - Console mechanics
  
  var status: String = "Not connected" {
    didSet {
      self.navigationItem.title = status
      if isConnected && !isRecoveryMode {
        console?.isEditable = true
        console.becomeFirstResponder()
      } else {
        console?.isEditable = false
      }
    }
  }
  
  var connectTitle: String {
    get {
      return current?.name != nil ? "Connected \(current!.name)" : "Connected"
    }
  }

  var isConnected: Bool {
    get {
      return status.hasPrefix("Connected") || status == "Upgrade available"
    }
  }
  
  var isRecoveryMode: Bool {
    get {
      return recoveryMode
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
              if self.current == nil {
                self.status = "Failed"
                onConnected?(false)
              } else {
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
                    self.status = self.connectTitle
                    self.current!.delegate = self
                    self.current!.notify(UUIDS.inputCharacteristicUUID, serviceUUID: UUIDS.commsServiceUUID)
                    onConnected?(true)
                  }
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
      status = connectTitle
      wrote = 0
      written = 0
    }
    delegate?.onWriteComplete(uuid)
    if reboot == written {
      console.insertText("\nREBOOT\ndisconnecting from console...\n")
      perform(#selector(disconnect), with: nil, afterDelay: 0.1)
      reboot = -1
      wrote = 0
      written = 0
    }
  }
  
  func onNotification(_ success: Bool, uuid: CBUUID, data: Data) {
    switch uuid {
    case UUIDS.inputCharacteristicUUID:
      if delegate == nil || delegate!.onNotification(uuid, data: data) {
        let str = NSString(data: data, encoding: String.Encoding.ascii.rawValue)!
        console.selectedRange = NSMakeRange(console.text!.utf16.count, 0)
        console.insertText(str as String)
        console.scrollRangeToVisible(NSMakeRange(console.text!.utf16.count, 0))
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
  
  @objc func disconnect(_ onDisconnect: CompletionHandler? = nil) {
    if let old = current {
      current = nil
      delegate = nil
      status = "Not connected"
      recoveryMode = false
      old.delegate = nil
      old.disconnect() {
        success in
        DispatchQueue.main.asyncAfter(deadline: DispatchTime.now() + Double(1_000_000_000) / Double(NSEC_PER_SEC)) {
          if onDisconnect != nil {
            onDisconnect!(success)
          }
        }
      }
    } else {
      onDisconnect?(true)
    }
  }
  
  func write(_ text: String = "\n") -> Bool {
    
    // since write() might get called with single characaters or entire text or anything in between
    // first we filter the input to throw away comment lines, in case an entire text comes in
    // likely a copy/paste action
    var textArray: [String]  = []
    for line in text.components(separatedBy: "\n") {
      if !line.hasPrefix("//") {
        textArray.append(line)
      }
    }
    let str = textArray.joined(separator: "\n")
    
    for ch in str {
      pending.append(ch)
      if ch == "\n" || pending.utf16.count > 19 {
        if let buf = pending.data(using: String.Encoding.ascii, allowLossyConversion: false) {
          current!.write(buf, characteristic: outputCharacteristic!, type: .withResponse)
          wrote += 1
          // check for the reboot command (a bit unsafe, since it could at line end and 20 byte boundery)
          if pending.lowercased() == "reboot\n" {
            // check if there is there are writes pending
            if wrote > 1 {
              reboot = wrote - 1 // schedule reboot one buffered write earlier
            } else {
              // in interactive mode we execute the disconnect
              console.insertText("\ndisconnecting from console...\n")
              perform(#selector(disconnect), with: nil, afterDelay: 0.1)
              wrote = 0
              written = 0
              reboot = -1
            }
          }
        } else {
          console.insertText("\nOnly ASCII characters, try again.\n")
          wrote = 0
          written = 0
          return false
        }
        pending = ""
      }
    }
    return true
  }
  
  func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool {
    if current == nil {
      return false
    } else if text.utf16.count > 0 {
      if write(text) {
        if range.location == console.text.utf16.count {
          return true
        } else {
          console.selectedRange = NSMakeRange(console.text!.utf16.count, 0)
          console.insertText(text)
          console.scrollRangeToVisible(NSMakeRange(console.text.utf16.count, 0))
          return false
        }
      } else {
        return false
      }
    } else if range.location == console.text.utf16.count - 1 && pending.utf16.count > 0 {
      pending.remove(at: pending.index(before: pending.endIndex))
      return true
    } else {
      return false
    }
  }
  
  func resignActive() {
    if _currentOwner == self {
      disconnect()
    }
  }

  @objc func upgrade() {
    if autoUpgrade != nil {
      let alert = UIAlertController(title: "Upgrade?", message: "Do you want to upgrade the device firmware?", preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: "OK", style: .destructive, handler: {
        action in
        self.navigationItem.rightBarButtonItem = nil
        UIApplication.shared.isIdleTimerDisabled = true
        self.autoUpgrade!.upgrade() {
          success in
          UIApplication.shared.isIdleTimerDisabled = false
        }
      }))
      alert.addAction(UIAlertAction(title: "Cancel", style: .default, handler: {
        action in
      }))
      self.present(alert, animated: true, completion: nil)
    }
  }

  
  @objc func keyboardDidShow(_ notification: Notification) {
    if keyboardOpen == nil {
      let kbInfo = (notification.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as! NSValue).cgRectValue
      keyboardOpen = constrainBottom.constant
      let adjustHeight =  console.frame.origin.y + console.frame.size.height - kbInfo.origin.y
      constrainBottom.constant += adjustHeight
      console.selectedRange = NSMakeRange(console.text.count, 0)
      console.scrollRangeToVisible(NSMakeRange(console.text.count, 0))
    }
  }
  
  @objc func keyboardWillHide(_ notification: Notification) {
    if keyboardOpen != nil {
      constrainBottom.constant = keyboardOpen!
      self.keyboardOpen = nil
      self.console.scrollRangeToVisible(NSMakeRange(self.console.text.utf16.count, 0))
    }
  }

}
