#include "pwm.h"


void initPWM(
    TIM_TypeDef *TIMx,
    const PWMChannel channel,
    const uint32_t prescaler,
    const uint32_t period)
{
    setTIMxClock(TIMx, STATE_ENABLE);

    TIMx->CR1 &= ~TIM_CR1_CEN;

    // timer_counter_clock = timer_clock / (PSC + 1)
    TIMx->PSC = prescaler;

    /* period = ARR + 1 timer ticks
     * counter runs 0 ... ARR
     */
    TIMx->ARR = period;

    /*
     * PWM mode 1:
     * OCxM = 110
     * OCxPE = 1
     */
    switch (channel) {
        case PWM_CHANNEL_1:
            TIMx->CCMR1 =
                (TIMx->CCMR1 &
                 ~(TIM_CCMR1_CC1S |
                   TIM_CCMR1_OC1M |
                   TIM_CCMR1_OC1PE)) |
                (6U << TIM_CCMR1_OC1M_Pos) |
                TIM_CCMR1_OC1PE;

            TIMx->CCR1 = 0;
            TIMx->CCER |= TIM_CCER_CC1E;
            break;

        case PWM_CHANNEL_2:
            TIMx->CCMR1 =
                (TIMx->CCMR1 &
                 ~(TIM_CCMR1_CC2S |
                   TIM_CCMR1_OC2M |
                   TIM_CCMR1_OC2PE)) |
                (6U << TIM_CCMR1_OC2M_Pos) |
                TIM_CCMR1_OC2PE;

            TIMx->CCR2 = 0;
            TIMx->CCER |= TIM_CCER_CC2E;
            break;

        case PWM_CHANNEL_3:
            TIMx->CCMR2 =
                (TIMx->CCMR2 &
                 ~(TIM_CCMR2_CC3S |
                   TIM_CCMR2_OC3M |
                   TIM_CCMR2_OC3PE)) |
                (6U << TIM_CCMR2_OC3M_Pos) |
                TIM_CCMR2_OC3PE;

            TIMx->CCR3 = 0;
            TIMx->CCER |= TIM_CCER_CC3E;
            break;

        case PWM_CHANNEL_4:
            TIMx->CCMR2 =
                (TIMx->CCMR2 &
                 ~(TIM_CCMR2_CC4S |
                   TIM_CCMR2_OC4M |
                   TIM_CCMR2_OC4PE)) |
                (6U << TIM_CCMR2_OC4M_Pos) |
                TIM_CCMR2_OC4PE;

            TIMx->CCR4 = 0;
            TIMx->CCER |= TIM_CCER_CC4E;
            break;

        default:
            return;
    }

    // ARR preload
    TIMx->CR1 |= TIM_CR1_ARPE;

    /*
     * Advanced-control timers require Main Output Enable.
     * TIM1/TIM8/TIM20 are advanced timers on G474.
     */
    if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM20)) {
        TIMx->BDTR |= TIM_BDTR_MOE;
    }

    // Generate an update event so PSC/ARR are loaded.
    TIMx->EGR = TIM_EGR_UG;

    // free timer
    TIMx->CR1 |= TIM_CR1_CEN;
}


void pwmWrite(
    TIM_TypeDef *TIMx,
    const PWMChannel channel,
    uint32_t value)
{
    if (value > TIMx->ARR) {
        value = TIMx->ARR;
    }

    switch (channel) {
        case PWM_CHANNEL_1:
            TIMx->CCR1 = value;
            break;

        case PWM_CHANNEL_2:
            TIMx->CCR2 = value;
            break;

        case PWM_CHANNEL_3:
            TIMx->CCR3 = value;
            break;

        case PWM_CHANNEL_4:
            TIMx->CCR4 = value;
            break;

        default:
            return;
    }
    // Transfer preloaded CCR value into the active register.
    TIMx->EGR = TIM_EGR_UG;
}
