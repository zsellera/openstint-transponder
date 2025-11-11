# OpenStint transponder reference design

JLCPCB manifactures and assembles 5 panels of 2x4s, grand total of 40 pcs, for $130, including taxes and shipping.

<img width="800" alt="openstint transponder v1 reference design" src="https://github.com/user-attachments/assets/bd993deb-2687-4035-adfb-4d545f512d77" />


## Manufacting

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
