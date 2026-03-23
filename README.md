# OpenStint transponder reference design

RC (surface) racing transponder with STM32C011, featuring:
* OpenStint protocol reference implentation (this is not an RC3 clone!)
* 5 MHz BPSK using SPI MOSI
* MOSFET driver repurposed as a high-current source for the antenna coil
* Tuned antenna, signal level is ca. 50% of the commercial deal when measured with a smallish near-field H-probe.
* Reverse polarity protection
* Power line filtering (TODO: LISN measurements....)
* LCSC product codes added.

**Related projects:** [OpenStint decoder with HackRF One](https://github.com/zsellera/openstint) | [Loop Amplifier](https://github.com/zsellera/openstint-preamp)

JLCPCB manifactures and assembles 5 panels of 2x4s, grand total of 40 pcs, for $130, including taxes and shipping.

<img width="800" alt="openstint transponder v1 reference design" src="https://github.com/user-attachments/assets/bd993deb-2687-4035-adfb-4d545f512d77" />

## Manufacting

See [relases](https://github.com/zsellera/openstint-transponder/releases/) for gerber, pos and BOM files. These are directly uploadable to (JLCPCB)[https://jlcpcb.com/]. As of now (2025-12-10), manufacturing 5 panels (40 transponders) with assembly costs $160, while 20 panels (160 pcs of transponders) cost $300 (prices with shipping and taxes to EU).

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
