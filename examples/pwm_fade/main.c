/*
 * PWM: fade LD2 in and out with TIM2 channel 1 on PA5 (AF1) at 1 kHz.
 *
 * Demonstrates: gpio_init_af(), pwm_init_frequency(), pwm_enable_channel(),
 * pwm_set_duty_permille(), pwm_get_period_ticks().
 *
 * Hardware check: LD2 "breathes" with a 2 s cycle. On an oscilloscope PA5
 * shows a 1 kHz square wave whose duty cycle sweeps 0-100 %.
 */

#include "drivers/gpio/gpio.h"
#include "drivers/pwm/pwm.h"
#include "system/clock/clock.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();

    /* PA5 = TIM2_CH1 on alternate function 1. */
    if ((gpio_init_af(GPIOA, 5U, 1U, GPIO_OTYPE_PUSH_PULL, GPIO_PULL_NONE,
                      GPIO_SPEED_LOW) != DRV_OK) ||
        (pwm_init_frequency(TIM2, 1000U) != DRV_OK) ||
        (pwm_enable_channel(TIM2, PWM_CHANNEL_1, PWM_POLARITY_ACTIVE_HIGH) != DRV_OK)) {
        panic("pwm");
    }

    /* 170 MHz / 1 kHz = 170000 ticks: TIM2 is 32-bit, so PSC = 0. */

    for (;;) {
        for (uint32_t step = 0U; step < 200U; ++step) {
            /* Triangle 0..1000..0 squared for a perceptually even fade. */
            const uint32_t linear = (step < 100U) ? (step * 10U) : ((200U - step) * 10U);
            pwm_set_duty_permille(TIM2, PWM_CHANNEL_1, (linear * linear) / 1000U);
            time_delay_ms(10U);
        }
    }
}
