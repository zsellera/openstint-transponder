---
title: Flashing OpenStint Transponder Firmware
description: How to flash the ATtiny-based OpenStint RC lap timing transponder firmware using an Atmel-ICE or a cheap UPDI programmer.
---

# Flashing the firmware

There is an automated job that compiles the firmware, check out [releases](https://github.com/zsellera/openstint-transponder/releases/tag/nightly-master). You'll need the `.hex` file. You can [compile yourself](https://github.com/zsellera/openstint-transponder/blob/master/avr-firmware/README.md), of course.

To flash the firmware, I use an [ATMEL-ICE](https://www.microchip.com/en-us/development-tool/atatmel-ice), sourceable from Mouser/DigiKey/etc. (~$100).

A much cheaper alternative is [Adafruit High Voltage UPDI Friend - USB Serial UPDI Programmer](https://www.adafruit.com/product/5893) (~$10).

## Hardware

There are test points on the bottom of the transponder:
* **VDD** powers the LDO, apply 4+ V here
* **VTG** is a testpoint to check the LDO output voltage (should be 3.3V)
* **GND**
* **UPDI** is the programming pin

You can connect +3.3V directly to `VTG`, as the LDO has reverse current protection, and not use `VDD` (leave floating).

The v2 transponder's test point spacing is 2.54 mm / 0.1" away from each other. You need pogo pins to make proper contact. [Adafruit](https://www.adafruit.com/product/5381), [AliExpress](https://www.aliexpress.com/item/4000452882780.html).

## Software

I use [avrdude](https://github.com/avrdudes/avrdude/), the makescript use it as well.

Program both the fuses and the flash memory! The fuses are set to use the 20 MHz internal RC oscillator as a fallback if the crystal oscillator fails, as a fallback mechanism.

This does both:
```
make flash
```

OR:
```
avrdude -c serialupdi -p attiny816 -U "flash:w:main.hex" -U fuse1:w:0x02:m
```
