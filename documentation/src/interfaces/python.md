---
layout: default
title: Python
parent: Interfaces
nav_order: 3
---
# Python Interface
The python interface is a wrapper around the [Low Level](/interfaces/low-level.html) TCP interface. Through python, you can issue motor control commands and utilize both the standard (non-crypto) and [crypto](/crypto-authentication.html) protocols.

## Standard Protocol Example
```python
# Standard protocol example
import goodrobotics as gr

def error_cb(id, subsystem, type, arg, when):
	print "Error has occurred @"+when+" when executing command with id="+id

MOTOR = gr.WifiStepper(host='wsx100.local', error_callback=error_cb)
MOTOR.connect()

# current motor configuration dict
config = MOTOR.getconfig()

# RPM calculator helper (given an RPM, calculates stepss)
rpm = gr.RPM()

# Spin motor at 30 RPM
MOTOR.run(MOTOR.FORWARD, rpm(30))

# Wait 5 seconds (motor will be spinning)
MOTOR.waitms(5 * 1000)

# Angle calculator helper
angle = gr.Angle(stepsize=config['stepsize'])

# Set and hold position at -90 degrees
MOTOR.goto(angle(-90))

# Wait for motor to reach position
MOTOR.waitbusy()

# Stop motor
MOTOR.stop()
```

## Crypto Protocol Example
```python
import goodrobotics as gr

# The crypto chip needs to be provisioned before this will work
# In addition, the Low-Level Crypto protocol needs to be enabled 
SECUREMOTOR = gr.WifiStepper(host='wsx100.local', key='My_AuthKey!@#')
SECUREMOTOR.connect()

# All the same standard protocol commands are available
# They will now be authenticated through the crypto chip
```
