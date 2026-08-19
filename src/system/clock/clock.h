#ifndef STM32G474RE__DRIVER_CLOCK_H
#define STM32G474RE__DRIVER_CLOCK_H

#include "stm32g474xx.h"

#include "drivers/common.h"
#include "drivers/power/power.h"
#include "system/panic/panic.h"
#include "drivers/common.h"


/* Clock source */

typedef enum {
    CLOCKSOURCE_HSI = RCC_CFGR_SW_HSI,
    CLOCKSOURCE_PLL = RCC_CFGR_SW_PLL
} ClockSource;

/** @brief Select the system clock source. @param source Clock source to select. */
void switchClockSource(ClockSource source);

/** @brief Set a clock source state without waiting. @param source Clock source. @param state Desired state. */
void setSourceClockStateAsync(ClockSource source, EnableState state);

/** @brief Set a clock source state and wait for it. @param source Clock source. @param state Desired state. */
void setSourceClockState(ClockSource source, EnableState state);

/* PLL */

/** @brief Configure the main PLL. @param m PLL input divider. @param n PLL multiplier. @param r PLL output divider. */
void configurePLL(
    uint32_t m,
    uint32_t n,
    uint32_t r
);

/* Utilities */

/** @brief Reset AHB and APB bus prescalers. */
void resetBusPrescalers(void);

/** @brief Set Flash latency without waiting. @param latency Flash latency bits. */
void flashSetLatencyAsync(uint32_t latency);

/** @brief Set Flash latency and wait for it. @param latency Flash latency bits. */
void flashSetLatency(uint32_t latency);

/* System clock */

typedef enum {
    CLOCKSPEED_16MHZ,
    CLOCKSPEED_80MHZ,
    CLOCKSPEED_170MHZ
} ClockSpeed;

/** @brief Configure the system clock speed. @param speed Target clock speed. */
void setClockSpeed(ClockSpeed speed);

#endif /* STM32G474RE__DRIVER_CLOCK_H */
