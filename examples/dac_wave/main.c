/*
 * DAC: step DAC1 channel 1 (PA4, Arduino A2) through 0, 0.5, 1.0, ... 3.0 V,
 * one step per second, then generate a 100 Hz triangle for 3 seconds.
 *
 * Demonstrates: gpio_init_analog(), dac_init(), dac_write(),
 * dac_mv_to_code() and fast updates with time_delay_us().
 *
 * Hardware check: a multimeter on PA4 follows the printed voltages within
 * a few tens of mV (the buffered output cannot reach the rails exactly);
 * an oscilloscope shows the 100 Hz triangle.
 */

#include "board/nucleo_g474re.h"
#include "drivers/dac/dac.h"
#include "drivers/gpio/gpio.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

#define VREF_MV 3300U /* VDDA = VREF+ on the Nucleo */

static UartHandle vcp;

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();
    if (board_vcp_init(&vcp, 115200U) != DRV_OK) {
        panic("vcp");
    }
    console_set_output(uart_console_write, &vcp);

    /* dac_init() runs after the clock change so the DAC interface mode
     * (HFSEL) matches the 170 MHz bus clock. */
    if ((gpio_init_analog(GPIOA, 4U) != DRV_OK) || (dac_init(DAC1, DAC_CHANNEL_1) != DRV_OK)) {
        panic("dac");
    }

    for (;;) {
        for (uint32_t mv = 0U; mv <= 3000U; mv += 500U) {
            dac_write(DAC1, DAC_CHANNEL_1, dac_mv_to_code(mv, VREF_MV));
            console_puts("PA4 = ");
            console_put_udec(mv);
            console_puts(" mV\r\n");
            time_delay_ms(1000U);
        }

        console_puts("100 Hz triangle for 3 s\r\n");
        /* 64 steps up + 64 down per 10 ms period: one step every 78 µs. */
        for (uint32_t period = 0U; period < 300U; ++period) {
            for (uint32_t i = 0U; i < 128U; ++i) {
                const uint32_t level = (i < 64U) ? i : (127U - i);
                dac_write(DAC1, DAC_CHANNEL_1, (uint16_t)((level * DAC_MAX_VALUE) / 63U));
                time_delay_us(78U);
            }
        }
    }
}
