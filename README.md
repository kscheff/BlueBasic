BlueBasic
=========

NOTE: Tim has no longer access to either hardware or tools to update the builds.
Therefore this fork implements fixes and improvements. The interpreter is used in low volume production devices.

BASIC interpreter for CC2540 and CC2541 Bluetooth LE chips.

Major differences since Tim left:
- BlueBasic Console supports now OSX 10.15 Catalina and iOS 14 build with xCode 12
- update to IAR 10.30.1 compiler settings
- extended parameter to support ANALOG REFERENCE, AVDD
- bugfixes for memory corruption (important for data logging applications)
- added math POW, TEMP functions
- fixed corrupted flashstore compacting  
- update to newest BLE 1.5.0.16 / 1.5.1.1 stack 
- added I2C Slave read for CC2541 chip (-Steca build)
- new memory layout includes BASIC program embedded in ImgB
- increased flashstore from 4 to 8 and 10 (-BBX) pages
- multitasking with adaptive time slicing to keep BLE connection alive
- file OPEN supports modulo and record pointer positioning e.g. for ring log buffering
- support for persistant SNV files "0" to "9" surviving OAD updates
- TIMER based ADC sampling with optional RMS calulation, supports differential analog input
- support for 2nd UART
- numerouse other fixes and improvements

This project contains a BASIC interpreter which can be flashed onto a CC2540 or CC2541 Bluetooth module. Once installed, simple use the Bluetooth Console tool to connect and start coding on the device using good old BASIC.

The project was inspired by experimenting with the HM-10 modules (a cheap BLE module) and a need to provide an easy way to prototype ideas (rather than coding in C using the very expensive IAR compiler). Hopefully other will find this useful.

For original information see https://github.com/aanon4/BlueBasic/wiki/Blue-Basic:-An-Introduction
