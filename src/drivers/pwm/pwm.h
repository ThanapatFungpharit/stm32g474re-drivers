#ifndef STM32G474RE_DRIVERS_PWM_H
#define STM32G474RE_DRIVERS_PWM_H

/**
 * @file pwm.h
 * @brief Edge-aligned PWM output (PWM mode 1) on timer channels 1-4.
 *
 * Typical use:
 * @code
 * gpio_init_af(GPIOA, 6U, 2U, GPIO_OTYPE_PUSH_PULL, GPIO_PULL_NONE, GPIO_SPEED_LOW); // TIM3_CH1
 * pwm_init_frequency(TIM3, 1000U);                      // 1 kHz, timer running
 * pwm_enable_channel(TIM3, PWM_CHANNEL_1, PWM_POLARITY_ACTIVE_HIGH);
 * pwm_set_duty_permille(TIM3, PWM_CHANNEL_1, 250U);      // 25 %
 * @endcode
 *
 * All channels of a timer share its frequency. Compare values are
 * preloaded: a new duty cycle takes effect at the next period boundary, so
 * updates never produce truncated or doubled pulses.
 *
 * On TIM1/8/15/16/17/20 the main output enable (MOE) is set automatically.
 * Complementary (CHxN) outputs, dead time and break inputs are not
 * configured by this driver.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "drivers/tim/tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Timer output channel. */
typedef enum {
    PWM_CHANNEL_1 = 1,
    PWM_CHANNEL_2 = 2,
    PWM_CHANNEL_3 = 3,
    PWM_CHANNEL_4 = 4,
} PwmChannel;

/** Output polarity. */
typedef enum {
    PWM_POLARITY_ACTIVE_HIGH = 0, /**< Output high for the first "compare" ticks of each period. */
    PWM_POLARITY_ACTIVE_LOW  = 1, /**< Inverted output. */
} PwmPolarity;

/**
 * @brief Configure the timer for PWM and start it.
 * @param tim Timer with at least one PWM channel.
 * @param psc Prescaler (counter clock = f_tim / (psc + 1)).
 * @param arr Auto-reload; the period is arr + 1 counter ticks.
 * @return DRV_OK or DRV_ERR_PARAM.
 * @note Channels are enabled separately with pwm_enable_channel().
 *       Calling this again changes the frequency of every channel.
 */
DrvStatus pwm_init(TIM_TypeDef *tim, uint32_t psc, uint32_t arr);

/**
 * @brief pwm_init() for a PWM frequency, maximising duty resolution.
 * @return DRV_OK, DRV_ERR_PARAM or DRV_ERR_UNSUPPORTED (frequency unreachable).
 * @note The resolution is pwm_get_period_ticks() steps; for example
 *       170 MHz / 20 kHz = 8500 steps.
 */
DrvStatus pwm_init_frequency(TIM_TypeDef *tim, uint32_t frequency_hz);

/**
 * @brief Configure a channel for PWM mode 1 with 0 % duty and enable its output.
 * @return DRV_OK or DRV_ERR_PARAM (invalid timer/channel or timer without that channel).
 */
DrvStatus pwm_enable_channel(TIM_TypeDef *tim, PwmChannel channel, PwmPolarity polarity);

/** @brief Disable a channel output (the pin then follows its idle state). */
DrvStatus pwm_disable_channel(TIM_TypeDef *tim, PwmChannel channel);

/**
 * @brief Number of counter ticks in one period (ARR + 1).
 */
uint32_t pwm_get_period_ticks(const TIM_TypeDef *tim);

/**
 * @brief Set the raw compare value.
 * @param value Active ticks per period, 0 (always inactive) to
 *        pwm_get_period_ticks() (always active); larger values are clamped.
 * @note Asserts that the timer/channel combination is valid.
 */
void pwm_set_compare(TIM_TypeDef *tim, PwmChannel channel, uint32_t value);

/**
 * @brief Set the duty cycle in tenths of a percent.
 * @param permille 0 to 1000; larger values are clamped to 1000 (100 %).
 */
void pwm_set_duty_permille(TIM_TypeDef *tim, PwmChannel channel, uint32_t permille);

/**
 * @brief Compute the compare value for a duty cycle.
 * @param period_ticks Ticks per period (ARR + 1).
 * @param permille Duty cycle 0-1000 (clamped).
 * @return Compare value, rounded to nearest.
 */
uint32_t pwm_duty_to_compare(uint32_t period_ticks, uint32_t permille);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_PWM_H */
