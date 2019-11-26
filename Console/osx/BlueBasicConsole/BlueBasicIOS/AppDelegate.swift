//
//  AppDelegate.swift
//  BlueBasicIOS
//
//  Created by tim on 10/3/14.
//  Copyright (c) 2014 tim. All rights reserved.
//

import UIKit

let deviceManager = DeviceManager()
var popover: UIPopoverController? = nil

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate, UISplitViewControllerDelegate {
  
  var window: UIWindow?

  func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
    let splitViewController = self.window!.rootViewController as! UISplitViewController
    let navigationController = splitViewController.viewControllers[splitViewController.viewControllers.count-1] as! UINavigationController
    navigationController.topViewController?.navigationItem.leftBarButtonItem = splitViewController.displayModeButtonItem
    splitViewController.delegate = self

    return true
  }

  func applicationWillResignActive(_ application: UIApplication) {
    let splitViewController = self.window!.rootViewController as! UISplitViewController
    for view in splitViewController.viewControllers {
      let controller = (view as! UINavigationController).topViewController
      if let master = controller as? MasterViewController {
        master.resignActive()
      }
//      else if let detail = controller as? DetailViewController {
//        detail.resignActive()
//      }
//      else if let nav = controller as? UINavigationController {
//        if let detail = nav.topViewController as? DetailViewController {
//          detail.resignActive()
//        }
//      }
    }
  }

  func applicationDidEnterBackground(_ application: UIApplication) {
  }

  func applicationWillEnterForeground(_ application: UIApplication) {
  }

  func applicationDidBecomeActive(_ application: UIApplication) {
  }

  func applicationWillTerminate(_ application: UIApplication) {
  }

  // MARK: - Split view

  func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController:UIViewController, onto primaryViewController:UIViewController) -> Bool {
      if let secondaryAsNavController = secondaryViewController as? UINavigationController {
          if let topAsDetailController = secondaryAsNavController.topViewController as? DetailViewController {
              if topAsDetailController.detailItem == nil {
                  // Return true to indicate that we have handled the collapse by doing nothing; the secondary controller will be discarded.
                  return true
              }
          }
      }
      return false
  }
  
  func splitViewController(_ svc: UISplitViewController, willHide aViewController: UIViewController, with barButtonItem: UIBarButtonItem, for pc: UIPopoverController) {
    popover = pc
  }

}

