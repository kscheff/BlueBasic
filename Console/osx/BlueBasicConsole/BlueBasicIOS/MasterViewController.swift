//
//  MasterViewController.swift
//  BlueBasicIOS
//
//  Created by tim on 10/3/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import UIKit
import CoreBluetooth

extension Array where Element: Equatable {

 // Remove first collection element that is equal to the given `object`:
 mutating func remove(object: Element) {
     guard let index = firstIndex(of: object) else {return}
     remove(at: index)
 }

}

class MasterViewController: UITableViewController {

  var names = [Device]()
  var lastIndexPath: IndexPath?
  var filter: Bool = true

  var buttonFiltered: UIBarButtonItem? = nil
  var buttonUnfiltered: UIBarButtonItem? = nil
  var buttonFilterBasic: UIBarButtonItem? = nil

  override func awakeFromNib() {
    super.awakeFromNib()
    if UIDevice.current.userInterfaceIdiom == .pad {
        self.clearsSelectionOnViewWillAppear = false
        self.preferredContentSize = CGSize(width: 320.0, height: 600.0)
    }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    self.refreshControl = UIRefreshControl()
    self.refreshControl?.addTarget(self, action: #selector(MasterViewController.pullToRefresh(_:)), for: .valueChanged)
    buttonFiltered = UIBarButtonItem(title: "filtered", style: .plain, target: self, action: #selector(tapFiltered))
    buttonUnfiltered = UIBarButtonItem(title: "unfiltered", style: .plain, target: self, action: #selector(tapUnfiltered))
    buttonFilterBasic = UIBarButtonItem(title: "BASIC#", style: .plain, target: self, action: #selector(tapFilterBasic))
    tapFilterBasic()
    scan()
  }

  override func didReceiveMemoryWarning() {
    super.didReceiveMemoryWarning()
    // Dispose of any resources that can be recreated.
  }
    
  @objc func tapUnfiltered() {
    filter = false
    stop()
    scan(services: Array(myUuids.values))
    navigationItem.rightBarButtonItem = buttonFiltered
  }
  
  @objc func tapFiltered() {
    filter = true
    stop()
    scan(services: nil)
    navigationItem.rightBarButtonItem = buttonFilterBasic
  }
  
  @objc func tapFilterBasic() {
    filter = false
    stop()
    scan(services: nil)
    navigationItem.rightBarButtonItem = buttonUnfiltered
  }
  
  func scan(services: [CBUUID]? = nil) {
    deviceManager.filter = services
    deviceManager.findDevices() {
      device in
      if self.filter, self.identify(device: device) != true {
        self.names.remove(object: device)
        return
      }
      self.aimNotActiveAnymore(device: device)
      if !self.names.contains(device) {
        self.names.append(device)
      }
      self.tableView.reloadData()
    }
  }
  
  func stop() {
    deviceManager.stopScan()
  }
  
  func identify(device: Device) -> Bool {
    if device.name.hasPrefix("BASIC#") ||
       device.name.hasPrefix("BlueBattery") {
      print(device.name, "OK" )
      return true
    }
    print(device.name)
    return false
  }
  
  func resignActive() {
    if let path = lastIndexPath {
      self.tableView.cellForRow(at: path)?.backgroundColor = nil
      lastIndexPath = nil
    }
  }
  
  @objc func pullToRefresh(_ sender: UIRefreshControl) {
    resignActive()
    names.removeAll(keepingCapacity: false)
    tableView.reloadData();
    sender.endRefreshing()
  }
  
  private func aimNotActiveAnymore(device: Device) {
    cancelNotActiveAnymore(device: device)
    self.perform(#selector(self.notActiveAnymore), with: device, afterDelay: 15)
  }
  
  private func cancelNotActiveAnymore(device: Device) {
    NSObject.cancelPreviousPerformRequests(withTarget: self, selector: #selector(self.notActiveAnymore), object: device)
  }
  
  @objc func notActiveAnymore(device: Device) {
    names.remove(object: device)
    tableView.reloadData()
  }
  
  // MARK: - Segues

  override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
    if segue.identifier == "showDetail" {
      if let indexPath = self.tableView.indexPathForSelectedRow {
        if let path = lastIndexPath {
          self.tableView.cellForRow(at: path)?.backgroundColor = nil
          lastIndexPath = nil
        }
        self.tableView.cellForRow(at: indexPath)?.backgroundColor = UIColor.lightGray
        lastIndexPath = indexPath
        let device = names[indexPath.row]
        let controller = (segue.destination as! UINavigationController).topViewController as! DetailViewController
        controller.navigationItem.leftBarButtonItem = self.splitViewController?.displayModeButtonItem
        controller.navigationItem.leftItemsSupplementBackButton = true
        controller.detailItem = device
        popover?.dismiss(animated: true)
      }
    }
  }

  // MARK: - Table View

  override func numberOfSections(in tableView: UITableView) -> Int {
    return 1
  }

  override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    return names.count
  }

  override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    let cell = tableView.dequeueReusableCell(withIdentifier: "Cell", for: indexPath) as UITableViewCell
    cell.textLabel?.text = names[indexPath.row].name
    switch names[indexPath.row].rssi {
    case -41...0:
      cell.imageView?.image = UIImage(named: "5bars")
    case -53...(-42):
      cell.imageView?.image = UIImage(named: "4bars")
    case -65...(-54):
      cell.imageView?.image = UIImage(named: "3bars")
    case -75...(-66):
      cell.imageView?.image = UIImage(named: "2bars")
    case -97...(-76):
      cell.imageView?.image = UIImage(named: "1bars")
    default:
      cell.imageView?.image = UIImage(named: "0bars")
    }
    cell.imageView?.sizeToFit()
    return cell
  }
}

