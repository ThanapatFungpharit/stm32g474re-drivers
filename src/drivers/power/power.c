#include "power.h"

#include "drivers/internal/drv_internal.h"


/* VOSF clears within a few tens of microseconds; allow generous margin. */
#define VOS_TIMEOUT_US 10000U

static const DrvClockGate PWR_GATE = { DRV_BUS_APB1_1, RCC_APB1ENR1_PWREN };


void power_enable_clock(void)
{
    if (!drv_clock_gate_is_enabled(PWR_GATE)) {
        drv_clock_gate_set(PWR_GATE, true);
    }
}

DrvStatus power_set_voltage_range(const PowerVoltageRange range)
{
    uint32_t vos;

    switch (range) {
        case POWER_RANGE_1: vos = PWR_CR1_VOS_0; break;
        case POWER_RANGE_2: vos = PWR_CR1_VOS_1; break;
        default:            return DRV_ERR_PARAM;
    }

    power_enable_clock();
    drv_reg_modify(&PWR->CR1, PWR_CR1_VOS, vos);

    return drv_wait_clear(&PWR->SR2, PWR_SR2_VOSF, VOS_TIMEOUT_US);
}

PowerVoltageRange power_get_voltage_range(void)
{
    power_enable_clock();
    return ((PWR->CR1 & PWR_CR1_VOS) == PWR_CR1_VOS_1) ? POWER_RANGE_2
                                                       : POWER_RANGE_1;
}

void power_set_boost(const bool enable)
{
    power_enable_clock();
    /* R1MODE = 0 selects boost mode, 1 selects normal mode. */
    drv_reg_modify(&PWR->CR5, enable ? PWR_CR5_R1MODE : 0U,
                   enable ? 0U : PWR_CR5_R1MODE);
}

bool power_is_boost_enabled(void)
{
    power_enable_clock();
    return (PWR->CR5 & PWR_CR5_R1MODE) == 0U;
}

void power_disable_ucpd_dead_battery(void)
{
    power_enable_clock();
    drv_reg_modify(&PWR->CR3, 0U, PWR_CR3_UCPD_DBDIS);
}
