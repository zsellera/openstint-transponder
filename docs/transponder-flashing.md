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

## Reading the transponder ID

The transponder ID is not stored anywhere: the firmware calculates it at boot from the chip's factory serial number (`crc32(SIGROW.SERNUM0..9) % 10000000`). The serial number is readable over UPDI, so [tools/transponder_id.py](https://github.com/zsellera/openstint-transponder/blob/master/tools/transponder_id.py) can tell you the ID a board will transmit, over the same connection you flash with. No decoder or RF power-up needed. It is read-only and works on a blank chip too.

It needs Python 3 and avrdude 7.x or newer (for the `sernum` memory), with a `serialupdi` programmer:

```
$ python tools/transponder_id.py --port COM5
SERNUM : 2F510C111E4277A305BB
ID     : 4729780
```

When building a batch, add `--csv` and `--label` to collect a labelling list as you go. Each run appends a `timestamp;label;sernum;transponder_id` row:

```
python tools/transponder_id.py --port COM5 --csv transponders.csv --label 07
```

Run `python tools/transponder_id.py --help` for the rest of the options (`--baud`, `--part`, `--avrdude`).
