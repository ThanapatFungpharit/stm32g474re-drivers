#ifndef NUCLEO_G474RE_DRIVER_ADC_H
#define NUCLEO_G474RE_DRIVER_ADC_H

#include "stm32g474xx.h"
#include "system/panic/panic.h"
#include "drivers/common.h"


#define ADC_MAX_VALUE 4095U


typedef enum {
    ADC_CHANNEL_1  = 0U,
    ADC_CHANNEL_2  = 1U,
    ADC_CHANNEL_3  = 2U,
    ADC_CHANNEL_4  = 3U,
    ADC_CHANNEL_5  = 4U,
    ADC_CHANNEL_6  = 5U,
    ADC_CHANNEL_7  = 6U,
    ADC_CHANNEL_8  = 7U,
    ADC_CHANNEL_9  = 8U,
    ADC_CHANNEL_10 = 9U,
    ADC_CHANNEL_11 = 10U,
    ADC_CHANNEL_12 = 11U,
    ADC_CHANNEL_13 = 12U,
    ADC_CHANNEL_14 = 13U,
    ADC_CHANNEL_15 = 14U,
    ADC_CHANNEL_16 = 15U,
    ADC_CHANNEL_17 = 16U,
    ADC_CHANNEL_18 = 17U,
} ADCChannel;

typedef enum {
    ADC_SAMPLE_2C5 = 0U,
    ADC_SAMPLE_6C5  = 1U,
    ADC_SAMPLE_12C5 = 2U,
    ADC_SAMPLE_24C5 = 3U,
    ADC_SAMPLE_47C5 = 4U,
    ADC_SAMPLE_92C5 = 5U,
    ADC_SAMPLE_247C5 = 6U,
    ADC_SAMPLE_640C5 = 7U,
} ADCSampleTime;


/**
 * @brief Enable or disable an ADC peripheral clock.
 * @param ADCx ADC peripheral instance.
 * @param state Desired clock state.
 * @note Modifies RCC->AHB2ENR and reads it back after the write.
 */
void setADCxClock(const ADC_TypeDef *ADCx, EnableState state);


/**
 * @brief Calibrate an ADC peripheral.
 * @param ADCx ADC peripheral instance.
 * @note The ADC must be disabled before calibration.
 */
void calibrateADC(ADC_TypeDef *ADCx);


/**
 * @brief Initialize an ADC peripheral.
 * @param ADCx ADC peripheral instance.
 * @note Enables the ADC voltage regulator, calibrates the ADC, and enables the ADC.
 */
void initADC(ADC_TypeDef *ADCx);


/**
 * @brief Uninitialize an ADC peripheral.
 * @param ADCx ADC peripheral instance.
 * @note Disables the ADC and places it in deep-power-down mode.
 */
void uninitADC(ADC_TypeDef *ADCx);


void setADCChannel(
    ADC_TypeDef *ADCx,
    ADCChannel channel,
    ADCSampleTime sample_time
);

uint16_t readADC(ADC_TypeDef *ADCx);

uint16_t readADCChannel(
    ADC_TypeDef *ADCx,
    ADCChannel channel,
    ADCSampleTime sample_time
);

#endif // NUCLEO_G474RE_DRIVER_ADC_H