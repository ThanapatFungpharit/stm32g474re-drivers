#ifndef STM32G474RE_DRIVERS_INTERNAL_H
#define STM32G474RE_DRIVERS_INTERNAL_H

/**
 * @file drv_internal.h
 * @brief Helpers shared by the driver implementations.
 *
 * This header is private to the library. Applications should not include it;
 * its contents may change without notice.
 */

#include "stm32g474xx.h"
#include "drivers/common.h"

/* --- Critical sections ---------------------------------------------------- */

/** Mask interrupts and return the previous PRIMASK value. */
static inline uint32_t drv_critical_enter(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/** Restore the PRIMASK value returned by drv_critical_enter(). */
static inline void drv_critical_exit(const uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

/**
 * @brief Atomically clear and set bits in a register.
 *
 * The read-modify-write runs with interrupts masked so that an interrupt
 * handler modifying other fields of the same register cannot lose its
 * update.
 */
static inline void drv_reg_modify(volatile uint32_t *reg,
                                  const uint32_t clear_mask,
                                  const uint32_t set_mask)
{
    const uint32_t primask = drv_critical_enter();
    *reg = (*reg & ~clear_mask) | set_mask;
    drv_critical_exit(primask);
}

/* --- RCC clock gating ------------------------------------------------------ */

/** RCC enable/reset register pair that gates a peripheral. */
typedef enum {
    DRV_BUS_AHB1,
    DRV_BUS_AHB2,
    DRV_BUS_APB1_1,
    DRV_BUS_APB1_2,
    DRV_BUS_APB2,
} DrvBus;

/** Location of a peripheral's clock-enable bit (and its matching reset bit). */
typedef struct {
    DrvBus   bus;
    uint32_t mask;
} DrvClockGate;

/**
 * @brief Enable or disable a peripheral bus clock.
 * @note The RCC register is read back after the write, which provides the
 *       delay the reference manual requires before the peripheral's
 *       registers may be accessed.
 */
void drv_clock_gate_set(DrvClockGate gate, bool enable);

/** @brief Return true when the peripheral bus clock is enabled. */
bool drv_clock_gate_is_enabled(DrvClockGate gate);

/**
 * @brief Pulse the peripheral's RCC reset bit, returning every register of
 *        the peripheral to its reset value.
 * @note On the STM32G4 the reset bits sit at the same positions as the
 *       enable bits in the matching xxxRSTR register.
 */
void drv_clock_gate_reset(DrvClockGate gate);

/* --- Bounded waiting -------------------------------------------------------- */

/**
 * @brief Wait until (*reg & mask) == expected.
 * @param reg Register to poll.
 * @param mask Bits to compare.
 * @param expected Value the masked bits must reach.
 * @param timeout_us Time limit in microseconds, or DRV_WAIT_FOREVER.
 * @return DRV_OK when the condition is met, DRV_ERR_TIMEOUT otherwise.
 * @note Works before time_init() because it uses the DWT cycle counter.
 */
DrvStatus drv_wait_bits(volatile const uint32_t *reg, uint32_t mask,
                        uint32_t expected, uint32_t timeout_us);

/** @brief Wait until all bits in @p mask are set. */
static inline DrvStatus drv_wait_set(volatile const uint32_t *reg,
                                     const uint32_t mask,
                                     const uint32_t timeout_us)
{
    return drv_wait_bits(reg, mask, mask, timeout_us);
}

/** @brief Wait until all bits in @p mask are clear. */
static inline DrvStatus drv_wait_clear(volatile const uint32_t *reg,
                                       const uint32_t mask,
                                       const uint32_t timeout_us)
{
    return drv_wait_bits(reg, mask, 0U, timeout_us);
}

/* --- NVIC ------------------------------------------------------------------- */

/** Highest numerically valid NVIC priority on the STM32G4 (4 priority bits). */
#define DRV_NVIC_PRIORITY_MAX ((1UL << __NVIC_PRIO_BITS) - 1UL)

/** @brief Set the priority of, clear any pending request for, and enable an IRQ. */
static inline void drv_nvic_enable(const IRQn_Type irqn, const uint32_t priority)
{
    NVIC_SetPriority(irqn, priority);
    NVIC_ClearPendingIRQ(irqn);
    NVIC_EnableIRQ(irqn);
}

#endif /* STM32G474RE_DRIVERS_INTERNAL_H */
