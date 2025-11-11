#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <stm32c0xx_ll_bus.h>
#include <stm32c0xx_ll_system.h>
#include <stm32c0xx_ll_gpio.h>
#include <stm32c0xx_ll_cortex.h>
#include <stm32c0xx_ll_rcc.h>
#include <stm32c0xx_ll_utils.h>
#include <stm32c0xx_ll_spi.h>
#include <stm32c0xx_ll_tim.h>

#define	V29POLYA	0x1af
#define	V29POLYB	0x11d

#define LED_PORT    GPIOA
#define LED_PIN     LL_GPIO_PIN_0

volatile uint16_t systime = 0;
void TIM14_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM14))
    {
        LL_TIM_ClearFlag_UPDATE(TIM14);   // Clear interrupt flag    
        systime++;
        // LL_GPIO_TogglePin(LED_PORT, LED_PIN);
    }
}

void initHSE();
void initGPIO();
void initTIM14();
void initSPI();

static const uint8_t bpsk_m = 0xaa; // BPSK -1+0j symbol
static const uint8_t bpsk_p = 0x55; // BPSK +1+0j symbol

// message to send:
static uint8_t transponder_message[104];  // updated once on startup
static uint8_t timesync_message[104];     // updated before each send

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
    unsigned int i, j, b, mask, key8=~0;
    unsigned int poly = 0xe0;
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

void init_message(uint8_t* dst) {
	// init seq:
	dst[0] = bpsk_m;
	dst[1] = bpsk_m;
	dst[2] = bpsk_m;
	dst[3] = bpsk_m;
	// preamble (barker13 + 000)
	dst[4] = bpsk_p; // b15
	dst[5] = bpsk_p; // b14
	dst[6] = bpsk_p; // b13
	dst[7] = bpsk_p; // b12
	dst[8] = bpsk_p; // b11
	dst[9] = bpsk_m; // b10
	dst[10] = bpsk_m; // b09
	dst[11] = bpsk_p; // b08
	dst[12] = bpsk_p; // b07
	dst[13] = bpsk_m; // b06
	dst[14] = bpsk_p; // b05
	dst[15] = bpsk_m; // b04
	dst[16] = bpsk_p; // b03
	dst[17] = bpsk_m; // b02
	dst[18] = bpsk_m; // b01
	dst[19] = bpsk_m; // b00
	// trailing seq:
	dst[100] = bpsk_m;
	dst[101] = bpsk_m;
	dst[102] = bpsk_m;
	dst[103] = bpsk_m;
}

static inline int parity4(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x &= 0xF;
    return (0x6996 >> x) & 1;
}

void encode_word(uint8_t* dst, uint32_t msg) {
	// NOTE: bit operations happen on registers, which are big-endian
	uint32_t shreg = 0;
	//int prev_bit = 1; // differential-encoding, preamble ends with +1
	for (int i=0; i<32; ++i) {
		// append bit to shift register
		int msg_bit = (msg >> (31-i)) & 1;
		shreg <<= 1;
		shreg |= msg_bit;
		// encode with poly-A
		dst[20 + 2*i + 0] = parity4(shreg & V29POLYA) ? bpsk_p : bpsk_m;
		dst[20 + 2*i + 1] = parity4(shreg & V29POLYB) ? bpsk_p : bpsk_m;
	}
	// trailing 0x00
	for (int i=0; i<8; ++i) {
		// shift in a zero
		shreg <<= 1;
		shreg &= 0xffe;
		dst[20 + 64 + 2*i + 0] = parity4(shreg & V29POLYA) ? bpsk_p : bpsk_m;
		dst[20 + 64 + 2*i + 1] = parity4(shreg & V29POLYB) ? bpsk_p : bpsk_m;
	}
}

uint32_t add_checksum(uint32_t msg) {
	uint8_t bytes[3] = { (msg >> 16) & 0xFF, (msg >> 8) & 0xFF, msg & 0xFF };
	uint8_t crc = crc8_generate_key(bytes, 3);
	return ((msg & 0xffffff) << 8) | crc;
}



int main(void)
{
    initHSE();
    initGPIO();
    initTIM14();
    initSPI();

    // setup transponder id:
    uint32_t id[3] = { *((uint32_t *)UID_BASE), *((uint32_t *)(UID_BASE+4)), *((uint32_t *)(UID_BASE+8)) };
    uint32_t transponder_id = crc32_calc(id, sizeof(id)) % 10000000U;
    uint32_t msg = add_checksum(transponder_id);
    init_message(transponder_message);
    encode_word(transponder_message, msg);

    // initialize empty timesync message (to be updated later)
    init_message(timesync_message);
    
    uint16_t message_counter = 0;
    uint16_t cnt = LL_TIM_GetCounter(TIM14);
    uint8_t *transmit_buffer = transponder_message;
    while(1) {
        __disable_irq();
        for (int i=0; i<104; i++) {
		    while (!(SPI1->SR & SPI_SR_TXE));  // wait until TX buffer empty
            *((__IO uint8_t *)&SPI1->DR) = transmit_buffer[i];
        }
        __enable_irq();
        while (SPI1->SR & SPI_SR_BSY);

        LL_GPIO_SetOutputPin(LED_PORT, LED_PIN);

        uint16_t deadline = (uint16_t)(cnt + 10u + rand() % 10);
        
        message_counter = (message_counter + 1) % 667; // reset counter ca. 1s
        if (message_counter == 0) { // next message will be the timesync
            LL_GPIO_ResetOutputPin(LED_PORT, LED_PIN); // blink once a second

            uint32_t deadline_systime = (deadline > cnt) ? systime : (systime+1);
            uint32_t timecode = ((deadline_systime << 16) & 0xffff0000) | deadline;
            timecode &= 0x000fffff;
            timecode |= 0x00A00000;
            uint32_t timecode_msg = add_checksum(timecode);
            encode_word(timesync_message, timecode_msg);
            transmit_buffer = timesync_message;
        } else {
            transmit_buffer = transponder_message;
        }

        while (cnt != deadline) { cnt = LL_TIM_GetCounter(TIM14); };
    }

    return 0;
}

void initGPIO()
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);

    // LED
    LL_GPIO_SetPinMode(LED_PORT, LED_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(LED_PORT, LED_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    // Configure PA2 as Alternate Function 0 (SPI1)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_0);  // AF0 = SPI1
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_2, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_2, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_NO);
}

void initSPI(void)
{
    // 1. Enable peripheral clock for SPI1
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1); // For STM32C0xx

    // 2. Disable SPI before configuration
    LL_SPI_Disable(SPI1);

    // 3. Configure SPI parameters
    LL_SPI_SetMode(SPI1, LL_SPI_MODE_MASTER);
    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
    LL_SPI_SetTransferBitOrder(SPI1, LL_SPI_MSB_FIRST);
    LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV2);
    LL_SPI_SetTransferDirection(SPI1, LL_SPI_FULL_DUPLEX);  // same as your default
    LL_SPI_SetDataWidth(SPI1, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetNSSMode(SPI1, LL_SPI_NSS_SOFT); // SSM + SSI handled automatically
    LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER); // FRXTH equivalent

    // 4. Enable SPI
    LL_SPI_Enable(SPI1);
}

void initTIM14(void)
{
    // 1. Enable TIM14 clock
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM14);

    // 2. Disable timer
    LL_TIM_DisableCounter(TIM14);

    // 3. Set prescaler and auto-reload
    LL_TIM_SetPrescaler(TIM14, 1999);      // 20 MHz / (1999+1) = 10 kHz
    LL_TIM_SetAutoReload(TIM14, 0xFFFF);   // Max 16-bit value

    // 4. Enable update interrupt
    LL_TIM_EnableIT_UPDATE(TIM14);

    // 5. Enable counter
    LL_TIM_EnableCounter(TIM14);

    // 6. Configure NVIC for TIM14 IRQ
    NVIC_SetPriority(TIM14_IRQn, 0);
    NVIC_EnableIRQ(TIM14_IRQn);
}

void initHSE()
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0); // 0 wait states is fine for 20 MHz
    LL_RCC_HSE_Enable();
    while (LL_RCC_HSE_IsReady() != 1) {}

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSE);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSE) {}

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    LL_SetSystemCoreClock(20000000);
    // SysTick_Config(SystemCoreClock / 1000);
    
    // Disable SysTick interrupt
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    NVIC_DisableIRQ(SysTick_IRQn);
}
