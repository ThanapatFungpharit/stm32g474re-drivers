#ifndef STM32G474RE_DRIVERS_EXTI_H
#define STM32G474RE_DRIVERS_EXTI_H

/**
 * @file exti.h
 * @brief GPIO edge interrupts through EXTI lines 0-15.
 *
 * EXTI line n can be connected to pin n of exactly one port at a time
 * (for example PA13 and PC13 cannot both use line 13).
 *
 * The driver does not own the interrupt vectors. Define the handler for the
 * line's IRQ (see exti_irqn()) in your application and acknowledge the line:
 *
 * @code
 * void EXTI15_10_IRQHandler(void)
 * {
 *     if (exti_take_pending(13U)) {
 *         button_pressed = true;
 *     }
 * }
 * @endcode
 *
 * Lines 5-9 and 10-15 share one IRQ each; check every line you enabled.
 */

#include <stdbool.h>
#include <stdint.h>

#include "stm32g474xx.h"
#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Edge(s) that trigger the interrupt. */
typedef enum {
    EXTI_TRIGGER_RISING  = 1U,
    EXTI_TRIGGER_FALLING = 2U,
    EXTI_TRIGGER_BOTH    = 3U,
} ExtiTrigger;

/**
 * @brief Route a GPIO pin to its EXTI line and enable the interrupt.
 * @param port GPIO port of the pin.
 * @param pin Pin number 0-15 (also the EXTI line number).
 * @param trigger Edge selection.
 * @param priority NVIC priority 0 (highest) to 15 (lowest).
 * @return DRV_OK or DRV_ERR_PARAM.
 * @note Enables the SYSCFG clock, clears a stale pending flag, and enables
 *       the IRQ in the NVIC. Configure the pin as an input (gpio_init_input())
 *       separately.
 */
DrvStatus exti_enable_gpio(GPIO_TypeDef *port, uint8_t pin, ExtiTrigger trigger,
                           uint32_t priority);

/**
 * @brief Mask EXTI line @p line and clear its edge selection.
 * @return DRV_OK or DRV_ERR_PARAM.
 * @note The shared NVIC IRQ stays enabled because other lines may use it.
 */
DrvStatus exti_disable(uint8_t line);

/**
 * @brief Check and clear the pending flag of a line. Call from the IRQ handler.
 * @return true when the line had a pending edge.
 */
bool exti_take_pending(uint8_t line);

/**
 * @brief NVIC interrupt number serving EXTI line @p line (0-15).
 * @note Asserts that the line is valid.
 */
IRQn_Type exti_irqn(uint8_t line);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_EXTI_H */
