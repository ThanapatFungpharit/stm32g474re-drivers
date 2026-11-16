#ifndef STM32G474RE_BOARD_NUCLEO_G474RE_H
#define STM32G474RE_BOARD_NUCLEO_G474RE_H

/**
 * @file nucleo_g474re.h
 * @brief Pin assignments and helpers for the NUCLEO-G474RE board (MB1367).
 *
 * | Function            | Pin  | Notes                                       |
 * |---------------------|------|---------------------------------------------|
 * | LD2 green LED       | PA5  | High = on. Shared with DAC1_OUT2 / D13.     |
 * | B1 user button      | PC13 | Active high; needs the internal pull-down.  |
 * | VCP TX (to ST-LINK) | PA2  | LPUART1_TX, AF12.                           |
 * | VCP RX (from ST-LINK)| PA3 | LPUART1_RX, AF12.                           |
 * | SWO                 | PB3  | ITM console output to the ST-LINK.          |
 * | HSE                 | PF0/PF1 | 24 MHz crystal (not used by the clock presets). |
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "drivers/uart/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_LED_PORT      GPIOA
#define BOARD_LED_PIN       5U

#define BOARD_BUTTON_PORT   GPIOC
#define BOARD_BUTTON_PIN    13U
#define BOARD_BUTTON_IRQn   EXTI15_10_IRQn

#define BOARD_VCP_UART      LPUART1
#define BOARD_VCP_IRQn      LPUART1_IRQn
#define BOARD_VCP_TX_PORT   GPIOA
#define BOARD_VCP_TX_PIN    2U
#define BOARD_VCP_RX_PORT   GPIOA
#define BOARD_VCP_RX_PIN    3U
#define BOARD_VCP_AF        12U

/** @brief Configure LD2 as an output, initially off. */
DrvStatus board_led_init(void);

/** @brief Switch LD2 on (true) or off. */
void board_led_set(bool on);

/** @brief Invert LD2. */
void board_led_toggle(void);

/** @brief Configure B1 as an input with pull-down. */
DrvStatus board_button_init(void);

/** @brief Return true while B1 is held down. */
bool board_button_pressed(void);

/**
 * @brief Configure PA2/PA3 and LPUART1 for the ST-LINK virtual COM port.
 * @param uart Handle to initialise.
 * @param baud_rate Baud rate (8N1, no flow control, PCLK1 kernel clock).
 * @return Result of uart_init().
 */
DrvStatus board_vcp_init(UartHandle *uart, uint32_t baud_rate);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_BOARD_NUCLEO_G474RE_H */
