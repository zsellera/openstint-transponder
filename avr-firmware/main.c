#define F_CPU 10000000UL  // Match your external CMOS clock frequency
#define MCU "attiny1616"

#define FRAME_LEN 12

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>
#include <avr/fuse.h>
#include <avr/sleep.h>

#include <stddef.h>
#include <string.h>

FUSES = {
    .OSCCFG = 0x2,  // 0x02 = 20 MHz
};

#define	V29POLYA	0x1af
#define	V29POLYB	0x11d

uint32_t crc32_update(uint32_t crc, uint8_t data)
{
    crc ^= data;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320U & (-(int)(crc & 1)));
    return crc;
}

uint32_t crc32_calc(const void *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;
    while (len--)
        crc = crc32_update(crc, *p++);
    return ~crc;
}

uint8_t crc8_generate_key(uint8_t *_msg, size_t len)
{
    uint32_t i, j, b, mask, key8=~0, poly = 0xe0;
    for (i=0; i<len; i++) {
        b = _msg[i];
        key8 ^= b;
        for (j=0; j<8; j++) {
            mask = -(key8 & 1);
            key8 = (key8>>1) ^ (poly & mask);
        }
    }
    return (~key8) & 0xff;
}

static inline uint8_t parity32(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x &= 0xF;
    x = (0x6996 >> x) & 1;
    return (uint8_t) x;
}

void encode_transponder_msg(uint8_t* dst, uint8_t msg[4]) {
    // this function requires a zeroed buffer
    memset(dst, 0, 10);

    // init with zeroed register (convention)
	uint32_t shreg = 0;
    // conv encode:
    for (int k=0; k<4; k++) {
        for (int i=0; i<4; ++i) {
            // append bit to shift register
            int msg_bit = (msg[k] >> (7-i)) & 1;
            shreg <<= 1;
            shreg |= msg_bit;
            uint8_t x = (parity32(shreg & V29POLYA) << 1) | parity32(shreg & V29POLYB);
            dst[2*k] |= (x << (3-i)*2);
        }
        for (int i=4; i<8; ++i) {
            // append bit to shift register
            int msg_bit = (msg[k] >> (7-i)) & 1;
            shreg <<= 1;
            shreg |= msg_bit;
            uint8_t x = (parity32(shreg & V29POLYA) << 1) | parity32(shreg & V29POLYB);
            dst[2*k+1] |= (x << (7-i)*2);
        }
    }
	// trailing 0x00
	for (int i=0; i<4; ++i) {
		// shift in a zero
		shreg <<= 1;
		shreg &= 0xffe;
		uint8_t x = (parity32(shreg & V29POLYA) << 1) | parity32(shreg & V29POLYB);
        dst[8] |= (x << (3-i)*2);
	}
    for (int i=4; i<8; ++i) {
		// shift in a zero
		shreg <<= 1;
		shreg &= 0xffe;
		uint8_t x = (parity32(shreg & V29POLYA) << 1) | parity32(shreg & V29POLYB);
        dst[9] |= (x << (7-i)*2);
	}
}

void encode_payload(uint8_t* dst, uint32_t payload24) {
    // create transponder message (id + crc8):
    uint8_t message[4];
    message[0] = (uint8_t)((payload24 >> 16) & 0xFF);
    message[1] = (uint8_t)((payload24 >> 8) & 0xFF);
    message[2] = (uint8_t)(payload24 & 0xFF);
    message[3] = crc8_generate_key(message, 3);

    // convolutional encoder:
    encode_transponder_msg(dst, message);
}

static volatile uint32_t _ticks;  // 10 kHz free-running counter (100 us per tick)

static uint32_t ticks_read(void)
{
    cli(); // do not increment tick while reading
    uint32_t t = _ticks;
    sei();
    return t;
}

ISR(TCB0_INT_vect) {
    _ticks++;
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

static uint16_t lfsr = 0xACE1;

static uint8_t rand8(void)
{
    uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
    lfsr = (lfsr >> 1) | (bit << 15);
    return (uint8_t)lfsr;
}

void transmit_frame(const uint8_t *frame)
{
    const uint8_t *fp = frame;
    cli();
    __asm__ __volatile__ (
        // Enable CCL
        "lds r16, %[cclctrla]    \n\t"  // 3 cyc
        "ori r16, %[cclen]       \n\t"  // 1 cyc
        "sts %[cclctrla], r16    \n\t"  // 2 cyc

        // Init sequence: 4 symbols @ 1.25 MHz = 3.2 us = 32 cycles
        // From STS above to first USART write:
        //   ldi r18 (1) + ldi r16 (1) + delay loop (3N-1)
        //   + lds (3) + sbrs-skip (2) + ld (2) + sts (2) = 10+3N
        // Total from CCL enable (STS, 2 cyc) = 2+1+1+3N-1+10 = 13+3N
        // Need 32 cycles: N = (32-13)/3 = 6.33 => N=6 (30 cyc) or N=7 (33 cyc)
        "ldi r18, %[len]         \n\t"  // 1 cyc - frame byte counter
        "ldi r16, 7              \n\t"  // 1 cyc - delay counter
    "1:                          \n\t"
        "dec r16                 \n\t"  // 1 cyc
        "brne 1b                 \n\t"  // 2 taken / 1 not taken

        // Transmit frame bytes
    "2:                          \n\t"
        "lds r16, %[status]      \n\t"  // 3 cyc - USART0.STATUS
        "sbrs r16, 5             \n\t"  // 1/2 cyc - skip if DREIF set
        "rjmp 2b                 \n\t"  // 2 cyc
        "ld r17, Z+              \n\t"  // 2 cyc - frame[i++]
        "sts %[txdata], r17      \n\t"  // 2 cyc - USART0.TXDATAL
        "dec r18                 \n\t"  // 1 cyc
        "brne 2b                 \n\t"  // 2/1 cyc

        // Wait for TX complete
    "3:                          \n\t"
        "lds r16, %[status]      \n\t"  // USART0.STATUS
        "sbrs r16, 6             \n\t"  // skip if TXCIF set
        "rjmp 3b                 \n\t"
        "ldi r16, %[txcif]       \n\t"
        "sts %[status], r16      \n\t"  // clear TXCIF

        // Tail sequence: ~4 symbols @ 1.25 MHz = 32 cycles
        // CCL still enabled, carrier runs as unmodulated tail
        // Overhead to CCL disable: lds (3) + andi (1) + sts (2) = 6 cyc
        // Budget: 32 - 6 = 26 cyc => ldi (1) + loop 3*9-1 = 26 => N=9
        "ldi r16, 9              \n\t"  // 1 cyc
    "4:                          \n\t"
        "dec r16                 \n\t"  // 1 cyc
        "brne 4b                 \n\t"  // 2 taken / 1 not taken

        // Disable CCL
        "lds r16, %[cclctrla]    \n\t"
        "andi r16, %[ccldis]     \n\t"
        "sts %[cclctrla], r16    \n\t"

        : "+z" (fp)
        : [status]   "i" (&USART0.STATUS),
          [txdata]   "i" (&USART0.TXDATAL),
          [cclctrla] "i" (&CCL.CTRLA),
          [txcif]    "M" (USART_TXCIF_bm),
          [len]      "M" (FRAME_LEN),
          [cclen]    "M" (CCL_ENABLE_bm),
          [ccldis]   "n" (~CCL_ENABLE_bm)
        : "r16", "r17", "r18", "memory"
    );
    sei();
}

int main(void)
{
    // PA5 LED — one small blink when timecode it sent
    PORTA.DIRSET = PIN5_bm;

    // External CMOS clock on CLKI pin; update F_CPU to match your clock source
    PORTA.OUTCLR = PIN5_bm; // led off
    ccp_write_io((void *)&CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_EXTCLK_gc);
    ccp_write_io((void *)&CLKCTRL.MCLKCTRLB, 0);  // No prescaler
    while (CLKCTRL.MCLKSTATUS & CLKCTRL_SOSC_bm);
    // if clock fails to start, this never goes off:
    PORTA.OUTSET = PIN5_bm;

    // setup timer for a 10 kHz internal tick
    TCB0.CCMP    = 499;   // 10 MHz / 2 / (499+1) = 10 kHz
    TCB0.CTRLA   = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;

    // enable sleep mode, peripherals remain active:
    SLPCTRL.CTRLA = SLPCTRL_SMODE_IDLE_gc | SLPCTRL_SEN_bm;
    sei();

    // UART in Master SPI Mode
    PORTB.DIRSET |= PIN2_bm | PIN1_bm;  // TXD (MOSI) and XCK (SCK) as outputs
    USART0.BAUD = 256;                  // Max baud rate: F_CPU / 2 = 5 MHz
    USART0.CTRLC = USART_CMODE_MSPI_gc | USART_CHSIZE_8BIT_gc;
    USART0.CTRLB = USART_TXEN_bm;

    // TCA (timer generate 5 MHz square, this gets modulated by CCL)
    PORTB.DIRSET |= PIN0_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_FRQ_gc
                      | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CMP0 = 0;
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;

    // Configurable Custom Logic (modulate: SPI_MOSI ^ TCA)
    PORTA.DIRSET |= PIN4_bm | PIN7_bm; // Configure LUT output pins as outputs
    CCL.CTRLA &= ~CCL_ENABLE_bm; // Make sure CCL is disabled before touching LUT registers
    // LUT0: MOSI ^ TCA
    CCL.LUT0CTRLB = CCL_INSEL1_USART0_gc | CCL_INSEL0_TCA0_gc;
    CCL.LUT0CTRLC = CCL_INSEL2_MASK_gc;
    CCL.TRUTH0 = 0x99;
    CCL.LUT0CTRLA = CCL_OUTEN_bm | CCL_ENABLE_bm | CCL_FILTSEL_SYNCH_gc;
    // LUT1: ~(MOSI ^ TCA)
    CCL.LUT1CTRLB = CCL_INSEL1_USART0_gc | CCL_INSEL0_TCA0_gc;
    CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc;
    CCL.TRUTH1 = 0x66;
    CCL.LUT1CTRLA = CCL_OUTEN_bm | CCL_ENABLE_bm | CCL_FILTSEL_SYNCH_gc;
    
    // setup transponder id:
    uint8_t id[10] = {
        SIGROW.SERNUM0, SIGROW.SERNUM1, SIGROW.SERNUM2,
        SIGROW.SERNUM3, SIGROW.SERNUM4, SIGROW.SERNUM5,
        SIGROW.SERNUM6, SIGROW.SERNUM7, SIGROW.SERNUM8,
        SIGROW.SERNUM9
    };
    uint32_t transponder_id = crc32_calc(id, sizeof(id)) % 10000000U;

    // construct transponder frame (preamble + encoded payload)
    uint8_t transponder_frame[12] = {0};
    transponder_frame[0] = 0xf9;
    transponder_frame[1] = 0xa8;
    encode_payload(transponder_frame + 2, transponder_id);

    // timesync frame; payload will be encoded later
    uint8_t timesync_frame[12];
    timesync_frame[0] = 0xf9;
    timesync_frame[1] = 0xa8;

    uint16_t message_counter = 0;
    uint8_t *tx_frame = transponder_frame; // timesync vs transponder frame
    while (1)
    {
        transmit_frame(tx_frame);

        // Wait 0.8..2.2 ms (avg 1.5 ms, ±700 us jitter)
        // 8 + rand8()%15 ticks at 10 kHz = 0.8-2.2 ms
        uint32_t deadline = ticks_read() + 8 + (rand8() % 15);

        // Count messages. Every 667th message is 
        message_counter = (message_counter + 1) % 667;
        if (message_counter == 0) { // next is the timesync
            
            
            // construct timecode message
            // deadline is actually the timecode when the next message is due
            uint32_t timecode = (deadline & 0x000fffff) | 0x00a00000;
            encode_payload(timesync_frame + 2, timecode);

            // re-wire tx_frame
            tx_frame = timesync_frame;
            PORTA.OUTCLR = PIN5_bm;  // LED on
        } else {
            tx_frame = transponder_frame;
            PORTA.OUTSET = PIN5_bm; // led off
        }
        
        while (ticks_read() < deadline) {
            sleep_cpu();
        }
    }
}
