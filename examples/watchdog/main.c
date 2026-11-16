/*
 * Independent watchdog: the main loop refreshes a 1 s watchdog while B1 is
 * released. Hold B1 for more than a second and the watchdog resets the MCU.
 * After an IWDG reset the program reports it and blinks LD2 rapidly for
 * two seconds.
 *
 * Demonstrates: iwdg_start(), iwdg_refresh(), iwdg_caused_reset(),
 * iwdg_clear_reset_flags().
 *
 * Note: with a debugger halted at a breakpoint the watchdog is frozen
 * (freeze_in_debug = true), so stepping through code does not reset it.
 */

#include "board/nucleo_g474re.h"
#include "drivers/iwdg/iwdg.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

static UartHandle vcp;

int main(void)
{
    const bool watchdog_reset = iwdg_caused_reset();
    iwdg_clear_reset_flags();

    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();

    if ((board_led_init() != DRV_OK) || (board_button_init() != DRV_OK) ||
        (board_vcp_init(&vcp, 115200U) != DRV_OK)) {
        panic("init");
    }
    console_set_output(uart_console_write, &vcp);

    if (watchdog_reset) {
        console_puts("\r\n*** Reset by the independent watchdog ***\r\n");
        for (uint32_t i = 0U; i < 20U; ++i) {
            board_led_toggle();
            time_delay_ms(100U);
        }
    }

    if (iwdg_start(1000U, true) != DRV_OK) {
        panic("iwdg");
    }
    console_puts("\r\nWatchdog running (1 s). Hold B1 to stop refreshing it.\r\n");
    board_led_set(true);

    for (;;) {
        if (!board_button_pressed()) {
            iwdg_refresh();
        }
        time_delay_ms(100U);
    }
}
