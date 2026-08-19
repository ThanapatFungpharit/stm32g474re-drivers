//
// Created by user on 8/19/2026.
//

#include "tim.h"


/**
 * @brief Get the RCC clock-enable mask for a timer peripheral.
 *
 * Maps a timer peripheral instance to its clock-enable bit. APB1
 * timers (TIM2, TIM3, TIM4, TIM5, TIM6, TIM7) map to bits in
 * RCC->APB1ENR1; APB2 timers (TIM1, TIM8, TIM15, TIM16, TIM17,
 * TIM20) map to bits in RCC->APB2ENR.
 *
 * @param TIMx Timer peripheral instance.
 *
 * @return Clock-enable bit mask, or 0 if TIMx is not recognized.
 */
uint32_t getTIMxClock(const TIM_TypeDef *TIMx) {
    switch ((uintptr_t)TIMx) {
        /* APB1 */
        case (uintptr_t)TIM2:
            return RCC_APB1ENR1_TIM2EN;
        case (uintptr_t)TIM3:
            return RCC_APB1ENR1_TIM3EN;
        case (uintptr_t)TIM4:
            return RCC_APB1ENR1_TIM4EN;
        case (uintptr_t)TIM5:
            return RCC_APB1ENR1_TIM5EN;
        case (uintptr_t)TIM6:
            return RCC_APB1ENR1_TIM6EN;
        case (uintptr_t)TIM7:
            return RCC_APB1ENR1_TIM7EN;

        /* APB2 */
        case (uintptr_t)TIM1:
            return RCC_APB2ENR_TIM1EN;
        case (uintptr_t)TIM8:
            return RCC_APB2ENR_TIM8EN;
        case (uintptr_t)TIM15:
            return RCC_APB2ENR_TIM15EN;
        case (uintptr_t)TIM16:
            return RCC_APB2ENR_TIM16EN;
        case (uintptr_t)TIM17:
            return RCC_APB2ENR_TIM17EN;
        case (uintptr_t)TIM20:
            return RCC_APB2ENR_TIM20EN;

        default:
            return 0;
    }
}

/**
 * @brief Enable or disable a timer peripheral's clock.
 *
 * Looks up the clock-enable bit for @p TIMx via getTIMxClock() and
 * sets or clears it.
 *
 * @param TIMx Timer peripheral instance.
 * @param state Desired state:
 *        - STATE_ENABLE: enable the timer's clock.
 *        - STATE_DISABLE: disable the timer's clock.
 */
void setTIMxClock(const TIM_TypeDef *TIMx, EnableState state) {
    const uint32_t tim = getTIMxClock(TIMx);
    volatile uint32_t *enable_register;

    switch ((uintptr_t)TIMx) {
        case (uintptr_t)TIM2:
        case (uintptr_t)TIM3:
        case (uintptr_t)TIM4:
        case (uintptr_t)TIM5:
        case (uintptr_t)TIM6:
        case (uintptr_t)TIM7:
            enable_register = &RCC->APB1ENR1;
            break;

        case (uintptr_t)TIM1:
        case (uintptr_t)TIM8:
        case (uintptr_t)TIM15:
        case (uintptr_t)TIM16:
        case (uintptr_t)TIM17:
        case (uintptr_t)TIM20:
            enable_register = &RCC->APB2ENR;
            break;

        default:
            panic("Invalid tim in toggleTIMxClock()");
    }

    switch (state) {
        case STATE_ENABLE:
            *enable_register |= tim;
            break;

        case STATE_DISABLE:
            *enable_register &= ~tim;
            break;

        default:
            // unreachable if types are respected
            panic("Invalid enable state in toggleTIMxClock()");
    }
    (void)*enable_register;
}
