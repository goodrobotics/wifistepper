---
layout: default
title: Crypto Authentication
nav_order: 7
---
# Crypto Authentication

There is an Atmel ECC508a Crypto Authentication chip on the I2C bus for use with the [Low Level Interface](/interfaces/low-level.html). When enabled, commands sent to and from the Wi-Fi Stepper can be authenticated using HMAC signatures against an *Auth Key*. All keys are locked into the EEPROM on-chip on this processor and cannot be read back out.

Two keys are needed to provision the Crypto Chip for use with HMAC: the *Master Key* and the *Auth Key*. The Master Key is permanently written and can never be changed once provisioned. The Auth Key is then locked behind the Master Key and is used with the HMAC signature calculation. This allows the Auth Key to be changed only if the Master Key is known as well. However, only the Auth Key is needed during normal operation to generate the HMAC signatures.

![](/images/ecc508a-layout.png)

To provision the ECC chip, enter the two keys in the settings page. Remember the Master Key as it is permanent.
