#ifndef NUCLEO_G474RE_DRIVER_POWER_H
#define NUCLEO_G474RE_DRIVER_POWER_H

#include "stm32g474xx.h"

typedef enum {
    POWERSUPPLY_LDO,
    POWERSUPPLY_SMPS,
} PowerSupply;

/** @brief Select the regulator power supply. @param power_supply LDO or SMPS selection. */
void setPowerSupply(PowerSupply power_supply);

typedef enum {
    VOLTAGE_SCALE_RANGE_1 = PWR_CR1_VOS_0,
    VOLTAGE_SCALE_RANGE_2 = PWR_CR1_VOS_1
} VoltageScaling;

/** @brief Set voltage scaling without waiting. @param voltage_scaling Voltage scaling bits. */
void setVoltageScalingAsync(uint32_t voltage_scaling);

/** @brief Set voltage scaling and wait for completion. @param voltage_scaling Voltage scaling bits. */
void setVoltageScaling(uint32_t voltage_scaling);

#endif //NUCLEO_G474RE_DRIVER_POWER_H
