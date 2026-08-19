#include "dac.h"

#define DAC_MAX_VALUE 0x0FFFU


/**
 * @brief Get the RCC clock-enable mask for a DAC peripheral.
 *
 * Maps a DAC peripheral instance to its corresponding AHB2 clock-enable bit.
 *
 * @param DACx DAC peripheral instance (DAC1 or DAC2).
 *
 * @return Clock-enable bit mask for the given DAC peripheral.
 */
uint32_t getDACxClock(const DAC_TypeDef *DACx) {
    switch ((uintptr_t)DACx) {
        case (uintptr_t)DAC1:
            return RCC_AHB2ENR_DAC1EN;

        case (uintptr_t)DAC2:
            return RCC_AHB2ENR_DAC2EN;

        default:
            panic("Invalid DAC peripheral in getDACxClock()");
    }
}

/**
 * @brief Enable or disable a DAC peripheral clock.
 *
 * Looks up the clock-enable bit for @p DACx via getDACxClock()
 * and sets or clears it in RCC->AHB2ENR.
 *
 * @param DACx DAC peripheral instance (DAC1 or DAC2).
 * @param state Desired state:
 *        - STATE_ENABLE: enable the DAC peripheral clock.
 *        - STATE_DISABLE: disable the DAC peripheral clock.
 */
void setDACxClock(const DAC_TypeDef *DACx, const EnableState state) {
    const uint32_t clock = getDACxClock(DACx);
    switch (state) {
        case STATE_ENABLE:
            RCC->AHB2ENR |= clock;
            break;

        case STATE_DISABLE:
            RCC->AHB2ENR &= ~clock;
            break;

        default:
            // unreachable if types are respected
            panic("Invalid enable state in toggleDACxClock()");
    }
    (void)RCC->AHB2ENR;
}

// === validate ===

/**
 * @brief Validate a DAC peripheral and channel combination.
 * @param DACx DAC peripheral.
 * @param channel DAC channel.
 * @note Calls panic() for unsupported combinations.
 */
static void validateDAC(
    const DAC_TypeDef *DACx,
    const DACChannel channel
)
{
    switch (channel) {
        case DACCHANNEL_1:
            if (DACx != DAC1 && DACx != DAC2)
                panic("Invalid DAC channel 1.");
            break;

        case DACCHANNEL_2:
            if (DACx != DAC1)
                panic("Invalid DAC channel 2.");
            break;

        default:
            panic("Invalid DAC channel.");
    }
}

// === DAC api ===

/**
 * @brief Enable a DAC channel.
 * @param DACx DAC peripheral.
 * @param channel DAC channel to enable.
 */
void initDAC(
    DAC_TypeDef *DACx,
    const DACChannel channel
)
{
    validateDAC(DACx, channel);

    if (channel == DACCHANNEL_1)
        DACx->CR |= DAC_CR_EN1;
    else
        DACx->CR |= DAC_CR_EN2;
}


/**
 * @brief Write a 12-bit value to a DAC channel holding register.
 * @param DACx DAC peripheral.
 * @param channel DAC channel.
 * @param value 12-bit value to write.
 * @note Calls panic() when value exceeds 12 bits.
 */
void dacWrite(
    DAC_TypeDef *DACx,
    const DACChannel channel,
    const uint16_t value
)
{
    validateDAC(DACx, channel);

    if (value > DAC_MAX_VALUE)
        panic("DAC value must be 12-bit.");

    if (channel == DACCHANNEL_1)
        DACx->DHR12R1 = value;
    else
        DACx->DHR12R2 = value;
}
