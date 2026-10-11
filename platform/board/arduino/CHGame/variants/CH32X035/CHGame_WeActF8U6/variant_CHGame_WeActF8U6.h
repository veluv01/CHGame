/* Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. */
/* CHGame on WeAct CH32X035F8U6 (QFN20). See docs/weact-f8u6.md.
 * Eight switches to GND, ST7735S and microSD on SPI1. No buzzer or external
 * status LED. PB12's onboard LED shares LCD reset; do not blink it.
 * PC16/PC17 remain USB, PC18/PC19 remain WCH two-wire SDI debug.
 */
#pragma once

#define ADC_MODULE_ENABLED
#define SPI_MODULE_ENABLED

#define PA0 PIN_A0
#define PA1 PIN_A1
#define PA2 PIN_A2
#define PA3 PIN_A3
#define PA4 PIN_A4
#define PA5 PIN_A5
#define PA6 PIN_A6
#define PA7 PIN_A7
#define PB0 PIN_A8
#define PB1 PIN_A9
#define PB3 10
#define PB11 11
#define PB12 12
#define PC14 13
#define PC15 14
#define PC16 15
#define PC17 16
#define PC18 17
#define PC19 18

#define PA0_ALT1 (PA0 | ALT1)
#define PA1_ALT1 (PA1 | ALT1)
#define PA2_ALT1 (PA2 | ALT1)
#define PA3_ALT1 (PA3 | ALT1)
#define PA4_ALT1 (PA4 | ALT1)
#define PA5_ALT1 (PA5 | ALT1)
#define PA6_ALT1 (PA6 | ALT1)
#define PA7_ALT1 (PA7 | ALT1)
#define PB0_ALT1 (PB0 | ALT1)
#define PB1_ALT1 (PB1 | ALT1)

#define NUM_DIGITAL_PINS 19
#define NUM_ANALOG_INPUTS 10
#define ADC_RESOLUTION 12

#define LED_BUILTIN PNUM_NOT_DEFINED
#define PIN_LED PNUM_NOT_DEFINED
#define PIN_BUZZER PNUM_NOT_DEFINED
#define USER_BTN PNUM_NOT_DEFINED

#define PIN_BTN_UP PA0
#define PIN_BTN_B PA1
#define PIN_BTN_START PA2
#define PIN_BTN_SELECT PA3
#define PIN_BTN_A PB1
#define PIN_BTN_LEFT PB3
#define PIN_BTN_DOWN PC14
#define PIN_BTN_RIGHT PC15

#define PIN_LCD_CS PA4
#define PIN_LCD_DC PB0
#define PIN_LCD_RST PB12
#define PIN_SD_CS PB11
#define PIN_SPI_SS PA4
#define PIN_SPI_SCK PA5
#define PIN_SPI_MISO PA6
#define PIN_SPI_MOSI PA7

#define PIN_WIRE_SDA PNUM_NOT_DEFINED
#define PIN_WIRE_SCL PNUM_NOT_DEFINED
#define PIN_SERIAL_RX PNUM_NOT_DEFINED
#define PIN_SERIAL_TX PNUM_NOT_DEFINED
#define SERIAL_UART_INSTANCE 1
#define PIN_GPIO1 PNUM_NOT_DEFINED
#define PIN_GPIO2 PNUM_NOT_DEFINED
#define PIN_GPIO3 PNUM_NOT_DEFINED
#define PIN_GPIO4 PNUM_NOT_DEFINED

/* Header compatibility only: the hardware timer module is disabled. */
#define TIMER_TONE TIM3
#define TIMER_SERVO TIM2
#ifdef __cplusplus
#define SERIAL_PORT_MONITOR Serial
#define SERIAL_PORT_USBVIRTUAL Serial
#endif
