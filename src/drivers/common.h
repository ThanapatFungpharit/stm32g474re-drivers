#ifndef STM32G474RE_DRIVERS_COMMON_H
#define STM32G474RE_DRIVERS_COMMON_H

/**
 * @file common.h
 * @brief Types and conventions shared by every driver in the library.
 *
 * Error-handling convention
 * -------------------------
 * - Functions that configure hardware or wait for it return ::DrvStatus.
 *   They validate every argument and return ::DRV_ERR_PARAM instead of
 *   touching hardware when an argument is invalid, and ::DRV_ERR_TIMEOUT
 *   when the hardware does not reach the expected state in bounded time.
 * - Fast-path functions (for example gpio_write(), dac_write(),
 *   pwm_set_compare()) return nothing. They check their arguments with
 *   DRV_ASSERT(), which calls panic() when assertions are enabled and
 *   compiles to nothing when DRV_ASSERT_ENABLED is defined as 0.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result of a driver operation. */
typedef enum {
    DRV_OK = 0,              /**< Operation completed successfully. */
    DRV_ERR_PARAM,           /**< An argument is out of range or invalid. */
    DRV_ERR_STATE,           /**< Peripheral is in the wrong state for this call. */
    DRV_ERR_TIMEOUT,         /**< Hardware did not respond within the time limit. */
    DRV_ERR_BUSY,            /**< Resource is busy (for example a full buffer). */
    DRV_ERR_UNSUPPORTED,     /**< Request is valid but cannot be met (for example an unreachable baud rate). */
    DRV_ERR_PARITY,          /**< Receiver detected a parity error. */
    DRV_ERR_FRAMING,         /**< Receiver detected a framing error (bad stop bit or break). */
    DRV_ERR_NOISE,           /**< Receiver detected noise on the line. */
    DRV_ERR_OVERRUN,         /**< Received data was lost because it was not read in time. */
} DrvStatus;

/** Timeout value meaning "wait without a time limit". */
#define DRV_WAIT_FOREVER UINT32_MAX

/**
 * @brief Get a short, constant, human-readable name for a status code.
 * @param status Status code.
 * @return Name such as "DRV_OK"; "DRV_ERR_?" for unknown values.
 */
const char *drv_status_str(DrvStatus status);

#ifndef DRV_ASSERT_ENABLED
#define DRV_ASSERT_ENABLED 1
#endif

/**
 * @brief Report a failed DRV_ASSERT() and halt through panic().
 * @param expr Text of the failed expression.
 * @param file Source file name.
 * @param line Source line number.
 */
_Noreturn void drv_assert_failed(const char *expr, const char *file, int line);

#if DRV_ASSERT_ENABLED
#define DRV_ASSERT(expr) \
    ((expr) ? (void)0 : drv_assert_failed(#expr, __FILE__, __LINE__))
#else
#define DRV_ASSERT(expr) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_COMMON_H */
