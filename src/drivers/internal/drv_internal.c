#include "drivers/internal/drv_internal.h"

#include "system/panic/panic.h"
#include "system/time/time.h"


const char *drv_status_str(const DrvStatus status)
{
    switch (status) {
        case DRV_OK:              return "DRV_OK";
        case DRV_ERR_PARAM:       return "DRV_ERR_PARAM";
        case DRV_ERR_STATE:       return "DRV_ERR_STATE";
        case DRV_ERR_TIMEOUT:     return "DRV_ERR_TIMEOUT";
        case DRV_ERR_BUSY:        return "DRV_ERR_BUSY";
        case DRV_ERR_UNSUPPORTED: return "DRV_ERR_UNSUPPORTED";
        case DRV_ERR_PARITY:      return "DRV_ERR_PARITY";
        case DRV_ERR_FRAMING:     return "DRV_ERR_FRAMING";
        case DRV_ERR_NOISE:       return "DRV_ERR_NOISE";
        case DRV_ERR_OVERRUN:     return "DRV_ERR_OVERRUN";
        default:                  return "DRV_ERR_?";
    }
}


_Noreturn void drv_assert_failed(const char *expr, const char *file, const int line)
{
    panic_at(expr, file, line);
}


/* --- RCC clock gating ------------------------------------------------------ */

static volatile uint32_t *enable_register(const DrvBus bus)
{
    switch (bus) {
        case DRV_BUS_AHB1:   return &RCC->AHB1ENR;
        case DRV_BUS_AHB2:   return &RCC->AHB2ENR;
        case DRV_BUS_APB1_1: return &RCC->APB1ENR1;
        case DRV_BUS_APB1_2: return &RCC->APB1ENR2;
        case DRV_BUS_APB2:   return &RCC->APB2ENR;
        default:             return NULL;
    }
}

static volatile uint32_t *reset_register(const DrvBus bus)
{
    switch (bus) {
        case DRV_BUS_AHB1:   return &RCC->AHB1RSTR;
        case DRV_BUS_AHB2:   return &RCC->AHB2RSTR;
        case DRV_BUS_APB1_1: return &RCC->APB1RSTR1;
        case DRV_BUS_APB1_2: return &RCC->APB1RSTR2;
        case DRV_BUS_APB2:   return &RCC->APB2RSTR;
        default:             return NULL;
    }
}

void drv_clock_gate_set(const DrvClockGate gate, const bool enable)
{
    volatile uint32_t *reg = enable_register(gate.bus);
    DRV_ASSERT(reg != NULL);

    drv_reg_modify(reg, enable ? 0U : gate.mask, enable ? gate.mask : 0U);
    /* Read back: guarantees the clock is running before the next access. */
    (void)*reg;
}

bool drv_clock_gate_is_enabled(const DrvClockGate gate)
{
    volatile uint32_t *reg = enable_register(gate.bus);
    DRV_ASSERT(reg != NULL);
    return (*reg & gate.mask) == gate.mask;
}

void drv_clock_gate_reset(const DrvClockGate gate)
{
    volatile uint32_t *reg = reset_register(gate.bus);
    DRV_ASSERT(reg != NULL);

    drv_reg_modify(reg, 0U, gate.mask);
    (void)*reg;
    drv_reg_modify(reg, gate.mask, 0U);
    (void)*reg;
}


/* --- Bounded waiting -------------------------------------------------------- */

DrvStatus drv_wait_bits(volatile const uint32_t *reg, const uint32_t mask,
                        const uint32_t expected, const uint32_t timeout_us)
{
    TimeTimeout timeout;
    time_timeout_start_us(&timeout, timeout_us);

    for (;;) {
        if ((*reg & mask) == expected) {
            return DRV_OK;
        }
        if (time_timeout_expired(&timeout)) {
            /* Check once more: the condition may have become true while the
             * timeout was being evaluated (for example after an interrupt). */
            return ((*reg & mask) == expected) ? DRV_OK : DRV_ERR_TIMEOUT;
        }
    }
}
