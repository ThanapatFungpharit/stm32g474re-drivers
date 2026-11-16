#include "tim.h"

#include "drivers/internal/drv_internal.h"
#include "system/clock/clock.h"


typedef struct {
    TIM_TypeDef *tim;
    DrvClockGate gate;
    uint32_t     max_arr;
    uint8_t      channels;
    bool         main_output;
    IRQn_Type    update_irqn;
} TimInfo;

/* Built at run time (not a static table) so the CMSIS peripheral macros may
 * be redefined by the host test harness. */
static bool tim_info(const TIM_TypeDef *tim, TimInfo *info)
{
#define TIM_ENTRY(t, bus, bit, arr, ch, moe, irq)          \
    if (tim == (t)) {                                      \
        const TimInfo entry = { (t), { (bus), (bit) }, (arr), (ch), (moe), (irq) }; \
        *info = entry;                                     \
        return true;                                       \
    }

    TIM_ENTRY(TIM1,  DRV_BUS_APB2,   RCC_APB2ENR_TIM1EN,   0xFFFFUL,     4U, true,  TIM1_UP_TIM16_IRQn)
    TIM_ENTRY(TIM2,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM2EN,  0xFFFFFFFFUL, 4U, false, TIM2_IRQn)
    TIM_ENTRY(TIM3,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM3EN,  0xFFFFUL,     4U, false, TIM3_IRQn)
    TIM_ENTRY(TIM4,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM4EN,  0xFFFFUL,     4U, false, TIM4_IRQn)
    TIM_ENTRY(TIM5,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM5EN,  0xFFFFFFFFUL, 4U, false, TIM5_IRQn)
    TIM_ENTRY(TIM6,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM6EN,  0xFFFFUL,     0U, false, TIM6_DAC_IRQn)
    TIM_ENTRY(TIM7,  DRV_BUS_APB1_1, RCC_APB1ENR1_TIM7EN,  0xFFFFUL,     0U, false, TIM7_DAC_IRQn)
    TIM_ENTRY(TIM8,  DRV_BUS_APB2,   RCC_APB2ENR_TIM8EN,   0xFFFFUL,     4U, true,  TIM8_UP_IRQn)
    TIM_ENTRY(TIM15, DRV_BUS_APB2,   RCC_APB2ENR_TIM15EN,  0xFFFFUL,     2U, true,  TIM1_BRK_TIM15_IRQn)
    TIM_ENTRY(TIM16, DRV_BUS_APB2,   RCC_APB2ENR_TIM16EN,  0xFFFFUL,     1U, true,  TIM1_UP_TIM16_IRQn)
    TIM_ENTRY(TIM17, DRV_BUS_APB2,   RCC_APB2ENR_TIM17EN,  0xFFFFUL,     1U, true,  TIM1_TRG_COM_TIM17_IRQn)
    TIM_ENTRY(TIM20, DRV_BUS_APB2,   RCC_APB2ENR_TIM20EN,  0xFFFFUL,     4U, true,  TIM20_UP_IRQn)

#undef TIM_ENTRY
    return false;
}

bool tim_is_valid(const TIM_TypeDef *tim)
{
    TimInfo info;
    return tim_info(tim, &info);
}

DrvStatus tim_set_clock(TIM_TypeDef *tim, const bool enable)
{
    TimInfo info;
    if (!tim_info(tim, &info)) {
        return DRV_ERR_PARAM;
    }
    drv_clock_gate_set(info.gate, enable);
    return DRV_OK;
}

uint32_t tim_get_clock_hz(const TIM_TypeDef *tim)
{
    TimInfo info;
    if (!tim_info(tim, &info)) {
        return 0U;
    }
    return (info.gate.bus == DRV_BUS_APB2) ? clock_get_apb2_timer_hz()
                                           : clock_get_apb1_timer_hz();
}

uint32_t tim_get_max_arr(const TIM_TypeDef *tim)
{
    TimInfo info;
    return tim_info(tim, &info) ? info.max_arr : 0U;
}

uint32_t tim_get_channel_count(const TIM_TypeDef *tim)
{
    TimInfo info;
    return tim_info(tim, &info) ? info.channels : 0U;
}

bool tim_has_main_output(const TIM_TypeDef *tim)
{
    TimInfo info;
    return tim_info(tim, &info) && info.main_output;
}

IRQn_Type tim_update_irqn(const TIM_TypeDef *tim)
{
    TimInfo info;
    const bool valid = tim_info(tim, &info);
    DRV_ASSERT(valid);
    return valid ? info.update_irqn : HardFault_IRQn;
}

DrvStatus tim_calc_period(const uint32_t clock_hz, const uint32_t frequency_hz,
                          const uint32_t max_arr, uint32_t *psc, uint32_t *arr)
{
    if ((clock_hz == 0U) || (frequency_hz == 0U) || (max_arr == 0U) ||
        (psc == NULL) || (arr == NULL)) {
        return DRV_ERR_PARAM;
    }

    /* Timer clock cycles per period, rounded to nearest. */
    const uint64_t ticks = ((uint64_t)clock_hz + (frequency_hz / 2U)) / frequency_hz;
    if (ticks < 2U) {
        return DRV_ERR_UNSUPPORTED; /* ARR must be at least 1 */
    }

    const uint64_t span = (uint64_t)max_arr + 1U;
    const uint64_t div = (ticks + span - 1U) / span; /* PSC + 1, rounded up */
    if (div > (TIM_PSC_MAX + 1U)) {
        return DRV_ERR_UNSUPPORTED;
    }

    uint64_t period = (ticks + (div / 2U)) / div; /* ARR + 1 */
    if (period < 2U) {
        period = 2U;
    } else if (period > span) {
        period = span;
    }

    *psc = (uint32_t)(div - 1U);
    *arr = (uint32_t)(period - 1U);
    return DRV_OK;
}

DrvStatus tim_init(TIM_TypeDef *tim, const uint32_t psc, const uint32_t arr)
{
    TimInfo info;
    if (!tim_info(tim, &info) || (psc > TIM_PSC_MAX) || (arr == 0U) ||
        (arr > info.max_arr)) {
        return DRV_ERR_PARAM;
    }

    if (!drv_clock_gate_is_enabled(info.gate)) {
        drv_clock_gate_set(info.gate, true);
    }

    tim->CR1 &= ~TIM_CR1_CEN;

    /* Up-counting, edge aligned, ARR preloaded; only counter overflow (not
     * the software UG below) raises the update interrupt. */
    tim->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
    tim->PSC = psc;
    tim->ARR = arr;
    tim->EGR = TIM_EGR_UG;      /* load PSC and ARR, reset counter */
    tim->SR = ~(uint32_t)TIM_SR_UIF;      /* rc_w0: clear only UIF */
    return DRV_OK;
}

DrvStatus tim_init_frequency(TIM_TypeDef *tim, const uint32_t frequency_hz)
{
    uint32_t psc;
    uint32_t arr;

    if (!tim_is_valid(tim)) {
        return DRV_ERR_PARAM;
    }

    const DrvStatus status = tim_calc_period(tim_get_clock_hz(tim), frequency_hz,
                                             tim_get_max_arr(tim), &psc, &arr);
    if (status != DRV_OK) {
        return status;
    }
    return tim_init(tim, psc, arr);
}

DrvStatus tim_deinit(TIM_TypeDef *tim)
{
    TimInfo info;
    if (!tim_info(tim, &info)) {
        return DRV_ERR_PARAM;
    }

    if (drv_clock_gate_is_enabled(info.gate)) {
        tim->CR1 &= ~TIM_CR1_CEN;
        tim->DIER = 0U;
    }
    drv_clock_gate_reset(info.gate);
    drv_clock_gate_set(info.gate, false);
    return DRV_OK;
}

void tim_start(TIM_TypeDef *tim)
{
    DRV_ASSERT(tim_is_valid(tim));
    tim->CR1 |= TIM_CR1_CEN;
}

void tim_stop(TIM_TypeDef *tim)
{
    DRV_ASSERT(tim_is_valid(tim));
    tim->CR1 &= ~TIM_CR1_CEN;
}

uint32_t tim_get_counter(const TIM_TypeDef *tim)
{
    DRV_ASSERT(tim_is_valid(tim));
    /* Bit 31 is UIFCPY on 16-bit timers when UIFREMAP is set; mask it off. */
    return tim->CNT & tim_get_max_arr(tim);
}

void tim_set_counter(TIM_TypeDef *tim, const uint32_t value)
{
    DRV_ASSERT(tim_is_valid(tim));
    tim->CNT = value;
}

DrvStatus tim_enable_update_irq(TIM_TypeDef *tim, const uint32_t priority)
{
    TimInfo info;
    if (!tim_info(tim, &info) || (priority > DRV_NVIC_PRIORITY_MAX)) {
        return DRV_ERR_PARAM;
    }

    tim->SR = ~(uint32_t)TIM_SR_UIF;
    drv_reg_modify(&tim->DIER, 0U, TIM_DIER_UIE);
    drv_nvic_enable(info.update_irqn, priority);
    return DRV_OK;
}

DrvStatus tim_disable_update_irq(TIM_TypeDef *tim)
{
    if (!tim_is_valid(tim)) {
        return DRV_ERR_PARAM;
    }
    drv_reg_modify(&tim->DIER, TIM_DIER_UIE, 0U);
    return DRV_OK;
}

bool tim_take_update(TIM_TypeDef *tim)
{
    if ((tim->SR & TIM_SR_UIF) == 0U) {
        return false;
    }
    /* SR bits are rc_w0: writing 1 leaves other flags untouched. */
    tim->SR = ~(uint32_t)TIM_SR_UIF;
    return true;
}
