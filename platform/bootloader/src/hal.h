/* Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. */
/*
 * The few hardware operations the portable parts of the bootloader need: the
 * pins, SPI1, the buttons and the LED. On the board these are static inline
 * register accesses, so the layer costs nothing; the host test harness
 * (test/native) provides the same names as functions over its models of the
 * card, the panel and the flash.
 *
 * Pins (platform/board/docs/hardware-pinmap.md, variant_CHGame_Rev0.h):
 *   SPI1   SCK PA5, MISO PA6, MOSI PA7 - shared by the panel and the card
 *   panel  CS PA4, DC PB0, RST PB12
 *   card   CS PB11
 *   LED    PB9 (active high), buzzer PB10 (held low)
 *   keys   A PB1, LEFT PB3, UP PB4, B PB6, SELECT PB7, START PB8,
 *          DOWN PC14, RIGHT PC15 - all to GND, internal pull-ups
 *
 * None of MISO, the two chip selects or LCD_RST has a pull-up on the board,
 * so hal_pins_init() drives every one of them before anything else runs.
 */
#ifndef CHBOOT_HAL_H
#define CHBOOT_HAL_H
#include <stdint.h>

/* CHBOOT_APP 1: the dry-run build that runs as a program (build.sh app). */
#ifndef CHBOOT_APP
#define CHBOOT_APP 0
#endif

/* Logical button bits are stable across boards. Rev0's physical PB/PC
 * bits happen to match them; WeAct's PA/PB inputs are translated below. */
#define BTN_A       (1u << 1)
#define BTN_LEFT    (1u << 3)
#define BTN_UP      (1u << 4)
#define BTN_B       (1u << 6)
#define BTN_SELECT  (1u << 7)
#define BTN_START   (1u << 8)
#define BTN_DOWN    (1u << 14)
#define BTN_RIGHT   (1u << 15)
#ifdef CHBOOT_WEACT_F8U6
#define BTN_PORTA   0x0Fu              /* PA0 UP, PA1 B, PA2 START, PA3 SELECT */
#define BTN_PORTB   ((1u << 1) | (1u << 3))  /* PB1 A, PB3 LEFT */
#else
#define BTN_PORTA   0u
#define BTN_PORTB   (BTN_A | BTN_LEFT | BTN_UP | BTN_B | BTN_SELECT | BTN_START)
#endif
#define BTN_PORTC   (BTN_DOWN | BTN_RIGHT)
#define BTN_ALL     (BTN_A | BTN_LEFT | BTN_UP | BTN_B | BTN_SELECT | BTN_START | BTN_DOWN | BTN_RIGHT)

/* SPI1 baud-rate field (CTLR1.BR): SCK = 48 MHz / 2^(BR+1). */
#define SPI_BR_187K5  7u
#define SPI_BR_6M     2u
#define SPI_BR_12M    1u
#define SPI_BR_24M    0u
#define HAL_SPI_16BIT (1u << 11)   /* CTLR1.DFF: 16-bit frames */

#ifdef CHBOOT_HOST

void     hal_pins_init(void);
void     hal_spi_speed(uint32_t br);
uint8_t  hal_spi_xfer(uint8_t b);
void     hal_spi_frames(uint32_t ctl);
void     hal_spi_put16(uint32_t v);
void     hal_sd_select(int on);
void     hal_lcd_select(int on);
void     hal_lcd_dc(int data);
void     hal_lcd_rst(int high);
uint32_t hal_buttons(void);
void     hal_led(int on);
uint32_t hal_uid(uint32_t word);
int      hal_soft_reset(void);
const uint8_t *hal_flash(uint32_t addr);
void     hal_reset(void) __attribute__((noreturn));
#define FLASH_AT(a)   hal_flash(a)

#else

#include "ch32x035.h"

/* Composed pin configuration, one nibble per pin (CNF:MODE).
 *   0x3 push-pull output, 0xB alternate-function push-pull,
 *   0x8 input with pull-up/down (OUTDR selects up), 0x4 floating input.
 * GPIOB/GPIOC CFGHR are WRITE-ONLY on this part (reading returns garbage), so
 * they are only ever written whole, from these constants.
 * PB5 shares pin 16 with BTN_A: it stays a floating input, never an output.
 *
 * Every pin the bootloader does not use is left as reset leaves it, a
 * floating input (high-Z): the buzzer (PB10), PB2, PB5, PB13-15, PC0-13,
 * PA0-3 and PA8-15. A board that puts any of them to another use sees
 * nothing from the bootloader. The USB pins (PC16, PC17) are the USB
 * stack's own (USB_init). The USB-only bootloader (CHBOOT_MENU 0, build.sh
 * nomenu) drives the LED and nothing else: the card's, the panel's and the
 * keys' pins stay high-Z too, for a board without those parts. */
#ifdef CHBOOT_WEACT_F8U6
#define HAL_GPIOA_CFGLR  0xB8B38888u   /* PA0..3 keys, PA4..7 unchanged SPI */
#define HAL_GPIOB_CFGLR  0x44448483u   /* PB0 DC, PB1 A, PB3 LEFT */
#define HAL_GPIOB_CFGHR  0x44433444u   /* PB11 SD_CS, PB12 LCD_RST; no LED/buzzer */
#define HAL_GPIOC_CFGHR  0x88444444u   /* PC14 DOWN, PC15 RIGHT; debug is in CFGXR */
#else
#define HAL_GPIOA_CFGLR  0xB8B34444u   /* PA7 MOSI, PA6 MISO pu, PA5 SCK, PA4 LCD_CS */
#define HAL_GPIOB_CFGLR  0x88488483u   /* PB7 SEL, PB6 B, PB5 -, PB4 UP, PB3 LEFT, PB2 -, PB1 A, PB0 DC */
#define HAL_GPIOB_CFGHR  0x44433438u   /* PB12 LCD_RST, PB11 SD_CS, PB10 -, PB9 LED, PB8 START */
#define HAL_GPIOC_CFGHR  0x88444444u   /* PC15 RIGHT, PC14 DOWN */
#define HAL_GPIOB_CFGHR_LED 0x44444434u /* (USB only) PB9 LED; every other pin as reset left it */
#endif

#ifndef CHBOOT_MENU
#define CHBOOT_MENU 1
#endif

static inline void hal_pins_init(void)
{
#if CHBOOT_MENU
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB
                    | RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
    /* Levels first, so no pin glitches when it becomes an output: both chip
     * selects, LCD_RST and DC high, MISO and the key pull-ups up, LED low. */
    GPIOA->BSHR = (1u << 4) | (1u << 6) | BTN_PORTA;
    GPIOB->BSHR = (1u << 0) | (1u << 11) | (1u << 12) | BTN_PORTB;
#ifndef CHBOOT_WEACT_F8U6
    GPIOB->BCR  = 1u << 9;
#endif
    GPIOC->BSHR = BTN_PORTC;
    GPIOA->CFGLR = HAL_GPIOA_CFGLR;
    GPIOB->CFGLR = HAL_GPIOB_CFGLR;
    GPIOB->CFGHR = HAL_GPIOB_CFGHR;
    GPIOC->CFGHR = HAL_GPIOC_CFGHR;
#elif !defined(CHBOOT_WEACT_F8U6)
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB;
    GPIOB->BCR   = 1u << 9;
    GPIOB->CFGHR = HAL_GPIOB_CFGHR_LED;
#endif
}

/* Mode 0, 8-bit, MSB first, master with software NSS. BR changes only with
 * SPE clear. */
static inline void hal_spi_speed(uint32_t br)
{
    while (SPI1->STATR & SPI_STATR_BSY) { }
    SPI1->CTLR1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSI | SPI_CTLR1_SSM | (br << 3));
    SPI1->CTLR1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSI | SPI_CTLR1_SSM | (br << 3) | SPI_CTLR1_SPE);
}

/* Every byte out is a byte in: always reading DATAR keeps RXNE and OVR from
 * leaving stale bytes for the card reads. */
static inline uint8_t hal_spi_xfer(uint8_t b)
{
    SPI1->DATAR = b;
    while (!(SPI1->STATR & SPI_STATR_RXNE)) { }
    return (uint8_t)SPI1->DATAR;
}

/* SPI1's frames and speed, ctl = (BR << 3) | HAL_SPI_16BIT or not: first the
 * wire runs dry and the unread echo and the overrun are cleared (bytes sent
 * with hal_spi_put16() are never read), so no stale byte reaches a card read.
 * The panel's pixels go as 16-bit frames at 24 MHz, as CHGfx sends them; the
 * card needs 8-bit frames back. */
static inline void hal_spi_frames(uint32_t ctl)
{
    while (!(SPI1->STATR & SPI_STATR_TXE)) { }
    while (SPI1->STATR & SPI_STATR_BSY) { }
    (void)SPI1->DATAR;
    (void)SPI1->STATR;
    SPI1->CTLR1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSI | SPI_CTLR1_SSM | ctl);
    SPI1->CTLR1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSI | SPI_CTLR1_SSM | ctl | SPI_CTLR1_SPE);
}

/* One 16-bit frame, queued: the next waits in the SPI while this one shifts,
 * so the wire, not the CPU, sets the pace. Its echo is never read. */
static inline void hal_spi_put16(uint32_t v)
{
    while (!(SPI1->STATR & SPI_STATR_TXE)) { }
    SPI1->DATAR = (uint16_t)v;
}

static inline void hal_sd_select(int on)  { if (on) GPIOB->BCR = 1u << 11; else GPIOB->BSHR = 1u << 11; }
static inline void hal_lcd_select(int on) { if (on) GPIOA->BCR = 1u << 4;  else GPIOA->BSHR = 1u << 4; }
static inline void hal_lcd_dc(int data)   { if (data) GPIOB->BSHR = 1u << 0; else GPIOB->BCR = 1u << 0; }
static inline void hal_lcd_rst(int high)  { if (high) GPIOB->BSHR = 1u << 12; else GPIOB->BCR = 1u << 12; }
static inline void hal_led(int on)
{
#ifdef CHBOOT_WEACT_F8U6
    (void)on;
#else
    if (on) GPIOB->BSHR = 1u << 9; else GPIOB->BCR = 1u << 9;
#endif
}

/* Pressed keys read low. */
static inline uint32_t hal_buttons(void)
{
#ifdef CHBOOT_WEACT_F8U6
    uint32_t a = ~GPIOA->INDR, b = ~GPIOB->INDR, c = ~GPIOC->INDR;
    uint32_t keys = 0;
    if (a & (1u << 0)) keys |= BTN_UP;
    if (a & (1u << 1)) keys |= BTN_B;
    if (a & (1u << 2)) keys |= BTN_START;
    if (a & (1u << 3)) keys |= BTN_SELECT;
    if (b & (1u << 1)) keys |= BTN_A;
    if (b & (1u << 3)) keys |= BTN_LEFT;
    if (c & (1u << 14)) keys |= BTN_DOWN;
    if (c & (1u << 15)) keys |= BTN_RIGHT;
    return keys;
#else
    return (~GPIOB->INDR & BTN_PORTB) | (~GPIOC->INDR & BTN_PORTC);
#endif
}

#ifndef CH32X035_ESIG_UNIID1
#define CH32X035_ESIG_UNIID1  0x1FFFF7E8u   /* 96-bit unique ID */
#endif
static inline uint32_t hal_uid(uint32_t word)
{
    return ((const volatile uint32_t *)CH32X035_ESIG_UNIID1)[word];
}

/* 1 if this boot follows a reset without loss of power (NVIC_SystemReset: a
   program going back to the menu, an upload request), 0 after power-on.
   Clears the reset flags, so the next boot sees only its own cause. The
   software-reset flag cannot tell the two apart: on the board it is set
   after a power-on as well (test/hil/RESULTS-2026-10-01.md; the factory boot
   code runs first and enters the user flash with a software reset). The
   power-on flag can. */
static inline int hal_soft_reset(void)
{
    uint32_t f = RCC->RSTSCKR;
    RCC->RSTSCKR |= RCC_RMVF;
    return (f & RCC_PORRSTF) == 0;
}

/* A full system reset (PFIC CFGR SYSRESET); SRAM survives it. */
static inline __attribute__((noreturn)) void hal_reset(void)
{
    NVIC->CFGR = NVIC_KEY3 | (1u << 7);
    for (;;) { }
}

#define FLASH_AT(a)   ((const uint8_t *)(uintptr_t)(a))

#endif /* CHBOOT_HOST */

/* An aligned word of flash. */
#define FLASH_W(a)    (*(const uint32_t *)(const void *)FLASH_AT(a))

#endif
