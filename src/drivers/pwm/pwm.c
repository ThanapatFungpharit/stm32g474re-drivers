#include "pwm.h"

#include "drivers/internal/drv_internal.h"


/* OCxM = 0110: PWM mode 1 (active while CNT < CCRx). */
#define OCM_PWM1 6UL

static bool channel_valid(const TIM_TypeDef *tim, const PwmChannel channel)
{
    return ((uint32_t)channel >= 1U) &&
           ((uint32_t)channel <= tim_get_channel_count(tim));
}

static volatile uint32_t *ccr_register(TIM_TypeDef *tim, const PwmChannel channel)
{
    switch (channel) {
        case PWM_CHANNEL_1: return &tim->CCR1;
        case PWM_CHANNEL_2: return &tim->CCR2;
        case PWM_CHANNEL_3: return &tim->CCR3;
        case PWM_CHANNEL_4: return &tim->CCR4;
        default:            return NULL;
    }
}

DrvStatus pwm_init(TIM_TypeDef *tim, const uint32_t psc, const uint32_t arr)
{
    if (tim_get_channel_count(tim) == 0U) {
        return DRV_ERR_PARAM;
    }

    const DrvStatus status = tim_init(tim, psc, arr);
    if (status != DRV_OK) {
        return status;
    }

    if (tim_has_main_output(tim)) {
        drv_reg_modify(&tim->BDTR, 0U, TIM_BDTR_MOE);
    }

    tim_start(tim);
    return DRV_OK;
}

DrvStatus pwm_init_frequency(TIM_TypeDef *tim, const uint32_t frequency_hz)
{
    uint32_t psc;
    uint32_t arr;

    if (tim_get_channel_count(tim) == 0U) {
        return DRV_ERR_PARAM;
    }

    const DrvStatus status = tim_calc_period(tim_get_clock_hz(tim), frequency_hz,
                                             tim_get_max_arr(tim), &psc, &arr);
    if (status != DRV_OK) {
        return status;
    }
    return pwm_init(tim, psc, arr);
}

DrvStatus pwm_enable_channel(TIM_TypeDef *tim, const PwmChannel channel,
                             const PwmPolarity polarity)
{
    if (!channel_valid(tim, channel) ||
        ((polarity != PWM_POLARITY_ACTIVE_HIGH) && (polarity != PWM_POLARITY_ACTIVE_LOW))) {
        return DRV_ERR_PARAM;
    }

    const uint32_t index = (uint32_t)channel - 1U;          /* 0..3 */
    volatile uint32_t *ccmr = (index < 2U) ? &tim->CCMR1 : &tim->CCMR2;
    const uint32_t shift = (index & 1U) * 8U;               /* channel 2/4 fields are 8 bits up */

    /* CCxS = 00 (output), OCxM = PWM mode 1, OCxPE = 1 (preload).
     * OCxM bit 3 sits at bit 16 (+shift) on the STM32G4. */
    const uint32_t clear = (TIM_CCMR1_CC1S | TIM_CCMR1_OC1FE | TIM_CCMR1_OC1PE |
                            TIM_CCMR1_OC1M | TIM_CCMR1_OC1CE) << shift;
    const uint32_t set = ((OCM_PWM1 << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE) << shift;

    const uint32_t ccer_shift = index * 4U;
    const uint32_t ccer_enable = TIM_CCER_CC1E << ccer_shift;
    const uint32_t ccer_polarity = TIM_CCER_CC1P << ccer_shift;

    /* The channel must be disabled while CCxS is written. */
    drv_reg_modify(&tim->CCER, ccer_enable, 0U);
    drv_reg_modify(ccmr, clear, set);
    *ccr_register(tim, channel) = 0U;
    drv_reg_modify(&tim->CCER, ccer_polarity,
                   ((polarity == PWM_POLARITY_ACTIVE_LOW) ? ccer_polarity : 0U) | ccer_enable);
    return DRV_OK;
}

DrvStatus pwm_disable_channel(TIM_TypeDef *tim, const PwmChannel channel)
{
    if (!channel_valid(tim, channel)) {
        return DRV_ERR_PARAM;
    }
    const uint32_t index = (uint32_t)channel - 1U;
    drv_reg_modify(&tim->CCER, TIM_CCER_CC1E << (index * 4U), 0U);
    return DRV_OK;
}

uint32_t pwm_get_period_ticks(const TIM_TypeDef *tim)
{
    DRV_ASSERT(tim_is_valid(tim));
    /* ARR is at most 0xFFFFFFFF on TIM2/5; the period then saturates. */
    return (tim->ARR == UINT32_MAX) ? UINT32_MAX : (tim->ARR + 1U);
}

uint32_t pwm_duty_to_compare(const uint32_t period_ticks, uint32_t permille)
{
    if (permille > 1000U) {
        permille = 1000U;
    }
    return (uint32_t)((((uint64_t)period_ticks * permille) + 500U) / 1000U);
}

void pwm_set_compare(TIM_TypeDef *tim, const PwmChannel channel, uint32_t value)
{
    DRV_ASSERT(channel_valid(tim, channel));

    const uint32_t period = pwm_get_period_ticks(tim);
    if (value > period) {
        value = period;
    }
    /* CCR > ARR keeps the output permanently active (100 % duty). No update
     * event is generated: the preloaded value is applied at the next period
     * boundary without disturbing the running counter. */
    *ccr_register(tim, channel) = value;
}

void pwm_set_duty_permille(TIM_TypeDef *tim, const PwmChannel channel,
                           const uint32_t permille)
{
    pwm_set_compare(tim, channel, pwm_duty_to_compare(pwm_get_period_ticks(tim), permille));
}
