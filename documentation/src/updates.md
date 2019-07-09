---
layout: default
title: Updates &amp; Downloads
nav_order: 3
---
# Updates & Downloads

Visit the firmware download page for **[WSX100 boards](/downloads/wsx100/check.html)** to download the latest firmware.

## Bug reporting
To submit bug reports and view issues, visit the GitHub [Wi-Fi Stepper Project Page](https://github.com/goodrobotics/wifistepper).

## Failsafe update
Due to space limitations on the device, firmware updating is done progressively, one section at a time. Starting with the core software and ending with the peripheral assets (webpages and images). If an error occurs during update, there is a possibility of leaving peripheral assets in an inconsistent state. Built into the core software portion (first section updated) is a fail-safe update mechanism that can be used to bootstrap the update without the need for the peripheral assets.

During an update failure, **if you see warning of possible firmware corruption**, retry the firmware updating using the failsafe update mechanism.

**The failsafe update webpage is located** `http://192.168.4.1/update/recovery` 
