//
// Created by user on 8/19/2026.
//

#ifndef STM32G474RE_DRIVERS_TIM_H
#define STM32G474RE_DRIVERS_TIM_H

#include "stm32g474xx.h"
#include "drivers/common.h"
#include "system/panic/panic.h"


/** @brief Get a timer peripheral's RCC clock mask. @param TIMx Timer peripheral. @return RCC clock-enable mask. */
uint32_t getTIMxClock(const TIM_TypeDef *TIMx);

/** @brief Set a timer peripheral clock state. @param TIMx Timer peripheral. @param state Desired clock state. */
void setTIMxClock(const TIM_TypeDef *TIMx, EnableState state);

#endif //STM32G474RE_DRIVERS_TIM_H
