//
//  DeviceManager.swift
//  BlueBasicConsole
//
//  Created by tim on 9/23/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import Foundation
import CoreBluetooth

class DeviceManager: NSObject, CBCentralManagerDelegate {
  
  var manager: CBCentralManager!
  var scanning = false
  var devices = [UUID: Device]()
  var connectCallbacks = OneTimeCallbacks<Bool>()
  var disconnectCallbacks = OneTimeCallbacks<Bool>()
  var findCallbacks = Callbacks<Device>()
  var filter: [CBUUID]? = nil
  
  override init() {
    super.init()
    manager = CBCentralManager(delegate: self, queue: DispatchQueue.main)
  }
  
  func findDevices(_ onNewDevice: @escaping NewDeviceFoundHandler) {
    _ = findCallbacks.append(onNewDevice)
    startScan();
  }
  
  func startScan() {
    if !scanning {
      scanning = true
      if manager.state == .poweredOn {
        scan()
      }
    }
  }
  
  func stopScan() {
    if scanning {
      scanning = false
      manager.stopScan()
      findCallbacks.removeAll()
    }
  }

  func connect(_ device: Device, onConnected: CompletionHandler?) {
    if !device.isConnected {
      _ = connectCallbacks.append(onConnected)
      manager.connect(device.peripheral, options: nil)
    } else {
      onConnected?(true)
    }
  }
  
  func disconnect(_ device: Device, onDisconnect: CompletionHandler?) {
    if device.isConnected {
      _ = disconnectCallbacks.append(onDisconnect)
      manager.cancelPeripheralConnection(device.peripheral)
    } else {
      onDisconnect?(true)
    }
  }
  
  func centralManagerDidUpdateState(_ central: CBCentralManager) {
    switch (manager.state) {
    case .poweredOn:
      if scanning {
        scan()
      }
    default:
      break
    }
  }

  func scan() {
    manager.scanForPeripherals(withServices: filter, options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
  }

  func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
    //let name = deviceName(peripheral)
    _ = deviceName(peripheral)
    if let device = devices[peripheral.identifier] {
      if (RSSI.int32Value <= 0) {
        device.rssi = RSSI.intValue
      }
    } else {
      let device = Device(peripheral: peripheral, rssi: RSSI.intValue, manager: self)
      devices[peripheral.identifier] = device
    }
    if devices[peripheral.identifier] != nil {
      findCallbacks.call(devices[peripheral.identifier]!)
    }
  }
  
  func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
    if let device = devices[peripheral.identifier] {
      device.isConnected = true
      connectCallbacks.call(true)
    }
  }

  func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
    if devices[peripheral.identifier] != nil {
      connectCallbacks.call(false)
    }
  }
  
  func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
    if let device = devices[peripheral.identifier] {
      device.isConnected = false
      disconnectCallbacks.call(error == nil)
    }
  }
  
  func deviceName(_ peripheral: CBPeripheral) -> String {
    if let n = peripheral.name {
      return n
    } else {
      return "(null)"
    }
  }
}
