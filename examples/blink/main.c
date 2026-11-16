/*
 * Blink: toggle LD2 (PA5) every 500 ms at 170 MHz.
 *
 * Demonstrates: clock_set_preset(), time_init(), gpio_init_output(),
 * gpio_toggle(), time_delay_ms().
 *
 * Expected: LD2 blinks at 1 Hz. If the clock setup fails, panic() halts
 * and LD2 stays off.
 */

#include "drivers/gpio/gpio.h"
#include "system/clock/clock.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();

    if (gpio_init_output(GPIOA, 5U, false) != DRV_OK) {
        panic("gpio");
    }

    for (;;) {
        gpio_toggle(GPIOA, 5U);
        time_delay_ms(500U);
    }
}
