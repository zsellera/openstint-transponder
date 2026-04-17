MCU=attiny1616
F_CPU=10000000UL
CC=avr-gcc
ATTINY_DFP ?= ../Atmel.ATtiny_DFP.2.0.368
CFLAGS=-mmcu=$(MCU) -O2 -fstack-usage -DF_CPU=$(F_CPU) -B $(ATTINY_DFP)/gcc/dev/attiny1616 -I $(ATTINY_DFP)/include
OBJCOPY=avr-objcopy

all: main.hex

main.elf: main.c
	$(CC) $(CFLAGS) -o $@ $<

main.hex: main.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	avr-size --mcu=$(MCU) --format=avr $<

flash: main.hex
	avrdude -c atmelice_updi -p $(MCU) -U flash:w:$<

# this sets the main oscillator to 20 MHz:
fuse:
	avrdude -c atmelice_updi -p attiny1616 -U fuse1:w:0x02:m

clean:
	rm -f *.elf *.hex
