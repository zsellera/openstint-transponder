#define F_CPU 10000000UL  // Match your external CMOS clock frequency
#define MCU "attiny1616"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>
#include <avr/fuse.h>

#include <util/delay.h>

#include <stddef.h>

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
	// NOTE: bit operations happen on registers, which are big-endian
	uint32_t shreg = 0;
    for (int k=0; k<4; k++) {
        for (int i=0; i<4; ++i) {
            // append bit to shift register
            int msg_bit = (msg[k] >> (7-i)) & 1;
            shreg <<= 1;
            shreg |= msg_bit;
            // differential encode
            uint8_t x = (parity32(shreg & V29POLYA) << 1) | parity32(shreg & V29POLYB);
            dst[2*k] |= (x << (3-i)*2);
        }
        for (int i=4; i<8; ++i) {
            // append bit to shift register
            int msg_bit = (msg[k] >> (7-i)) & 1;
            shreg <<= 1;
            shreg |= msg_bit;
            // differential encode
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

static volatile uint16_t _ms;

ISR(TCB0_INT_vect) {
    if (++_ms >= 500) {
        _ms = 0;
        PORTA.OUTTGL = PIN5_bm;
    }
    TCB0.INTFLAGS = TCB_CAPT_bm;
}

int main(void)
{
    // External CMOS clock on CLKI pin; update F_CPU to match your clock source
    ccp_write_io((void *)&CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_EXTCLK_gc);
    ccp_write_io((void *)&CLKCTRL.MCLKCTRLB, 0);  // No prescaler

    // Internal 16/20 MHz oscillator (write fuse)
    // ccp_write_io((void *)&CLKCTRL.MCLKCTRLA, CLKCTRL_CLKSEL_OSC20M_gc);
    // ccp_write_io((void *)&CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);

    while (CLKCTRL.MCLKSTATUS & CLKCTRL_SOSC_bm);

    // PA5 LED — 1Hz blink via TCB0 (1ms tick, toggle every 500ms)
    PORTA.DIRSET = PIN5_bm;
    TCB0.CCMP    = 4999;  // F_CPU/(CCMP+1)
    TCB0.CTRLA   = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
    sei();

    // while (1);

    // SPI
    PORTMUX.CTRLB |= PORTMUX_SPI0_bm;
    PORTC.DIRSET = PIN0_bm | PIN2_bm;  // MOSI; SCK
    SPI0.CTRLB = SPI_SSD_bm; // | SPI_BUFEN_bm;
    SPI0.CTRLA = SPI_MASTER_bm
               | SPI_CLK2X_bm
               | SPI_PRESC_DIV16_gc
               | SPI_ENABLE_bm;
    
    // UART in Master SPI Mode
    PORTB.DIRSET |= PIN2_bm | PIN1_bm;  // TXD (MOSI) and XCK (SCK) as outputs
    USART0.BAUD = 256;                  // Max baud rate: F_CPU / 2 = 5 MHz
    USART0.CTRLC = USART_CMODE_MSPI_gc | USART_CHSIZE_8BIT_gc;
    USART0.CTRLB = USART_TXEN_bm;


    // TCA
    PORTB.DIRSET |= PIN0_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_FRQ_gc
                      | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CMP0 = 0;
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;

    // LUT
    PORTA.DIRSET |= PIN4_bm | PIN7_bm; // Configure LUT output pins as outputs
    CCL.CTRLA &= ~CCL_ENABLE_bm; // Make sure CCL is disabled before touching LUT registers

    /* Configure both LUTs */
    CCL.LUT0CTRLB = CCL_INSEL1_USART0_gc | CCL_INSEL0_TCA0_gc;
    CCL.LUT0CTRLC = CCL_INSEL2_MASK_gc;
    CCL.TRUTH0 = 0x99;
    CCL.LUT0CTRLA = CCL_OUTEN_bm | CCL_ENABLE_bm | CCL_FILTSEL_SYNCH_gc;

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

    // create transponder message (id + crc8):
    uint8_t transponder_message[4] = {0};
    transponder_message[0] = (uint8_t)((transponder_id >> 16) & 0xFF);
    transponder_message[1] = (uint8_t)((transponder_id >> 8) & 0xFF);
    transponder_message[2] = (uint8_t)(transponder_id & 0xFF);
    transponder_message[3] = crc8_generate_key(&transponder_message[0], 3);

    // construct frame (preamble + encoded payload)
    uint8_t frame[11] = {0};
    frame[0] = 0xf9;
    frame[1] = 0xa8;
    encode_transponder_msg(frame + 2, transponder_message);

    while (1)
    {
        // transmit_frame(frame);
        CCL.CTRLA |= CCL_ENABLE_bm;
        __asm__ __volatile__ (
            // first byte sent outside the loop:
            "ldi r16, 20         \n\t"
            "wait_loop0:         \n\t"
            "dec r16             \n\t"  // 1 cycle
            "brne wait_loop0     \n\t"  // 2 cycles (1 when not taken)
            :
            :
            : "r16"        // Clobbered registers
        );
        for (uint8_t i = 0; i < sizeof(frame); i++) {
            while (!(USART0.STATUS & USART_DREIF_bm));
            USART0.TXDATAL = frame[i];
        }
        while (!(USART0.STATUS & USART_TXCIF_bm));
        USART0.STATUS = USART_TXCIF_bm;  // clear flag
        CCL.CTRLA &= ~CCL_ENABLE_bm;

        _delay_ms(1);
    }
}
