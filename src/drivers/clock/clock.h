#ifndef NUCLEO_G474RE_DRIVER_CLOCK_H
#define NUCLEO_G474RE_DRIVER_CLOCK_H

#include "stm32g474xx.h"

#include "drivers/common.h"
#include "drivers/power/power.h"


/* GPIO clock */

uint32_t getGPIOxClock(const GPIO_TypeDef *GPIOx);

void enableGPIOxClock(const GPIO_TypeDef *GPIOx);

void disableGPIOxClock(const GPIO_TypeDef *GPIOx);


/* Clock source */

typedef enum {
    CLOCKSOURCE_HSI = RCC_CFGR_SW_HSI,
    CLOCKSOURCE_PLL = RCC_CFGR_SW_PLL
} ClockSource;

void switchClockSource(ClockSource source);


/* PLL */

void setPLLState(EnableState state);

void configurePLL(
    uint32_t m,
    uint32_t n,
    uint32_t r
);


/* Utilities */

void resetBusPrescalers(void);

void flashSetLatency(uint32_t latency);


/* System clock */

typedef enum {
    CLOCK_16MHZ,
    CLOCK_80MHZ,
    CLOCK_170MHZ
} ClockSpeed;

void setClockSpeed(ClockSpeed speed);


#endif /* NUCLEO_G474RE_DRIVER_CLOCK_H */