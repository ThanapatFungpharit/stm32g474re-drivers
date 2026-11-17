#ifndef STM32G474RE_SYSTEM_CLOCK_H
#define STM32G474RE_SYSTEM_CLOCK_H

/**
 * @file clock.h
 * @brief System clock configuration and clock-tree frequency queries.
 *
 * All presets are derived from the internal 16 MHz HSI oscillator
 * (±1 % over temperature), so they work on any board without a crystal:
 *
 * | Preset                   | SYSCLK  | PLL (M/N/R) | Voltage range     | Flash WS |
 * |--------------------------|--------:|-------------|-------------------|---------:|
 * | CLOCK_PRESET_HSI_16MHZ   |  16 MHz | off         | Range 1 normal    | 0        |
 * | CLOCK_PRESET_PLL_80MHZ   |  80 MHz | /2, ×20, /2 | Range 1 normal    | 2        |
 * | CLOCK_PRESET_PLL_170MHZ  | 170 MHz | /4, ×85, /2 | Range 1 **boost** | 4        |
 *
 * In every preset HCLK = SYSCLK and PCLK1 = PCLK2 = HCLK (all prescalers
 * /1), so timer kernel clocks also equal SYSCLK.
 *
 * Peripheral clock dependencies:
 * - GPIO, ADC, DAC: HCLK (AHB2)
 * - USART2/3, UART4/5, LPUART1, TIM2-7, PWR: PCLK1 (APB1)
 * - USART1, TIM1/8/15/16/17/20, SYSCFG: PCLK2 (APB2)
 * - Timer kernel clock: PCLKx when the APBx prescaler is /1, else 2 × PCLKx
 * - IWDG: LSI (~32 kHz)
 *
 * Drivers that derive timing from the clock (UART baud rate, timer
 * frequency, DAC interface mode, ADC clock) read it when they are
 * initialised. Re-initialise them after changing the clock preset.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "drivers/power/power.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Frequency of the internal HSI16 oscillator. */
#define CLOCK_HSI_HZ 16000000UL

/** Supported system clock configurations. */
typedef enum {
    CLOCK_PRESET_HSI_16MHZ,   /**< HSI directly, PLL off. Reset default frequency. */
    CLOCK_PRESET_PLL_80MHZ,   /**< 80 MHz from HSI through the PLL. */
    CLOCK_PRESET_PLL_170MHZ,  /**< 170 MHz (device maximum), Range 1 boost mode. */
} ClockPreset;

/**
 * @brief Switch the system clock to a preset.
 * @param preset Target configuration.
 * @return DRV_OK; DRV_ERR_PARAM for an unknown preset; DRV_ERR_TIMEOUT if
 *         an oscillator, the PLL, the regulator or the clock switch does
 *         not become ready. On a PLL failure the system is left running
 *         from HSI at 16 MHz with safe Flash wait states.
 * @note Follows the RM0440 sequence: Flash wait states are raised before
 *       and lowered after a frequency change, and when going above 80 MHz
 *       HCLK is divided by 2 for at least 1 µs around the switch.
 *       Updates SystemCoreClock and calls time_clock_changed().
 *       May be called at any time, also from a PLL-based clock.
 */
DrvStatus clock_set_preset(ClockPreset preset);

/** @brief Current SYSCLK frequency in hertz, computed from RCC registers. */
uint32_t clock_get_sysclk_hz(void);

/** @brief Current HCLK (AHB, core) frequency in hertz. */
uint32_t clock_get_hclk_hz(void);

/** @brief Current PCLK1 (APB1) frequency in hertz. */
uint32_t clock_get_pclk1_hz(void);

/** @brief Current PCLK2 (APB2) frequency in hertz. */
uint32_t clock_get_pclk2_hz(void);

/** @brief Kernel clock of timers on APB1 (TIM2-TIM7) in hertz. */
uint32_t clock_get_apb1_timer_hz(void);

/** @brief Kernel clock of timers on APB2 (TIM1/8/15/16/17/20) in hertz. */
uint32_t clock_get_apb2_timer_hz(void);

/**
 * @brief Minimum Flash wait states for an HCLK frequency (RM0440 Table 9).
 * @param hclk_hz HCLK frequency.
 * @param range Voltage range.
 * @param boost Range 1 boost mode (ignored for Range 2).
 * @return Wait states (0-4), or -1 when the frequency is not allowed in
 *         the given range.
 */
int32_t clock_flash_wait_states(uint32_t hclk_hz, PowerVoltageRange range, bool boost);

/**
 * @brief Validate PLL settings and compute the PLLR output frequency.
 * @param input_hz PLL input frequency (HSI or HSE).
 * @param m Input divider, 1..16.
 * @param n VCO multiplier, 8..127.
 * @param r Output divider, 2, 4, 6 or 8.
 * @param[out] output_hz PLLR output frequency; may be NULL.
 * @return DRV_OK, or DRV_ERR_PARAM when a divider is out of range, the
 *         VCO input is outside 2.66-16 MHz, the VCO output is outside
 *         96-344 MHz, or the output exceeds 170 MHz.
 */
DrvStatus clock_pll_check(uint32_t input_hz, uint32_t m, uint32_t n, uint32_t r,
                          uint32_t *output_hz);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_SYSTEM_CLOCK_H */
