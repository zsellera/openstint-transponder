# OpenStint transponder reference design, version 2

RC (surface) racing transponder with ATtiny816/1616/3216, featuring:
* OpenStint protocol reference implentation (this is not an RC3 clone!)
* 5 MHz BPSK using SPI MOSI (UART module can send SPI without transmit gaps)
* A two-transistor push-pull coil driver
* Signal level is circa the same as of an RC4
* Works down to 2.8V, output level remains flat down to 3.4V (MiniZ/1S support)
* Reverse polarity protection
* Power line filtering
* LCSC product codes added (orderable from JLCPCB)

**Related projects:** [OpenStint decoder](https://github.com/zsellera/openstint) | [Loop Amplifier](https://github.com/zsellera/openstint-preamp)

JLCPCB manifactures and assembles 5 panels of 2x4s, grand total of 40 pcs, for less than $200, including taxes and shipping (Hungary 27% VAT, no tariff, 2026 May).

<img width="800" alt="openstint transponder v1 reference design" src="https://github.com/user-attachments/assets/bd993deb-2687-4035-adfb-4d545f512d77" />

## Old version

There is a previous version, with lower output level, more stressed components, pickier power requirements, using an STM32 MCU. It's on [v1](https://github.com/zsellera/openstint-transponder/tree/transponder-v2) branch. Production files from [2025 December](https://github.com/zsellera/openstint-transponder/releases/tag/release-2025-12-10).

## Manufacting

See [relases](https://github.com/zsellera/openstint-transponder/releases/tag/nightly-master) for gerber, pos and BOM files. These are directly uploadable to (JLCPCB)[https://jlcpcb.com/].

Order a standard 1.2 mm 4-layer PCB to get the same performace as tested.

```
kikit panelize \
    --layout 'hspace: 3mm; vspace: 3mm; rows: 4; cols: 2' \
    --tabs 'type: fixed; vwidth: 4mm; hwidth: 4mm; vcount: 0' \
    --cuts 'type: mousebites; offset: -0.25mm' \
    --framing 'type: frame; hspace: 3mm; vspace: 3mm; width: 4mm' \
    --post 'millradius: 1.5mm' \
    ...
```

```
kikit fab jlcpcb --assembly --schematic ./openstint-transponder.kicad_sch ./panel/panel-8.kicad_pcb panel/
```
