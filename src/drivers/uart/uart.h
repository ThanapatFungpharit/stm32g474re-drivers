#ifndef STM32G474RE_DRIVERS_UART_H
#define STM32G474RE_DRIVERS_UART_H

/**
 * @file uart.h
 * @brief Asynchronous serial driver for USART1-3, UART4-5 and LPUART1.
 *
 * Features: 7/8 data bits, none/even/odd parity, 1/2 stop bits, optional
 * RTS/CTS flow control, selectable kernel clock, automatic prescaler /
 * oversampling selection with baud-error checking, blocking transfers with
 * timeouts, line-error reporting, and optional interrupt-driven ring buffers
 * for receive and/or transmit.
 *
 * Instances, kernel clocks and some pins (see the datasheet AF table for all):
 *
 * | Instance | Bus clock | IRQ          | TX pin (AF)  | RX pin (AF)  |
 * |----------|-----------|--------------|--------------|--------------|
 * | USART1   | PCLK2     | USART1_IRQn  | PC4 (7), PA9 (7) | PC5 (7), PA10 (7) |
 * | USART2   | PCLK1     | USART2_IRQn  | PA2 (7), PB3 (7) | PA3 (7), PB4 (7)  |
 * | USART3   | PCLK1     | USART3_IRQn  | PB10 (7), PC10 (7) | PB11 (7), PC11 (7) |
 * | UART4    | PCLK1     | UART4_IRQn   | PC10 (5)     | PC11 (5)     |
 * | UART5    | PCLK1     | UART5_IRQn   | PC12 (5)     | PD2 (5)      |
 * | LPUART1  | PCLK1     | LPUART1_IRQn | PA2 (12)     | PA3 (12)     |
 *
 * On the Nucleo-G474RE the ST-LINK virtual COM port is LPUART1 on PA2/PA3
 * (AF12); board_vcp_init() in board/nucleo_g474re.h sets it up.
 *
 * The driver does not configure pins: use gpio_init_af() (TX push-pull,
 * RX with pull-up so the idle line reads high).
 *
 * Threading model: one UartHandle per instance, owned by the application.
 * Transmit and receive may be used from different contexts (for example
 * receive in the main loop while an interrupt handler transmits only if
 * transmit is interrupt-driven), but two contexts must not transmit, or
 * receive, through the same handle at the same time.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Data bits per character (parity bit not included). */
typedef enum {
    UART_DATA_BITS_7 = 7,
    UART_DATA_BITS_8 = 8,
} UartDataBits;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD  = 2,
} UartParity;

typedef enum {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_2 = 2,
} UartStopBits;

typedef enum {
    UART_FLOW_NONE    = 0,
    UART_FLOW_RTS_CTS = 1, /**< Hardware flow control; configure the RTS/CTS pins too. */
} UartFlowControl;

/** Kernel clock feeding the baud-rate generator (RCC_CCIPR). */
typedef enum {
    UART_CLOCK_PCLK   = 0, /**< APB clock: PCLK2 for USART1, PCLK1 otherwise. */
    UART_CLOCK_SYSCLK = 1, /**< System clock. */
    UART_CLOCK_HSI16  = 2, /**< HSI16: baud rate independent of clock presets. */
} UartClockSource;

/** Line configuration. */
typedef struct {
    uint32_t        baud_rate;
    UartDataBits    data_bits;
    UartParity      parity;
    UartStopBits    stop_bits;
    UartFlowControl flow_control;
    UartClockSource clock_source;
} UartConfig;

/** @brief 8 data bits, no parity, 1 stop bit, no flow control, PCLK. */
static inline UartConfig uart_default_config(const uint32_t baud_rate)
{
    const UartConfig config = {
        baud_rate, UART_DATA_BITS_8, UART_PARITY_NONE, UART_STOP_BITS_1,
        UART_FLOW_NONE, UART_CLOCK_PCLK,
    };
    return config;
}

/** Accumulated error flags returned by uart_take_errors(). */
#define UART_ERROR_PARITY      (1UL << 0) /**< Parity error (byte dropped). */
#define UART_ERROR_FRAMING     (1UL << 1) /**< Framing error or break (byte dropped). */
#define UART_ERROR_NOISE       (1UL << 2) /**< Noise detected (byte dropped). */
#define UART_ERROR_OVERRUN     (1UL << 3) /**< Hardware overrun: bytes lost before being read. */
#define UART_ERROR_RX_OVERFLOW (1UL << 4) /**< Receive ring buffer full: bytes lost. */

/** Largest baud-rate error accepted by uart_init(), in parts per million (2.5 %). */
#define UART_MAX_BAUD_ERROR_PPM 25000UL

/** Result of uart_calc_baud(). */
typedef struct {
    uint32_t presc;       /**< PRESC register value (0..11). */
    uint32_t brr;         /**< BRR register value. */
    bool     over8;       /**< Oversampling by 8 (USART only). */
    uint32_t actual_baud; /**< Baud rate actually produced. */
    uint32_t error_ppm;   /**< |actual - requested| / requested, in ppm. */
} UartBaudSetting;

/** Ring buffer used in interrupt mode. Treat as private. */
typedef struct {
    volatile uint8_t *data;
    uint16_t          size;
    volatile uint16_t head; /* written by the producer only */
    volatile uint16_t tail; /* written by the consumer only */
} UartRing;

/** Driver state for one UART instance. Treat the fields as private. */
typedef struct {
    USART_TypeDef    *instance;
    uint8_t           data_mask;
    bool              rx_irq;
    bool              tx_irq;
    volatile uint32_t errors;
    UartRing          rx;
    UartRing          tx;
} UartHandle;

/**
 * @brief Compute prescaler, oversampling and BRR for a baud rate.
 * @param kernel_hz Kernel clock frequency.
 * @param baud_rate Requested baud rate.
 * @param lpuart true for LPUART1, false for USART/UART.
 * @param[out] setting Register values and resulting error.
 * @return DRV_OK; DRV_ERR_PARAM for zero inputs or NULL; DRV_ERR_UNSUPPORTED
 *         when no register setting reaches the rate within
 *         UART_MAX_BAUD_ERROR_PPM.
 */
DrvStatus uart_calc_baud(uint32_t kernel_hz, uint32_t baud_rate, bool lpuart,
                         UartBaudSetting *setting);

/**
 * @brief Initialise a UART in blocking (polled) mode.
 * @param uart Handle to initialise.
 * @param instance USART1, USART2, USART3, UART4, UART5 or LPUART1.
 * @param config Line configuration.
 * @return DRV_OK; DRV_ERR_PARAM for a NULL pointer, unknown instance, zero
 *         baud rate or out-of-range enum; DRV_ERR_UNSUPPORTED if the baud rate cannot be
 *         produced from the kernel clock; DRV_ERR_TIMEOUT if the
 *         transmitter/receiver do not acknowledge enable or HSI16 does not start.
 * @note Resets the peripheral through RCC first. Configure the pins
 *       separately. Re-initialise after changing the clock preset unless
 *       UART_CLOCK_HSI16 is used.
 */
DrvStatus uart_init(UartHandle *uart, USART_TypeDef *instance, const UartConfig *config);

/**
 * @brief Disable the UART, its interrupt, and its bus clock.
 */
DrvStatus uart_deinit(UartHandle *uart);

/**
 * @brief Switch receive and/or transmit to interrupt-driven ring buffers.
 * @param uart Initialised handle.
 * @param rx_buffer Receive storage, or NULL to keep receive polled.
 * @param rx_size Size of @p rx_buffer (2..65535); one slot stays unused.
 * @param tx_buffer Transmit storage, or NULL to keep transmit polled.
 * @param tx_size Size of @p tx_buffer (2..65535).
 * @param priority NVIC priority 0 (highest) to 15 (lowest).
 * @return DRV_OK, DRV_ERR_PARAM, or DRV_ERR_STATE if not initialised.
 * @note The buffers must stay valid until uart_deinit(). The application
 *       must define the instance's IRQ handler and call uart_irq_handler()
 *       from it, e.g.
 *       `void LPUART1_IRQHandler(void) { uart_irq_handler(&vcp); }`.
 */
DrvStatus uart_enable_irq(UartHandle *uart, uint8_t *rx_buffer, size_t rx_size,
                          uint8_t *tx_buffer, size_t tx_size, uint32_t priority);

/** @brief Interrupt service routine body; call it from the instance's IRQ handler. */
void uart_irq_handler(UartHandle *uart);

/**
 * @brief Transmit bytes.
 * @param data Bytes to send.
 * @param len Number of bytes.
 * @param timeout_ms Limit for the whole call, or DRV_WAIT_FOREVER.
 * @return DRV_OK, DRV_ERR_PARAM, DRV_ERR_STATE (not initialised) or
 *         DRV_ERR_TIMEOUT (not all bytes were accepted; e.g. CTS held off).
 * @note Polled mode returns once the last byte is in the transmit data
 *       register; interrupt mode once it is queued. Use uart_flush() to wait
 *       until it has left the pin. Safe to call with interrupts masked:
 *       interrupt-mode transmission then drains the queue by polling.
 */
DrvStatus uart_write(UartHandle *uart, const void *data, size_t len, uint32_t timeout_ms);

/** @brief Transmit a NUL-terminated string (without the terminator). */
DrvStatus uart_write_string(UartHandle *uart, const char *str, uint32_t timeout_ms);

/** @brief Transmit one byte. */
DrvStatus uart_write_byte(UartHandle *uart, uint8_t byte, uint32_t timeout_ms);

/**
 * @brief Wait until every queued byte has been completely shifted out.
 * @return DRV_OK, DRV_ERR_STATE or DRV_ERR_TIMEOUT.
 */
DrvStatus uart_flush(UartHandle *uart, uint32_t timeout_ms);

/**
 * @brief Receive exactly @p len bytes or until the timeout.
 * @param data Destination.
 * @param len Number of bytes wanted.
 * @param[out] received Bytes stored in @p data; may be NULL.
 * @param timeout_ms Limit for the whole call; 0 returns what is already
 *        available (interrupt mode) or at most one waiting byte (polled).
 * @return DRV_OK when @p len bytes were received; DRV_ERR_TIMEOUT when fewer
 *         arrived; in polled mode DRV_ERR_PARITY, DRV_ERR_FRAMING,
 *         DRV_ERR_NOISE or DRV_ERR_OVERRUN stop the read at the bad byte.
 * @note In interrupt mode line errors never abort a read: corrupted bytes
 *       are dropped and the error is recorded for uart_take_errors().
 *       Characters are masked to the configured data bits.
 */
DrvStatus uart_read(UartHandle *uart, void *data, size_t len, size_t *received,
                    uint32_t timeout_ms);

/** @brief Receive one byte. */
DrvStatus uart_read_byte(UartHandle *uart, uint8_t *byte, uint32_t timeout_ms);

/**
 * @brief Receive a text line terminated by CR or LF.
 * @param line Destination; always NUL-terminated on return.
 * @param size Size of @p line including the terminator (>= 2).
 * @param timeout_ms Limit for the whole line.
 * @return DRV_OK when a terminator was received (it is not stored; an LF
 *         directly after a CR is consumed by the next call as an empty
 *         line), DRV_ERR_BUSY when the buffer filled up first (the partial
 *         line is returned), DRV_ERR_TIMEOUT, or a line error (polled mode).
 */
DrvStatus uart_read_line(UartHandle *uart, char *line, size_t size, uint32_t timeout_ms);

/** @brief Bytes waiting to be read (interrupt mode: ring buffer; polled: 0 or 1). */
size_t uart_rx_available(UartHandle *uart);

/** @brief Return and clear the accumulated UART_ERROR_* flags. */
uint32_t uart_take_errors(UartHandle *uart);

/**
 * @brief ConsoleWriteFn adapter: console_set_output(uart_console_write, &uart).
 * @return Number of bytes accepted (stops at the first timeout, 100 ms).
 */
size_t uart_console_write(void *uart, const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_UART_H */
