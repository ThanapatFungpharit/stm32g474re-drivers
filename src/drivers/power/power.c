#include "power.h"


/**
 * @brief Select the regulator power supply.
 * @param power_supply LDO or SMPS supply selection.
 * @note Updates the R1MODE bit in PWR->CR5.
 */
void setPowerSupply(PowerSupply power_supply)
{
    switch (power_supply) {
        case POWERSUPPLY_SMPS:
            PWR->CR5 &= ~PWR_CR5_R1MODE;
            break;

        case POWERSUPPLY_LDO:
            PWR->CR5 |= PWR_CR5_R1MODE;
            break;

        default:
            break;
    }
}

/**
 * @brief Set the voltage scaling field without waiting.
 * @param voltage_scaling Voltage scaling bits for PWR->CR1.
 */
void setVoltageScalingAsync(uint32_t voltage_scaling) {
    PWR->CR1 =
        (PWR->CR1 & ~PWR_CR1_VOS) |
        voltage_scaling;
}

/**
 * @brief Set the voltage scaling field and wait for completion.
 * @param voltage_scaling Voltage scaling bits for PWR->CR1.
 * @note Blocks while PWR->SR2 reports voltage-scaling activity.
 */
void setVoltageScaling(uint32_t voltage_scaling)
{
    setVoltageScalingAsync(voltage_scaling);
    while (PWR->SR2 & PWR_SR2_VOSF) {
    }
}
