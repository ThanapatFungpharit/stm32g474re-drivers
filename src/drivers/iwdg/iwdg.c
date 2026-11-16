#include "iwdg.h"

#include "stm32g474xx.h"
#include "drivers/internal/drv_internal.h"


#define KEY_ENABLE  0xCCCCUL
#define KEY_ACCESS  0x5555UL
#define KEY_REFRESH 0xAAAAUL

#define RELOAD_MAX  0xFFFUL
#define TIMEOUT_MAX_MS 32768UL

/* Register updates are synchronised to the LSI domain: up to ~5 LSI periods
 * plus LSI start-up; allow ample margin. */
#define UPDATE_TIMEOUT_US 50000U


DrvStatus iwdg_calc(const uint32_t timeout_ms, const uint32_t lsi_hz, IwdgSetting *setting)
{
    if ((setting == NULL) || (lsi_hz == 0U) || (timeout_ms == 0U) ||
        (timeout_ms > TIMEOUT_MAX_MS)) {
        return DRV_ERR_PARAM;
    }

    for (uint32_t code = 0U; code <= 6U; ++code) {
        const uint64_t divider = 4ULL << code;
        /* Counter ticks for the timeout, rounded up. */
        const uint64_t ticks = (((uint64_t)timeout_ms * lsi_hz) + (divider * 1000U) - 1U) /
                               (divider * 1000U);
        if ((ticks >= 1U) && (ticks <= (RELOAD_MAX + 1U))) {
            setting->prescaler_code = code;
            setting->reload = (uint32_t)ticks - 1U;
            setting->timeout_ms = (uint32_t)((ticks * divider * 1000U) / lsi_hz);
            return DRV_OK;
        }
    }
    return DRV_ERR_PARAM; /* only for a very fast LSI argument */
}

DrvStatus iwdg_start(const uint32_t timeout_ms, const bool freeze_in_debug)
{
    IwdgSetting setting;
    const DrvStatus status = iwdg_calc(timeout_ms, IWDG_LSI_HZ, &setting);
    if (status != DRV_OK) {
        return status;
    }

    if (freeze_in_debug) {
        drv_reg_modify(&DBGMCU->APB1FZR1, 0U, DBGMCU_APB1FZR1_DBG_IWDG_STOP);
    }

    IWDG->KR = KEY_ENABLE;  /* starts LSI and the watchdog */
    IWDG->KR = KEY_ACCESS;  /* unlock PR, RLR, WINR */
    IWDG->PR = setting.prescaler_code;
    IWDG->RLR = setting.reload;

    if (drv_wait_clear(&IWDG->SR, IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU,
                       UPDATE_TIMEOUT_US) != DRV_OK) {
        return DRV_ERR_TIMEOUT;
    }

    /* Window = max disables window mode; writing it also reloads the counter. */
    IWDG->WINR = RELOAD_MAX;
    if (drv_wait_clear(&IWDG->SR, IWDG_SR_WVU, UPDATE_TIMEOUT_US) != DRV_OK) {
        return DRV_ERR_TIMEOUT;
    }

    IWDG->KR = KEY_REFRESH;
    return DRV_OK;
}

void iwdg_refresh(void)
{
    IWDG->KR = KEY_REFRESH;
}

bool iwdg_caused_reset(void)
{
    return (RCC->CSR & RCC_CSR_IWDGRSTF) != 0U;
}

void iwdg_clear_reset_flags(void)
{
    drv_reg_modify(&RCC->CSR, 0U, RCC_CSR_RMVF);
}
