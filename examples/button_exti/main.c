/*
 * Button interrupt: each press of B1 (PC13) toggles LD2 and prints a count
 * on the ST-LINK virtual COM port (115200 8N1).
 *
 * Demonstrates: exti_enable_gpio(), exti_take_pending(), a user-defined
 * EXTI15_10_IRQHandler, and software debouncing with time_millis().
 *
 * Expected: one toggle and one "press N" line per press. Contact bounce is
 * filtered by ignoring edges less than 50 ms after the previous one.
 */

#include "board/nucleo_g474re.h"
#include "drivers/exti/exti.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

#define DEBOUNCE_MS 50U

static UartHandle vcp;
static volatile uint32_t presses;
static volatile uint32_t last_edge_ms;

void EXTI15_10_IRQHandler(void);

void EXTI15_10_IRQHandler(void)
{
    if (exti_take_pending(BOARD_BUTTON_PIN)) {
        const uint32_t now = time_millis();
        if ((now - last_edge_ms) >= DEBOUNCE_MS) {
            ++presses;
            board_led_toggle();
        }
        last_edge_ms = now;
    }
}

int main(void)
{
    if ((clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK)) {
        panic("clock");
    }
    time_init();

    if ((board_led_init() != DRV_OK) || (board_button_init() != DRV_OK) ||
        (board_vcp_init(&vcp, 115200U) != DRV_OK)) {
        panic("init");
    }
    console_set_output(uart_console_write, &vcp);

    /* Priority 5 (0 = highest, 15 = lowest). The handler calls time_millis(),
     * which is safe at any priority. */
    if (exti_enable_gpio(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN, EXTI_TRIGGER_RISING, 5U) != DRV_OK) {
        panic("exti");
    }

    console_puts("\r\nPress B1\r\n");

    uint32_t reported = 0U;
    for (;;) {
        const uint32_t count = presses;
        if (count != reported) {
            reported = count;
            console_puts("press ");
            console_put_udec(count);
            console_puts("\r\n");
        }
        time_delay_ms(10U);
    }
}
