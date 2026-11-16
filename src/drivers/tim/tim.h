#ifndef STM32G474RE_DRIVERS_TIM_H
#define STM32G474RE_DRIVERS_TIM_H

/**
 * @file tim.h
 * @brief Timer time base: clocking, period configuration and update
 *        interrupts for TIM1-TIM8, TIM15-TIM17 and TIM20.
 *
 * | Timer             | Bus  | Counter | PWM channels | Notes            |
 * |-------------------|------|---------|-------------:|------------------|
 * | TIM1, TIM8, TIM20 | APB2 | 16-bit  | 4            | advanced (MOE)   |
 * | TIM2, TIM5        | APB1 | 32-bit  | 4            |                  |
 * | TIM3, TIM4        | APB1 | 16-bit  | 4            |                  |
 * | TIM6, TIM7        | APB1 | 16-bit  | 0            | basic            |
 * | TIM15             | APB2 | 16-bit  | 2            | MOE              |
 * | TIM16, TIM17      | APB2 | 16-bit  | 1            | MOE              |
 *
 * The counter frequency is f_tim / (PSC + 1) and the update (overflow)
 * frequency is f_tim / ((PSC + 1) * (ARR + 1)), where f_tim is the timer
 * kernel clock (see tim_get_clock_hz()).
 *
 * As with EXTI, the application defines the interrupt handler (see
 * tim_update_irqn() for the vector) and calls tim_take_update() in it.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest prescaler value (PSC is 16 bits on every timer). */
#define TIM_PSC_MAX 0xFFFFUL

/** @brief Enable or disable the bus clock of a timer. */
DrvStatus tim_set_clock(TIM_TypeDef *tim, bool enable);

/** @brief Return true if @p tim is a timer supported by this driver. */
bool tim_is_valid(const TIM_TypeDef *tim);

/** @brief Kernel clock of the timer in hertz, or 0 for an invalid timer. */
uint32_t tim_get_clock_hz(const TIM_TypeDef *tim);

/** @brief Largest auto-reload value: 0xFFFFFFFF for TIM2/TIM5, else 0xFFFF; 0 if invalid. */
uint32_t tim_get_max_arr(const TIM_TypeDef *tim);

/** @brief Number of channels usable for PWM output (0-4); 0 if invalid. */
uint32_t tim_get_channel_count(const TIM_TypeDef *tim);

/** @brief Return true for timers with a break/dead-time unit (MOE bit). */
bool tim_has_main_output(const TIM_TypeDef *tim);

/**
 * @brief NVIC interrupt that carries the update event of @p tim.
 * @note Some vectors are shared: TIM1_UP with TIM16, TIM6 and TIM7 with
 *       the DAC underrun interrupts. Asserts that @p tim is valid.
 */
IRQn_Type tim_update_irqn(const TIM_TypeDef *tim);

/**
 * @brief Choose PSC and ARR for an update frequency.
 * @param clock_hz Timer kernel clock.
 * @param frequency_hz Wanted update frequency (> 0).
 * @param max_arr Largest allowed ARR (counter width).
 * @param[out] psc Prescaler register value.
 * @param[out] arr Auto-reload register value.
 * @return DRV_OK, DRV_ERR_PARAM for zero inputs, or DRV_ERR_UNSUPPORTED if
 *         the frequency is above clock_hz or too low to reach.
 * @note Uses the smallest prescaler that fits, which gives the finest
 *       period (and PWM duty) resolution; ARR is rounded to nearest.
 */
DrvStatus tim_calc_period(uint32_t clock_hz, uint32_t frequency_hz, uint32_t max_arr,
                          uint32_t *psc, uint32_t *arr);

/**
 * @brief Configure an up-counting time base and leave the timer stopped.
 * @param tim Timer.
 * @param psc Prescaler, 0-65535 (counter clock = f_tim / (psc + 1)).
 * @param arr Auto-reload, 1..tim_get_max_arr().
 * @return DRV_OK or DRV_ERR_PARAM.
 * @note Enables the timer clock, enables ARR preload, loads PSC/ARR with an
 *       update event that does not raise an interrupt (URS = 1), and resets
 *       the counter.
 */
DrvStatus tim_init(TIM_TypeDef *tim, uint32_t psc, uint32_t arr);

/**
 * @brief tim_init() with PSC/ARR computed by tim_calc_period().
 * @param frequency_hz Update (overflow) frequency.
 */
DrvStatus tim_init_frequency(TIM_TypeDef *tim, uint32_t frequency_hz);

/**
 * @brief Stop a timer, disable its update interrupt and gate its clock.
 * @note Resets the timer through RCC so the next init starts clean.
 */
DrvStatus tim_deinit(TIM_TypeDef *tim);

/** @brief Start counting. */
void tim_start(TIM_TypeDef *tim);

/** @brief Stop counting (the counter value is kept). */
void tim_stop(TIM_TypeDef *tim);

/** @brief Current counter value. */
uint32_t tim_get_counter(const TIM_TypeDef *tim);

/** @brief Overwrite the counter value. */
void tim_set_counter(TIM_TypeDef *tim, uint32_t value);

/**
 * @brief Enable the update interrupt in the timer and in the NVIC.
 * @param priority NVIC priority 0 (highest) to 15 (lowest).
 */
DrvStatus tim_enable_update_irq(TIM_TypeDef *tim, uint32_t priority);

/**
 * @brief Disable the update interrupt in the timer.
 * @note The NVIC line is left enabled because some vectors are shared.
 */
DrvStatus tim_disable_update_irq(TIM_TypeDef *tim);

/**
 * @brief Check and clear the update flag. Call from the IRQ handler.
 * @return true if an update event was pending.
 */
bool tim_take_update(TIM_TypeDef *tim);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_TIM_H */
