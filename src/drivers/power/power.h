#ifndef NUCLEO_G474RE_DRIVER_POWER_H
#define NUCLEO_G474RE_DRIVER_POWER_H

#include "stm32g474xx.h"

typedef enum {
    POWERSUPPLY_LDO,
    POWERSUPPLY_SMPS,
} PowerSupply;

void setPowerSupply(PowerSupply power_supply);

#endif //NUCLEO_G474RE_DRIVER_POWER_H
