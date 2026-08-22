#include "adc.h"


/**
 * @brief Get the RCC clock-enable mask for an ADC peripheral.
 * @param ADCx ADC peripheral instance.
 * @return Corresponding RCC AHB2 clock-enable mask.
 */
static uint32_t getADCxClockEnableBit(const ADC_TypeDef *ADCx)
{
    switch ((uintptr_t)ADCx) {
        case (uintptr_t)ADC1:
        case (uintptr_t)ADC2:
            return RCC_AHB2ENR_ADC12EN;

        case (uintptr_t)ADC3:
        case (uintptr_t)ADC4:
        case (uintptr_t)ADC5:
            return RCC_AHB2ENR_ADC345EN;

        default:
            panic("Invalid ADC peripheral in getADCxClockEnableBit().");
    }
}


/**
 * @brief Validate an ADC peripheral instance.
 * @param ADCx ADC peripheral instance.
 */
static void validateADCx(const ADC_TypeDef *ADCx)
{
    if (ADCx != ADC1 &&
        ADCx != ADC2 &&
        ADCx != ADC3 &&
        ADCx != ADC4 &&
        ADCx != ADC5)
        panic("Invalid ADC peripheral.");
}


/**
 * @brief Validate an ADC channel.
 * @param channel ADC regular channel number.
 */
static void validateADCChannel(const ADCChannel channel)
{
    if (channel > ADC_CHANNEL_18)
        panic("Invalid ADC channel.");
}


/**
 * @brief Validate an ADC sampling-time selection.
 * @param sample_time ADC sampling-time selection.
 */
static void validateADCSampleTime(const ADCSampleTime sample_time)
{
    if (sample_time > ADC_SAMPLE_640C5)
        panic("Invalid ADC sample time.");
}


/**
 * @brief Enable or disable the ADC peripheral clock.
 * @param ADCx ADC peripheral instance.
 * @param state Clock enable state.
 */
void setADCxClock(
    const ADC_TypeDef *ADCx,
    const EnableState state
)
{
    const uint32_t clock = getADCxClockEnableBit(ADCx);

    switch (state) {
        case STATE_ENABLE:
            RCC->AHB2ENR |= clock;
            break;

        case STATE_DISABLE:
            RCC->AHB2ENR &= ~clock;
            break;

        default:
            panic("Invalid enable state in setADCxClock().");
    }

    (void)RCC->AHB2ENR;
}


/**
 * @brief Calibrate an ADC in single-ended mode.
 * @param ADCx ADC peripheral instance.
 *
 * The ADC must be disabled before calibration.
 */
void calibrateADC(ADC_TypeDef *ADCx)
{
    validateADCx(ADCx);

    if (ADCx->CR & ADC_CR_ADEN)
        panic("ADC must be disabled before calibration.");

    ADCx->CR &= ~ADC_CR_ADCALDIF;
    ADCx->CR |= ADC_CR_ADCAL;

    while (ADCx->CR & ADC_CR_ADCAL);
}


/**
 * @brief Initialize an ADC for single-ended 12-bit software-triggered conversion.
 * @param ADCx ADC peripheral instance.
 */
void initADC(ADC_TypeDef *ADCx)
{
    validateADCx(ADCx);

    setADCxClock(ADCx, STATE_ENABLE);

    /*
     * Exit deep-power-down mode and enable the internal ADC regulator.
     */
    ADCx->CR &= ~ADC_CR_DEEPPWD;
    ADCx->CR |= ADC_CR_ADVREGEN;

    /*
     * The ADC voltage regulator requires startup time before calibration.
     * At this point the project should provide a microsecond delay.
     */
    delayMicroseconds(20U);

    calibrateADC(ADCx);

    /*
     * 12-bit resolution, single conversion mode.
     */
    ADCx->CFGR &= ~(ADC_CFGR_RES | ADC_CFGR_CONT);

    /*
     * Software trigger, no external trigger.
     */
    ADCx->CFGR &= ~(ADC_CFGR_EXTEN | ADC_CFGR_EXTSEL);

    /*
     * Clear any stale status flags.
     */
    ADCx->ISR = ADC_ISR_ADRDY |
                ADC_ISR_EOSMP |
                ADC_ISR_EOC |
                ADC_ISR_EOS |
                ADC_ISR_OVR;

    ADCx->CR |= ADC_CR_ADEN;

    while (!(ADCx->ISR & ADC_ISR_ADRDY));
}


/**
 * @brief Configure the regular ADC channel and its sampling time.
 * @param ADCx ADC peripheral instance.
 * @param channel ADC channel to convert.
 * @param sample_time Sampling time for the selected channel.
 */
void setADCChannel(
    ADC_TypeDef *ADCx,
    const ADCChannel channel,
    const ADCSampleTime sample_time
)
{
    validateADCx(ADCx);
    validateADCChannel(channel);
    validateADCSampleTime(sample_time);

    if (!(ADCx->CR & ADC_CR_ADEN))
        panic("ADC must be enabled before configuring a channel.");

    if (ADCx->CR & ADC_CR_ADSTART)
        panic("ADC must not be converting when configuring a channel.");

    /*
     * The first regular conversion rank is stored in SQ1.
     *
     * SQR1:
     *   L[3:0]    = 0 -> one conversion
     *   SQ1[4:0]  = selected channel
     */
    ADCx->SQR1 &= ~(ADC_SQR1_L | ADC_SQR1_SQ1);
    ADCx->SQR1 |= ((uint32_t)channel << ADC_SQR1_SQ1_Pos);

    /*
     * Channels 1-9 use SMPR1.
     * Channels 10-18 use SMPR2.
     */
    if (channel <= ADC_CHANNEL_9) {
        const uint32_t shift = (uint32_t)channel * 3U;
        const uint32_t mask = 0x7U << shift;

        ADCx->SMPR1 =
            (ADCx->SMPR1 & ~mask) |
            ((uint32_t)sample_time << shift);
    } else {
        const uint32_t shift = ((uint32_t)channel - 10U) * 3U;
        const uint32_t mask = 0x7U << shift;

        ADCx->SMPR2 =
            (ADCx->SMPR2 & ~mask) |
            ((uint32_t)sample_time << shift);
    }
}


/**
 * @brief Start a single ADC conversion and return the result.
 * @param ADCx ADC peripheral instance.
 * @return 12-bit ADC conversion result in the range 0-4095.
 */
uint16_t readADC(ADC_TypeDef *ADCx)
{
    validateADCx(ADCx);

    if (!(ADCx->CR & ADC_CR_ADEN))
        panic("ADC must be enabled before conversion.");

    if (ADCx->CR & ADC_CR_ADSTART)
        panic("ADC conversion already in progress.");

    /*
     * Clear stale end-of-conversion and overrun flags.
     */
    ADCx->ISR = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;

    /*
     * Start a regular software-triggered conversion.
     */
    ADCx->CR |= ADC_CR_ADSTART;

    /*
     * Wait until the conversion completes.
     */
    while (!(ADCx->ISR & ADC_ISR_EOC));

    return (uint16_t)(ADCx->DR & ADC_MAX_VALUE);
}


/**
 * @brief Configure a channel, perform one conversion, and return its result.
 * @param ADCx ADC peripheral instance.
 * @param channel ADC channel to convert.
 * @param sample_time Sampling time for the selected channel.
 * @return 12-bit ADC conversion result in the range 0-4095.
 */
uint16_t readADCChannel(
    ADC_TypeDef *ADCx,
    const ADCChannel channel,
    const ADCSampleTime sample_time
)
{
    setADCChannel(ADCx, channel, sample_time);

    return readADC(ADCx);
}


/**
 * @brief Disable an ADC and return it to deep-power-down mode.
 * @param ADCx ADC peripheral instance.
 */
void uninitADC(ADC_TypeDef *ADCx)
{
    validateADCx(ADCx);

    if (ADCx->CR & ADC_CR_ADEN) {
        ADCx->CR |= ADC_CR_ADDIS;

        while (ADCx->CR & ADC_CR_ADEN);
    }

    ADCx->CR |= ADC_CR_DEEPPWD;

    setADCxClock(ADCx, STATE_DISABLE);
}
