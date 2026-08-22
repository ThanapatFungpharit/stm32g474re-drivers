#ifndef STM32G474RE_DRIVERS_DAC_H
#define STM32G474RE_DRIVERS_DAC_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"

#define DAC_MAX_VALUE 0x0FFFU


/**
 * @brief Enable or disable a DAC peripheral clock.
 * @param DACx DAC peripheral instance.
 * @param state Desired clock state.
 * @note Modifies RCC->AHB2ENR and reads it back after the write.
 */
void setDACxClock(const DAC_TypeDef *DACx, EnableState state);

typedef enum {
    DACCHANNEL_1 = 0b01,
    DACCHANNEL_2 = 0b10,
} DACChannel;


/**
 * @brief Enable a DAC channel.
 * @param DACx DAC peripheral instance.
 * @param channel DAC channel to enable.
 * @note Writes the channel enable bit in DACx->CR.
 *       Calls panic() for an unsupported peripheral and channel combination.
 */
void initDAC(
    DAC_TypeDef *DACx,
    DACChannel channel
);

/**
 * @brief Write a 12-bit value to a DAC channel holding register.
 * @param DACx DAC peripheral instance.
 * @param channel DAC channel.
 * @param value 12-bit value to write.
 * @note Calls panic() for an unsupported peripheral and channel combination
 *       or when value exceeds 12 bits.
 */
void dacWrite(
    DAC_TypeDef *DACx,
    DACChannel channel,
    uint16_t value
);


#endif // STM32G474RE_DRIVERS_DAC_H
