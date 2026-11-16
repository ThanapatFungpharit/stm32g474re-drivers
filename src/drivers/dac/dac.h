#ifndef STM32G474RE_DRIVERS_DAC_H
#define STM32G474RE_DRIVERS_DAC_H

/**
 * @file dac.h
 * @brief 12-bit DAC output with software updates (no trigger, no DMA).
 *
 * | Instance | Channel 1           | Channel 2           |
 * |----------|---------------------|---------------------|
 * | DAC1     | PA4 (buffered pin)  | PA5 (buffered pin)  |
 * | DAC2     | PA6 (buffered pin)  | —                   |
 * | DAC3     | internal only       | internal only       |
 * | DAC4     | internal only       | internal only       |
 *
 * DAC1/DAC2 drive their pin through the output buffer. Configure the pin as
 * analog first (gpio_init_analog()). DAC3/DAC4 have no pins; they feed the
 * comparators and op-amps and run with the buffer disabled.
 *
 * On the Nucleo-G474RE, PA5 also drives the green LED LD2, which loads
 * DAC1 channel 2.
 *
 * The DAC's bus interface needs a "high frequency" setting when HCLK is
 * above 80 MHz; dac_init() selects it from the current HCLK, so
 * re-initialise the DAC after raising the system clock.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest 12-bit output code. */
#define DAC_MAX_VALUE 4095U

/** DAC channel. */
typedef enum {
    DAC_CHANNEL_1 = 1,
    DAC_CHANNEL_2 = 2,
} DacChannel;

/**
 * @brief Enable a DAC channel with output code 0.
 * @param dac DAC1 ... DAC4.
 * @param channel Channel (DAC2 has channel 1 only).
 * @return DRV_OK, DRV_ERR_PARAM, DRV_ERR_STATE (HCLK changed while the
 *         other channel of the instance runs with an unsuitable interface
 *         setting), or DRV_ERR_TIMEOUT if the channel does not report ready.
 * @note Enables the DAC bus clock.
 */
DrvStatus dac_init(DAC_TypeDef *dac, DacChannel channel);

/**
 * @brief Disable a DAC channel; gates the clock when both channels are off.
 */
DrvStatus dac_deinit(DAC_TypeDef *dac, DacChannel channel);

/**
 * @brief Set the output code of an enabled channel.
 * @param value 0 to 4095 (Vout = VREF+ × value / 4096; VREF+ = VDDA = 3.3 V
 *        on the Nucleo).
 * @note Takes effect one bus clock later. Asserts valid arguments.
 */
void dac_write(DAC_TypeDef *dac, DacChannel channel, uint16_t value);

/**
 * @brief Convert millivolts to a DAC code.
 * @param mv Wanted output voltage.
 * @param vref_mv Reference voltage (VREF+).
 * @return Code, rounded to nearest and clamped to 0..4095.
 */
uint16_t dac_mv_to_code(uint32_t mv, uint32_t vref_mv);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_DAC_H */
