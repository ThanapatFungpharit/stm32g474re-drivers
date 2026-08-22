#include "dac.h"


/**
 * @brief Get the RCC clock-enable mask for a DAC peripheral.
 * @param DACx DAC peripheral instance.
 * @return Corresponding RCC AHB2 clock-enable mask.
 */
static uint32_t getDACxClockEnableBit(const DAC_TypeDef *DACx)
{
    switch ((uintptr_t)DACx) {
        case (uintptr_t)DAC1:
            return RCC_AHB2ENR_DAC1EN;

        case (uintptr_t)DAC2:
            return RCC_AHB2ENR_DAC2EN;

        case (uintptr_t)DAC3:
            return RCC_AHB2ENR_DAC3EN;

        case (uintptr_t)DAC4:
            return RCC_AHB2ENR_DAC4EN;

        default:
            panic("Invalid DAC peripheral in getDACxClockEnableBit().");
    }
}


static void validateDACx(const DAC_TypeDef *DACx)
{
    if (DACx != DAC1 &&
        DACx != DAC2 &&
        DACx != DAC3 &&
        DACx != DAC4)
        panic("Invalid DAC peripheral.");
}


static void validateDACChannel(
    const DAC_TypeDef *DACx,
    const DACChannel channel
    )
{
    validateDACx(DACx);

    switch ((uint8_t)channel) {
        case DACCHANNEL_1:
            break;

        case DACCHANNEL_2:
            if (DACx == DAC2)
                panic("DAC2 does not have channel 2.");
            break;

        default:
            panic("Invalid DAC channel.");
    }
}


void setDACxClock(
    const DAC_TypeDef *DACx,
    const EnableState state
    )
{
    const uint32_t clock = getDACxClockEnableBit(DACx);

    switch (state) {
        case STATE_ENABLE:
            RCC->AHB2ENR |= clock;
            break;

        case STATE_DISABLE:
            RCC->AHB2ENR &= ~clock;
            break;

        default:
            panic("Invalid enable state in setDACxClock().");
    }

    (void)RCC->AHB2ENR;
}


// === DAC API ===

void initDAC(
    DAC_TypeDef *DACx,
    const DACChannel channel
    )
{
    validateDACChannel(DACx, channel);

    switch ((uint8_t)channel) {
        case DACCHANNEL_1:
            DACx->CR |= DAC_CR_EN1;
            break;

        case DACCHANNEL_2:
            DACx->CR |= DAC_CR_EN2;
            break;

        default:
            // Validated above.
            break;
    }
}


void dacWrite(
    DAC_TypeDef *DACx,
    const DACChannel channel,
    const uint16_t value
    )
{
    validateDACChannel(DACx, channel);

    if (value > DAC_MAX_VALUE)
        panic("DAC value must be 12-bit.");

    switch ((uint8_t)channel) {
        case DACCHANNEL_1:
            DACx->DHR12R1 = value;
            break;

        case DACCHANNEL_2:
            DACx->DHR12R2 = value;
            break;

        default:
            // Validated above.
            break;
    }
}