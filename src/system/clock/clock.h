#ifndef STM32G474RE__DRIVER_CLOCK_H
#define STM32G474RE__DRIVER_CLOCK_H

#include "stm32g474xx.h"

#include "drivers/common.h"
#include "drivers/power/power.h"
#include "system/panic/panic.h"


/* Clock source */

typedef enum {
    CLOCKSOURCE_HSI = RCC_CFGR_SW_HSI,
    CLOCKSOURCE_PLL = RCC_CFGR_SW_PLL
} ClockSource;

/**
 * @brief Select the system clock source.
 * @param source Clock source to select.
 * @note The selected clock source must already be configured and ready.
 */
void switchClockSource(ClockSource source);

/**
 * @brief Enable or disable a clock source without waiting for readiness.
 * @param source Clock source to control.
 * @param state Desired clock source state.
 * @note Modifies RCC->CR without waiting for its ready flag.
 */
void setSourceClockStateAsync(ClockSource source, EnableState state);

/**
 * @brief Enable or disable a clock source and wait for completion.
 * @param source Clock source to control.
 * @param state Desired clock source state.
 * @note Blocks until the corresponding RCC->CR ready status reaches the requested state.
 */
void setSourceClockState(ClockSource source, EnableState state);

/* PLL */

/**
 * @brief Configure the main PLL.
 * @param m PLL input divider value.
 * @param n PLL multiplication value.
 * @param r PLL output divider value.
 * @note Writes RCC->PLLCFGR; the PLL must be disabled before configuration changes.
 */
void configurePLL(
    uint32_t m,
    uint32_t n,
    uint32_t r
);

/* Utilities */

/**
 * @brief Reset the AHB and APB bus prescalers.
 * @note Clears the HPRE, PPRE1, and PPRE2 fields in RCC->CFGR.
 */
void resetBusPrescalers(void);

/**
 * @brief Set Flash memory wait-state latency without waiting.
 * @param latency Flash latency value for FLASH->ACR.
 */
void flashSetLatencyAsync(uint32_t latency);

/**
 * @brief Set Flash memory wait-state latency.
 * @param latency Flash latency value for FLASH->ACR.
 * @note Blocks until FLASH->ACR reflects the requested value.
 */
void flashSetLatency(uint32_t latency);

/* System clock */

typedef enum {
    CLOCKSPEED_16MHZ,
    CLOCKSPEED_80MHZ,
    CLOCKSPEED_170MHZ
} ClockSpeed;

/**
 * @brief Configure the MCU system clock.
 * @param speed Target system clock speed.
 * @note Reconfigures power, Flash latency, prescalers, PLL, and the system clock source.
 *       Updates SystemCoreClock after the hardware configuration completes.
 */
void setClockSpeed(ClockSpeed speed);

#endif /* STM32G474RE__DRIVER_CLOCK_H */
