//
// Created by user on 8/19/2026.
//

#ifndef STM32G474RE_DRIVERS_TIM_H
#define STM32G474RE_DRIVERS_TIM_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"


/**
 * @brief Get the RCC clock-enable mask for a timer peripheral.
 * @param TIMx Timer peripheral instance.
 * @return Corresponding RCC clock-enable mask.
 */
uint32_t getTIMxClockEnableBit(const TIM_TypeDef *TIMx);

/**
 * @brief Enable or disable a timer peripheral clock.
 * @param TIMx Timer peripheral instance.
 * @param state Desired clock state.
 * @note Updates RCC->APB1ENR1 or RCC->APB2ENR according to the timer.
 */
void setTIMxClock(const TIM_TypeDef *TIMx, EnableState state);

#endif //STM32G474RE_DRIVERS_TIM_H
