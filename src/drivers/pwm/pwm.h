#ifndef STM32G474RE_DRIVERS_PWM_H
#define STM32G474RE_DRIVERS_PWM_H

#include "stm32g474xx.h"
#include "drivers/tim/tim.h"
#include "drivers/common.h"


typedef enum {
    PWM_CHANNEL_1 = 1,
    PWM_CHANNEL_2 = 2,
    PWM_CHANNEL_3 = 3,
    PWM_CHANNEL_4 = 4
} PWMChannel;

/**
 * @brief Configure and start PWM mode 1 on a timer channel.
 * @param TIMx Timer peripheral to configure.
 * @param channel PWM channel to configure.
 * @param prescaler Value to write to the timer PSC register.
 * @param period Value to write to the timer ARR register.
 * @note Enables the timer clock, configures the selected channel, generates an
 *       update event, and starts the timer. TIM1, TIM8, and TIM20 also have
 *       their main output enabled.
 *       Unsupported channel values return before the timer is started.
 */
void initPWM(TIM_TypeDef *TIMx, PWMChannel channel, uint32_t prescaler, uint32_t period);

/**
 * @brief Set a PWM channel compare value.
 * @param TIMx Timer peripheral.
 * @param channel PWM channel to update.
 * @param value Compare value to write.
 * @note Clamps value to TIMx->ARR and generates an update event after writing
 *       the selected compare register.
 *       Unsupported channel values return without updating a compare register.
 */
void pwmWrite(TIM_TypeDef *TIMx, PWMChannel channel, uint32_t value);

#endif //STM32G474RE_DRIVERS_PWM_H
