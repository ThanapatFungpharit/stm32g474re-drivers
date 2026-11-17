#ifndef STM32G474RE_DRIVERS_IWDG_H
#define STM32G474RE_DRIVERS_IWDG_H

/**
 * @file iwdg.h
 * @brief Independent watchdog: resets the MCU unless refreshed in time.
 *
 * The IWDG runs from the LSI oscillator (32 kHz nominal; the datasheet
 * allows roughly 29.5-34 kHz), so the real timeout can differ from the
 * requested one by several percent. Refresh comfortably more often than
 * the timeout.
 *
 * Once started the watchdog cannot be stopped except by a reset.
 */

#include <stdbool.h>
#include <stdint.h>

#include "drivers/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Nominal LSI frequency used for timeout calculations. */
#define IWDG_LSI_HZ 32000UL

/** Settings computed by iwdg_calc(). */
typedef struct {
    uint32_t prescaler_code; /**< PR register value 0..6 (divider 4 << code). */
    uint32_t reload;         /**< RLR register value 0..4095. */
    uint32_t timeout_ms;     /**< Timeout these settings produce at the nominal LSI. */
} IwdgSetting;

/**
 * @brief Compute prescaler and reload for a timeout.
 * @param timeout_ms Wanted timeout, 1 ms to 32768 ms.
 * @param lsi_hz LSI frequency to assume.
 * @param[out] setting Result.
 * @return DRV_OK, or DRV_ERR_PARAM if the timeout is out of range.
 * @note Picks the smallest prescaler (finest resolution); the resulting
 *       timeout is never shorter than requested by more than one tick.
 */
DrvStatus iwdg_calc(uint32_t timeout_ms, uint32_t lsi_hz, IwdgSetting *setting);

/**
 * @brief Start the watchdog.
 * @param timeout_ms Timeout, 1 to 32768 ms.
 * @param freeze_in_debug Stop the watchdog counter while the core is
 *        halted by a debugger (recommended during development).
 * @return DRV_OK, DRV_ERR_PARAM, or DRV_ERR_TIMEOUT if the register update
 *         is not acknowledged.
 * @note May be called again to change the timeout.
 */
DrvStatus iwdg_start(uint32_t timeout_ms, bool freeze_in_debug);

/** @brief Reload the watchdog counter ("kick the dog"). */
void iwdg_refresh(void);

/**
 * @brief Return true if the last reset was caused by the IWDG.
 * @note Read it early in main(); iwdg_clear_reset_flags() clears it (and
 *       every other RCC reset flag).
 */
bool iwdg_caused_reset(void);

/** @brief Clear all RCC reset-cause flags (RCC_CSR.RMVF). */
void iwdg_clear_reset_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_DRIVERS_IWDG_H */
