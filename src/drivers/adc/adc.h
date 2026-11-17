#ifndef STM32G474RE_DRIVERS_ADC_H
#define STM32G474RE_DRIVERS_ADC_H

/**
 * @file adc.h
 * @brief Single-ended, software-triggered conversions on ADC1-ADC5.
 *
 * Channel numbers follow the datasheet pin names: pin ADCx_INn is channel
 * n. For example PA0 = ADC1_IN1, so read it with channel 1 on ADC1. Put
 * the pin in analog mode first (gpio_init_analog(); analog is also the
 * reset state of most pins).
 *
 * Nucleo-G474RE Arduino analog header:
 *
 * | Header | Pin | ADC channel     |
 * |--------|-----|-----------------|
 * | A0     | PA0 | ADC1_IN1 / ADC2_IN1 |
 * | A1     | PA1 | ADC1_IN2 / ADC2_IN2 |
 * | A2     | PA4 | ADC2_IN17       |
 * | A3     | PB0 | ADC1_IN15 / ADC3_IN12 |
 * | A4     | PC1 | ADC1_IN7 / ADC2_IN7 |
 * | A5     | PC0 | ADC1_IN6 / ADC2_IN6 |
 *
 * Internal channels (enable them with adc_enable_internal_channels()):
 *
 * | Channel                      | Number | Available on          |
 * |------------------------------|-------:|-----------------------|
 * | ADC_CHANNEL_TEMPSENSOR_ADC1  | 16     | ADC1                  |
 * | ADC_CHANNEL_TEMPSENSOR_ADC5  | 4      | ADC5                  |
 * | ADC_CHANNEL_VBAT             | 17     | ADC1, ADC3, ADC5      |
 * | ADC_CHANNEL_VREFINT          | 18     | ADC1, ADC3, ADC4, ADC5|
 *
 * ADC clock: the driver selects the synchronous clock HCLK/2 when
 * HCLK <= 120 MHz and HCLK/4 above, keeping the ADC clock at or below
 * 60 MHz. ADC1/ADC2 and ADC3/ADC4/ADC5 each share one clock setting, which
 * is only written while every ADC of the group is disabled. Re-initialise
 * the ADC after changing the system clock.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest 12-bit conversion result. */
#define ADC_MAX_VALUE 4095U

/** Highest channel number (ADCx_IN18). */
#define ADC_CHANNEL_MAX 18U

#define ADC_CHANNEL_TEMPSENSOR_ADC1 16U /**< Temperature sensor on ADC1. */
#define ADC_CHANNEL_TEMPSENSOR_ADC5  4U /**< Temperature sensor on ADC5. */
#define ADC_CHANNEL_VBAT            17U /**< VBAT / 3 on ADC1, ADC3, ADC5. */
#define ADC_CHANNEL_VREFINT         18U /**< Internal reference on ADC1, ADC3, ADC4, ADC5. */

/** Default conversion timeout used by adc_read_channel(). */
#define ADC_DEFAULT_TIMEOUT_US 1000U

/**
 * Sampling time in ADC clock cycles. Longer sampling suits high-impedance
 * sources; the internal channels need at least the datasheet minimum
 * (temperature sensor 5 µs, VREFINT 4 µs: use ADC_SAMPLE_640_5 at up to
 * 60 MHz ADC clock).
 */
typedef enum {
    ADC_SAMPLE_2_5   = 0U,
    ADC_SAMPLE_6_5   = 1U,
    ADC_SAMPLE_12_5  = 2U,
    ADC_SAMPLE_24_5  = 3U,
    ADC_SAMPLE_47_5  = 4U,
    ADC_SAMPLE_92_5  = 5U,
    ADC_SAMPLE_247_5 = 6U,
    ADC_SAMPLE_640_5 = 7U,
} AdcSampleTime;

/**
 * @brief Power up, calibrate and enable an ADC for 12-bit single conversions.
 * @param adc ADC1 ... ADC5.
 * @return DRV_OK; DRV_ERR_PARAM for an invalid instance; DRV_ERR_STATE if
 *         the shared ADC clock cannot be configured because another ADC of
 *         the group is enabled with no clock selected; DRV_ERR_TIMEOUT if
 *         calibration or enabling does not complete.
 * @note If the ADC is already enabled it is first disabled and
 *       recalibrated.
 */
DrvStatus adc_init(ADC_TypeDef *adc);

/**
 * @brief Disable an ADC and put it in deep-power-down.
 * @note The shared bus clock is turned off only when the other ADCs of the
 *       group are disabled too.
 */
DrvStatus adc_deinit(ADC_TypeDef *adc);

/** @brief Return true when the ADC is enabled (ADEN set). */
bool adc_is_enabled(const ADC_TypeDef *adc);

/**
 * @brief Select the channel for the next conversion and its sampling time.
 * @param adc Enabled ADC.
 * @param channel Channel 0-18.
 * @param sample_time Sampling time.
 * @return DRV_OK, DRV_ERR_PARAM, or DRV_ERR_STATE if the ADC is disabled or
 *         converting.
 */
DrvStatus adc_config_channel(ADC_TypeDef *adc, uint8_t channel, AdcSampleTime sample_time);

/**
 * @brief Run one conversion on the configured channel.
 * @param adc Enabled ADC.
 * @param[out] value Result 0-4095.
 * @param timeout_us Time limit for the conversion.
 * @return DRV_OK, DRV_ERR_PARAM, DRV_ERR_STATE or DRV_ERR_TIMEOUT.
 */
DrvStatus adc_read(ADC_TypeDef *adc, uint16_t *value, uint32_t timeout_us);

/**
 * @brief Configure a channel and convert it once (ADC_DEFAULT_TIMEOUT_US).
 */
DrvStatus adc_read_channel(ADC_TypeDef *adc, uint8_t channel, AdcSampleTime sample_time,
                           uint16_t *value);

/**
 * @brief Enable or disable the internal measurement paths of an ADC group.
 * @param adc Any ADC of the group (ADC1/2 or ADC3/4/5).
 * @param vrefint Connect the internal voltage reference.
 * @param temp_sensor Connect the temperature sensor.
 * @param vbat Connect the VBAT/3 divider (drains the backup battery while on).
 * @return DRV_OK, DRV_ERR_PARAM, or DRV_ERR_STATE if any ADC of the group is
 *         enabled (the common register may then not be written).
 * @note Waits for the sensors' start-up time (up to 120 µs) when enabling.
 */
DrvStatus adc_enable_internal_channels(ADC_TypeDef *adc, bool vrefint, bool temp_sensor,
                                       bool vbat);

/**
 * @brief Compute VDDA from a VREFINT conversion.
 * @param vrefint_raw 12-bit VREFINT reading.
 * @param vrefint_cal Factory calibration value (VREFINT_CAL, taken at 3.0 V).
 * @return VDDA in millivolts, or 0 when @p vrefint_raw is 0.
 */
uint32_t adc_calc_vdda_mv(uint16_t vrefint_raw, uint16_t vrefint_cal);

/**
 * @brief Convert a 12-bit reading to millivolts.
 * @param raw Conversion result.
 * @param vdda_mv Reference voltage (VDDA, 3300 on the Nucleo).
 */
uint32_t adc_calc_mv(uint16_t raw, uint32_t vdda_mv);

/**
 * @brief Compute the die temperature from a temperature-sensor reading.
 * @param raw 12-bit reading of the temperature sensor channel.
 * @param vdda_mv VDDA during the measurement.
 * @param ts_cal1 Factory value at 30 °C (TS_CAL1, taken at 3.0 V).
 * @param ts_cal2 Factory value at 130 °C (TS_CAL2, taken at 3.0 V).
 * @return Temperature in hundredths of a degree Celsius.
 */
int32_t adc_calc_temperature_centi_c(uint16_t raw, uint32_t vdda_mv,
                                     uint16_t ts_cal1, uint16_t ts_cal2);

/** @brief Factory VREFINT calibration value read from system memory. */
uint16_t adc_vrefint_cal(void);

/** @brief Factory temperature-sensor calibration value at 30 °C. */
uint16_t adc_ts_cal1(void);

/** @brief Factory temperature-sensor calibration value at 130 °C. */
uint16_t adc_ts_cal2(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_ADC_H */
