---
layout: default
title: High Voltage Operation
nav_order: 4
---
# High Voltage Operation (50 Volts and Above)
With proper configuration, the Wi-Fi Stepper can handle up to 80 volts input. Board modification is needed when using 50 volts and higher. This is due to two factors: 1) Input ratings on the barrel connector and 2) the maximum input voltage of the DC-DC regulator. When operating at 50+ volts, please ensure the following:

1. Use *only* the screw terminals for voltage input. (The barrel connector is not rated for more than 50 volts)
2. Cut jumper **JP0** (located mid-board next to DC-DC regulator) and provide 3.3 volt a regulated supply separately to one of the inputs labeled **3V3** and **GND** on either side of the board.

**Special Note:** This board needs the *Vin* input supply to be present prior to the 3.3v logic supply being present. Under this modification, please ensure that the logic supply voltage is **only** active when *Vin* is active as well.

## Heat Sink and Cooling Fans
The Wi-Fi Stepper comes with a standard **Heat Sink** that under typical operation should be affixed to the *powerSTEP01* main chip (center board). If your application uses **high current**, the standard heat sink may not be sufficient in all cases. Monitor the temperature of both the *powerSTEP01* and the *current sense resistors* adjacent to determine if active cooling (fans) or a larger heat sink is needed. It is recommended that a fan be used if the operational current exceeds 7 amps RMS regularly.
