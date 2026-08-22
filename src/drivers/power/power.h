#ifndef NUCLEO_G474RE_DRIVER_POWER_H
#define NUCLEO_G474RE_DRIVER_POWER_H

#include "stm32g474xx.h"
#include "system/panic/panic.h"

typedef enum {
    REGULATORMODE_LDO,
    REGULATORMODE_SMPS,
} RegulatorMode;

/**
 * @brief Select the regulator power supply.
 * @param regulator_mode LDO or SMPS supply selection.
 * @note Updates the R1MODE bit in PWR->CR5.
 */
void setRegulatorMode(RegulatorMode regulator_mode);

typedef enum {
    VOLTAGE_SCALE_RANGE_1 = PWR_CR1_VOS_0,
    VOLTAGE_SCALE_RANGE_2 = PWR_CR1_VOS_1
} VoltageScaling;

/**
 * @brief Set the voltage scaling field without waiting.
 * @param voltage_scaling Voltage scaling bits for PWR->CR1.
 */
void setVoltageScalingAsync(uint32_t voltage_scaling);

/**
 * @brief Set the voltage scaling field and wait for completion.
 * @param voltage_scaling Voltage scaling bits for PWR->CR1.
 * @note Blocks while PWR->SR2 reports voltage-scaling activity.
 */
void setVoltageScaling(uint32_t voltage_scaling);

#endif //NUCLEO_G474RE_DRIVER_POWER_H
