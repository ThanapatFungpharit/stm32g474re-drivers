/*
 * Polled UART line echo on the ST-LINK virtual COM port.
 *
 * Open a terminal at 115200 8N1 (for example `pio device monitor`), type a
 * line and press Enter; the board answers with the line and its length.
 * Line errors (wrong baud rate, parity or framing on the host side) are
 * reported instead.
 *
 * Demonstrates: board_vcp_init() (gpio_init_af() + uart_init()),
 * uart_write_string(), uart_read_line() with timeouts, error statuses.
 */

#include "board/nucleo_g474re.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

static UartHandle vcp;

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();

    if ((board_led_init() != DRV_OK) || (board_vcp_init(&vcp, 115200U) != DRV_OK)) {
        panic("init");
    }
    console_set_output(uart_console_write, &vcp);

    console_puts("\r\nUART echo ready. Type a line and press Enter.\r\n> ");

    char line[64];
    for (;;) {
        /* Wait up to 1 s at a time so the LED can show the loop is alive. */
        const DrvStatus status = uart_read_line(&vcp, line, sizeof line, 1000U);

        switch (status) {
            case DRV_OK:
                if (line[0] == '\0') {
                    break; /* LF after CR, or an empty line */
                }
                console_puts("\r\nyou typed \"");
                console_puts(line);
                console_puts("\" (");
                console_put_udec((uint32_t)__builtin_strlen(line));
                console_puts(" chars)\r\n> ");
                break;

            case DRV_ERR_TIMEOUT:
                board_led_toggle();
                break;

            case DRV_ERR_BUSY:
                console_puts("\r\nline too long, truncated: ");
                console_puts(line);
                console_puts("\r\n> ");
                break;

            default:
                console_puts("\r\nreceive error: ");
                console_puts(drv_status_str(status));
                console_puts("\r\n> ");
                break;
        }
    }
}
