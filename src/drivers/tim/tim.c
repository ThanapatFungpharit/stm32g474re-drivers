//
// Created by user on 8/19/2026.
//

#include "tim.h"


uint32_t getTIMxClockEnableBit(const TIM_TypeDef *TIMx) {
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


static volatile uint32_t *getTIMxEnableRegister(const TIM_TypeDef *TIMx) {
    switch ((uintptr_t)TIMx) {
        case (uintptr_t)TIM2:
        case (uintptr_t)TIM3:
        case (uintptr_t)TIM4:
        case (uintptr_t)TIM5:
        case (uintptr_t)TIM6:
        case (uintptr_t)TIM7:
            return &RCC->APB1ENR1;

        case (uintptr_t)TIM1:
        case (uintptr_t)TIM8:
        case (uintptr_t)TIM15:
        case (uintptr_t)TIM16:
        case (uintptr_t)TIM17:
        case (uintptr_t)TIM20:
            return &RCC->APB2ENR;

        default:
            panic("Invalid tim in setTIMxClock()");
    }
}


void setTIMxClock(const TIM_TypeDef *TIMx, EnableState state) {
    const uint32_t tim = getTIMxClockEnableBit(TIMx);
    volatile uint32_t *enable_register = getTIMxEnableRegister(TIMx);

    switch (state) {
        case STATE_ENABLE:
            *enable_register |= tim;
            break;

        case STATE_DISABLE:
            *enable_register &= ~tim;
            break;

        default:
            // unreachable if types are respected
            panic("Invalid enable state in setTIMxClock()");
    }
    (void)*enable_register;
}
