#include "power.h"


void setRegulatorMode(const RegulatorMode regulator_mode)
{
    switch (regulator_mode) {
        case REGULATORMODE_SMPS:
            PWR->CR5 &= ~PWR_CR5_R1MODE;
            break;

        case REGULATORMODE_LDO:
            PWR->CR5 |= PWR_CR5_R1MODE;
            break;

        default:
            break;
    }
}

void setVoltageScalingAsync(const uint32_t voltage_scaling) {
    if (voltage_scaling != PWR_CR1_VOS_0 &&
        voltage_scaling != PWR_CR1_VOS_1 &&
        voltage_scaling != (PWR_CR1_VOS_0 | PWR_CR1_VOS_1))
        panic("Invalid voltage scaling.");

    PWR->CR1 =
        (PWR->CR1 & ~PWR_CR1_VOS) |
        voltage_scaling;
}

void setVoltageScaling(const uint32_t voltage_scaling)
{
    setVoltageScalingAsync(voltage_scaling);
    while (PWR->SR2 & PWR_SR2_VOSF);
}
