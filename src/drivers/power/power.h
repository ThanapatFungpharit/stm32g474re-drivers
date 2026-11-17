#ifndef STM32G474RE_DRIVERS_POWER_H
#define STM32G474RE_DRIVERS_POWER_H

/**
 * @file power.h
 * @brief PWR peripheral: core voltage range, Range 1 boost mode and the
 *        USB Type-C dead-battery pull-downs.
 *
 * The voltage range limits the maximum system clock (RM0440 §6.1.5):
 *
 * | Range                 | Max SYSCLK |
 * |-----------------------|-----------:|
 * | Range 1, boost mode   |    170 MHz |
 * | Range 1, normal mode  |    150 MHz |
 * | Range 2               |     26 MHz |
 *
 * clock_set_preset() selects the range and boost mode for you; use these
 * functions directly only when managing the clock tree yourself, and follow
 * the ordering rules in the reference manual.
 */

#include <stdbool.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Core voltage range (PWR_CR1.VOS). */
typedef enum {
    POWER_RANGE_1 = 1, /**< High performance, up to 170 MHz. */
    POWER_RANGE_2 = 2, /**< Low power, up to 26 MHz. */
} PowerVoltageRange;

/** @brief Enable the PWR peripheral bus clock (idempotent). */
void power_enable_clock(void);

/**
 * @brief Select the core voltage range and wait until the regulator settles.
 * @param range Voltage range.
 * @return DRV_OK, DRV_ERR_PARAM, or DRV_ERR_TIMEOUT when VOSF stays set.
 * @note Enables the PWR clock. Before entering Range 2 the system clock
 *       must already be at or below 26 MHz.
 */
DrvStatus power_set_voltage_range(PowerVoltageRange range);

/** @brief Return the currently selected voltage range. */
PowerVoltageRange power_get_voltage_range(void);

/**
 * @brief Enable or disable Range 1 boost mode (PWR_CR5.R1MODE).
 * @param enable true for boost mode (needed above 150 MHz).
 * @note When raising the clock into boost territory the AHB prescaler must
 *       be /2 during the switch; clock_set_preset() handles this.
 */
void power_set_boost(bool enable);

/** @brief Return true when Range 1 boost mode is selected. */
bool power_is_boost_enabled(void);

/**
 * @brief Disable the USB Type-C dead-battery pull-downs on PB4 and PB6.
 *
 * After reset the UCPD1 dead-battery logic connects pull-downs (~5.1 kΩ)
 * to PB4 (UCPD1_CC2) and PB6 (UCPD1_CC1). Call this before using those
 * pins as GPIO or for another peripheral, otherwise inputs read low and
 * outputs are loaded.
 */
void power_disable_ucpd_dead_battery(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_POWER_H */
