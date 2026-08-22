//
// Created by user on 8/19/2026.
//

#ifndef STM32G474RE_DRIVERS_TIME_H
#define STM32G474RE_DRIVERS_TIME_H

#include "stm32g474xx.h"
#include "stdbool.h"


// === VARIABLE ===
/**
 * @brief Check whether the DWT cycle counter is available.
 * @return Nonzero when the DWT cycle counter is available.
 */
uint8_t systick_has_cycle_counter(void);

/**
 * @brief Get the extended DWT cycle count.
 * @return Current 64-bit cycle count, or zero when unavailable.
 */
uint64_t dwt_cycle_count64(void);

/**
 * @brief Get elapsed time in microseconds.
 * @return Current 64-bit microsecond count.
 */
uint64_t micros64(void);

/**
 * @brief Get elapsed time in milliseconds.
 * @return Current 64-bit millisecond count.
 */
uint64_t millis64(void);

/**
 * @brief Get the low 32 bits of the microsecond count.
 * @return Current microsecond count truncated to 32 bits.
 */
uint32_t micros(void);

/**
 * @brief Get the low 32 bits of the millisecond count.
 * @return Current millisecond count truncated to 32 bits.
 */
uint32_t millis(void);

// === API ===
/**
 * @brief Initialize SysTick and timing state.
 * @note Updates SystemCoreClock, configures DWT, and programs SysTick.
 */
void systick_init(void);

/**
 * @brief Reconfigure timing after a core-clock change.
 * @note Temporarily disables interrupts while updating timing state and SysTick.
 */
void systick_clock_updated(void);

/**
 * @brief Handle a SysTick interrupt.
 * @note Updates the cycle or millisecond time base.
 */
void SysTick_Handler(void);

/**
 * @brief Delay execution for a number of milliseconds.
 * @param ms Milliseconds to delay.
 * @note Waits for the time base and enters sleep while it advances.
 */
void delay_ms(uint32_t ms);

/**
 * @brief Delay execution for a number of microseconds.
 * @param us Microseconds to delay.
 */
void delay_us(uint32_t us);

#endif //STM32G474RE_DRIVERS_TIME_H
