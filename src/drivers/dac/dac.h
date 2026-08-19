//
// Created by user on 8/18/2026.
//

#ifndef STM32G474RE_DRIVERS_DAC_H
#define STM32G474RE_DRIVERS_DAC_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"


/** @brief Get a DAC peripheral's RCC clock mask. @param DACx DAC peripheral. @return RCC clock-enable mask. */
uint32_t getDACxClock(const DAC_TypeDef *DACx);

/** @brief Set a DAC peripheral clock state. @param DACx DAC peripheral. @param state Desired clock state. */
void setDACxClock(const DAC_TypeDef *DACx, EnableState state);

typedef enum {
    DACCHANNEL_1 = 0b01,
    DACCHANNEL_2 = 0b10,
} DACChannel;


/** @brief Enable a DAC channel. @param DACx DAC peripheral. @param channel DAC channel. */
void initDAC(
    DAC_TypeDef *DACx,
    DACChannel channel
);

/** @brief Write a 12-bit value to a DAC channel. @param DACx DAC peripheral. @param channel DAC channel. @param value Value to write. */
void dacWrite(
    DAC_TypeDef *DACx,
    DACChannel channel,
    uint16_t value
);


#endif // STM32G474RE_DRIVERS_DAC_H
