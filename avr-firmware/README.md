# AVR Firmware

Firmware for the OpenStint transponder, targeting the **ATtiny1616**.

## Prerequisites

- `avr-gcc` toolchain (avr-gcc, avr-objcopy, avr-size)
- `avrdude` for flashing
- **Microchip ATtiny Device Family Pack (DFP)** — required because avr-libc does not ship full support for newer ATtiny 0/1/2-series chips

## Obtaining the ATtiny DFP

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
  gcc/dev/attiny1616/   -- linker scripts and startup objects (-B path)
  include/              -- device header files (-I path)
```

## Building

Set the `ATTINY_DFP` environment variable to point to the root of the extracted DFP directory, then run `make`:

```sh
export ATTINY_DFP=/path/to/Atmel.ATtiny_DFP.2.0.368
make
```

Alternatively, pass it directly to make:

```sh
make ATTINY_DFP=/path/to/Atmel.ATtiny_DFP.2.0.368
```

If `ATTINY_DFP` is not set, it defaults to `../Atmel.ATtiny_DFP.2.0.368` (relative to this directory).

## Flashing

```sh
make flash
```

This uses `avrdude` with an Atmel-ICE programmer over UPDI.

## Fuse Programming

To set the main oscillator to 20 MHz:

```sh
make fuse
```
