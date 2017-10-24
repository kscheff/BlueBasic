BlueBasic
=========

NOTE: Tim has no longer access to either hardware or tools to update the builds.
Therefore this forked updates a few not working things.

Main differences:
- BlueBasic Console supports now OSX 10.12 Sierra and iOS 10 build with xCode 8.1
- update to IAR 9.30.3 compiler settings
- extended parameter to support ANALOG REFERENCE, AVDD
- bugfixes for memory corruption (important for data logging applications)
- added math exp() function
- fixed writing & reading arrays from flash
- update to newest BLE 1.4.2.2 stack 
- added I2C Slave read for CC2541 chip

BASIC interpreter for CC2540 and CC2541 Bluetooth LE chips.

This project contains a BASIC interpreter which can be flashed onto a CC2540 or CC2541 Bluetooth module. Once installed, simple use the Bluetooth Console tool to connect and start coding on the device using good old BASIC.

The project was inspired by experimenting with the HM-10 modules (a cheap BLE module) and a need to provide an easy way to prototype ideas (rather than coding in C using the very expensive IAR compiler). Hopefully other will find this useful.

For original information see https://github.com/aanon4/BlueBasic/wiki/Blue-Basic:-An-Introduction
