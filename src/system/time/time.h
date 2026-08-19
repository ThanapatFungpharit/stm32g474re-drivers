//
// Created by user on 8/19/2026.
//

#ifndef STM32G474RE_DRIVERS_TIME_H
#define STM32G474RE_DRIVERS_TIME_H

#include "stm32g474xx.h"


// === VARIABLE ===
/** @brief Check DWT cycle-counter availability. @return Nonzero when available. */
uint8_t systick_has_cycle_counter(void);

/** @brief Get the extended DWT cycle count. @return Current cycle count or zero. */
uint64_t dwt_cycle_count64(void);

/** @brief Get elapsed microseconds. @return Current 64-bit microsecond count. */
uint64_t micros64(void);

/** @brief Get elapsed milliseconds. @return Current 64-bit millisecond count. */
uint64_t millis64(void);

/** @brief Get elapsed microseconds. @return 32-bit microsecond count. */
uint32_t micros(void);

/** @brief Get elapsed milliseconds. @return 32-bit millisecond count. */
uint32_t millis(void);

// === API ===
/** @brief Initialize SysTick and timing state. */
void systick_init(void);

/** @brief Reconfigure timing after a core-clock change. */
void systick_clock_updated(void);

/** @brief Handle a SysTick interrupt. */
void SysTick_Handler(void);

/** @brief Delay execution in milliseconds. @param ms Milliseconds to delay. */
void delay_ms(uint32_t ms);

/** @brief Delay execution in microseconds. @param us Microseconds to delay. */
void delay_us(uint32_t us);

#endif //STM32G474RE_DRIVERS_TIME_H
