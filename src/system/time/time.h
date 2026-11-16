#ifndef STM32G474RE_SYSTEM_TIME_H
#define STM32G474RE_SYSTEM_TIME_H

/**
 * @file time.h
 * @brief System time base, delays and timeouts.
 *
 * Two independent counters are used:
 *
 * - **SysTick** interrupts once per millisecond and drives time_millis() and
 *   time_micros(). SysTick keeps running in Sleep mode, so time keeps
 *   advancing while time_delay_ms() waits with WFI. Microseconds are
 *   interpolated from the SysTick down-counter.
 * - **DWT CYCCNT** counts core clock cycles and drives busy-wait delays
 *   (time_delay_us()) and timeouts (TimeTimeout). It does not need
 *   time_init() or interrupts, so drivers can use bounded waits from reset,
 *   inside interrupt handlers, and with interrupts masked.
 *
 * Call time_init() once after the clock is configured. clock_set_preset()
 * calls time_clock_changed() automatically, so time stays continuous and
 * monotonic across clock changes (with at most 1 ms of rounding per change).
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Initialisation --------------------------------------------------------- */

/**
 * @brief Start the 1 kHz SysTick time base and the DWT cycle counter.
 * @note Uses SystemCoreClock; the SysTick interrupt gets the lowest priority.
 *       Calling it again restarts the millisecond count from zero.
 */
void time_init(void);

/** @brief Return true once time_init() has run. */
bool time_is_initialized(void);

/**
 * @brief Reprogram SysTick after the core clock frequency changed.
 * @note Called by clock_set_preset(). Call it yourself only if you change
 *       the clock tree with your own code. Does nothing before time_init().
 */
void time_clock_changed(void);

/**
 * @brief SysTick exception handler (overrides the weak startup symbol).
 */
void SysTick_Handler(void);

/* --- Reading time ----------------------------------------------------------- */

/** @brief Milliseconds since time_init() (64-bit, never wraps in practice). */
uint64_t time_millis64(void);

/** @brief Microseconds since time_init() (64-bit). */
uint64_t time_micros64(void);

/** @brief Milliseconds since time_init(), wrapping every ~49.7 days. */
uint32_t time_millis(void);

/** @brief Microseconds since time_init(), wrapping every ~71.6 minutes. */
uint32_t time_micros(void);

/**
 * @brief Read the free-running 32-bit core cycle counter.
 * @note Enables the DWT cycle counter on first use. Wraps every
 *       2^32 / SystemCoreClock seconds (about 25 s at 170 MHz).
 */
uint32_t time_cycles(void);

/* --- Delays ----------------------------------------------------------------- */

/**
 * @brief Wait for at least @p ms milliseconds.
 * @note In thread mode with interrupts enabled and the time base running,
 *       the core sleeps (WFI) between SysTick interrupts. Otherwise (inside
 *       an interrupt handler, with interrupts masked, or before time_init())
 *       it falls back to a cycle-counting busy wait.
 */
void time_delay_ms(uint32_t ms);

/**
 * @brief Busy-wait for at least @p us microseconds.
 * @note Cycle accurate; works before time_init() and with interrupts masked.
 *       Interrupt handlers that run during the wait lengthen it.
 */
void time_delay_us(uint32_t us);

/* --- Timeouts --------------------------------------------------------------- */

/**
 * @brief State of a running timeout. Treat the fields as private.
 *
 * Elapsed cycles are accumulated in 64 bits on every check, so a timeout can
 * be longer than the 32-bit counter wrap period as long as it is checked
 * at least once per wrap period.
 */
typedef struct {
    uint32_t last;
    uint64_t elapsed;
    uint64_t limit;
    bool     forever;
} TimeTimeout;

/**
 * @brief Start a timeout of @p us microseconds.
 * @param timeout Timeout state to initialise.
 * @param us Duration; DRV_WAIT_FOREVER (UINT32_MAX) never expires and 0
 *           expires on the first check.
 */
void time_timeout_start_us(TimeTimeout *timeout, uint32_t us);

/** @brief Start a timeout of @p ms milliseconds (UINT32_MAX never expires). */
void time_timeout_start_ms(TimeTimeout *timeout, uint32_t ms);

/** @brief Return true once the timeout has elapsed. */
bool time_timeout_expired(TimeTimeout *timeout);

/**
 * @brief Convert a duration to core clock cycles.
 * @param us Duration in microseconds.
 * @param clock_hz Core clock in hertz.
 * @return Cycle count, rounded up.
 */
uint64_t time_us_to_cycles(uint64_t us, uint32_t clock_hz);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_SYSTEM_TIME_H */
