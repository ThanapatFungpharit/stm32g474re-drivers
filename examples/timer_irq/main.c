/*
 * Timer interrupt: TIM6 overflows at 10 Hz; its interrupt toggles LD2 every
 * 5th tick (1 Hz blink) and the main loop prints the tick count every
 * second on the virtual COM port (115200 8N1).
 *
 * Demonstrates: tim_init_frequency(), tim_enable_update_irq(),
 * tim_take_update() in a user-defined TIM6_DAC_IRQHandler, tim_start().
 *
 * Hardware check: the count increases by exactly 10 per printed line, and
 * LD2 blinks at 1 Hz (compare against a stopwatch over a minute: 600 ticks).
 */

#include "board/nucleo_g474re.h"
#include "drivers/tim/tim.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

static UartHandle vcp;
static volatile uint32_t ticks;

void TIM6_DAC_IRQHandler(void);

void TIM6_DAC_IRQHandler(void)
{
    if (tim_take_update(TIM6)) {
        ++ticks;
        if ((ticks % 5U) == 0U) {
            board_led_toggle();
        }
    }
}

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

    if ((tim_init_frequency(TIM6, 10U) != DRV_OK) ||
        (tim_enable_update_irq(TIM6, 4U) != DRV_OK)) {
        panic("tim6");
    }
    tim_start(TIM6);

    for (;;) {
        time_delay_ms(1000U);
        console_puts("ticks: ");
        console_put_udec(ticks);
        console_puts("\r\n");
    }
}
