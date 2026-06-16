# AVR Firmware

Firmware for the OpenStint transponder, targeting the **ATtiny816**/**ATtiny1616**/**ATtiny3216**.

Not tested with 416, but I think it would work there as well. Not measured, but I guesstimate the stack size upper limit to be around 80-90 bytes.

## Precompiled firmware

An automated job compiles the firmware, check out [releases](https://github.com/zsellera/openstint-transponder/releases/tag/nightly-master).

## Prerequisites

- `avr-gcc` toolchain (avr-gcc, avr-objcopy, avr-size)
- `avrdude` for flashing
- **Microchip ATtiny Device Family Pack (DFP)** — required because avr-libc does not ship full support for newer ATtiny 0/1/2-series chips

## Building

The project generates a Makefile with CMake, downloading AVR support packages meanwhile.

```sh
mkdir build
cd build
cmake ..
make
```

Alternatives:

```sh
# v1 revision (led on different pin)
cmake -B build -DHW_REV=v1

# manual DFP path
cmake -B build -DATTINY_DFP=/path/to/Atmel.ATtiny_DFP.2.0.368
```

### Obtaining the ATtiny DFP manually

The new CMake script downloads this automatically

The DFP provides the device-specific headers, linker scripts, and startup objects needed by avr-gcc.

1. Go to the Microchip Packs Repository: <http://packs.download.atmel.com/>
2. Search for **ATtiny** and download the latest **Atmel ATtiny Series Device Support** pack (e.g. `Atmel.ATtiny_DFP.2.0.368.atpack`)
3. The `.atpack` file is a regular ZIP archive — extract it to a directory of your choice:
   ```sh
   unzip Atmel.ATtiny_DFP.2.0.368.atpack -d Atmel.ATtiny_DFP.2.0.368
   ```

The extracted directory structure contains:
```
Atmel.ATtiny_DFP.2.0.368/
  gcc/dev/attiny816/   -- linker scripts and startup objects (-B path)
  gcc/dev/attiny1616/   -- linker scripts and startup objects (-B path)
  include/              -- device header files (-I path)
```

## Flashing

```sh
make flash
```

This uses `avrdude` with an Atmel-ICE programmer over UPDI.

## Fuse Programming

FOR PAST/FUTURE REFERENCE ONLY, THIS STEP IS NOT REQUIRED!

ATtinys have an internal oscillator, which - by default - run at 16 MHz. A fuse programming
is required to make it 20 MHz. However it's pointless - the RC oscillator is so unprecise we can not calibratie it to be within 5 MHz ±19 kHz reliably (which is needed for openstint decoder to detect it).

To set the main oscillator to 20 MHz:

```sh
make fuse
```
