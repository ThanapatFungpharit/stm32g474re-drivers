/*
 * Interrupt-driven UART on the ST-LINK virtual COM port (115200 8N1).
 *
 * Every received byte is echoed back. Receive and transmit run through
 * ring buffers filled/drained by LPUART1_IRQHandler, so the main loop never
 * blocks on the line and characters are not lost while it is busy (here:
 * a deliberate 200 ms of "work" each second). Accumulated line errors are
 * reported once per second.
 *
 * Demonstrates: uart_enable_irq(), uart_irq_handler(), uart_read() with a
 * zero timeout, uart_take_errors(), uart_flush().
 *
 * Hardware check: paste a long block of text into the terminal; it must be
 * echoed back completely even though the main loop is busy for 200 ms.
 */

#include "board/nucleo_g474re.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

static UartHandle vcp;
static uint8_t rx_storage[512];
static uint8_t tx_storage[512];

void LPUART1_IRQHandler(void);

void LPUART1_IRQHandler(void)
{
    uart_irq_handler(&vcp);
}

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();

    if ((board_led_init() != DRV_OK) || (board_vcp_init(&vcp, 115200U) != DRV_OK) ||
        (uart_enable_irq(&vcp, rx_storage, sizeof rx_storage,
                         tx_storage, sizeof tx_storage, 6U) != DRV_OK)) {
        panic("init");
    }
    console_set_output(uart_console_write, &vcp);

    console_puts("\r\nInterrupt-driven echo. Paste some text.\r\n");

    uint32_t last_report_ms = time_millis();
    for (;;) {
        uint8_t chunk[32];
        size_t got = 0U;

        /* Timeout 0: take whatever has arrived, never wait. */
        (void)uart_read(&vcp, chunk, sizeof chunk, &got, 0U);
        if (got > 0U) {
            (void)uart_write(&vcp, chunk, got, 100U);
        }

        if ((time_millis() - last_report_ms) >= 1000U) {
            last_report_ms = time_millis();
            board_led_toggle();

            const uint32_t errors = uart_take_errors(&vcp);
            if (errors != 0U) {
                console_puts("\r\n[errors 0x");
                console_put_hex(errors, 2U);
                console_puts("]\r\n");
            }

            time_delay_ms(200U); /* simulated work; reception continues */
        }
    }
}
