#include "power.h"


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