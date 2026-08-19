#include "time.h"


static volatile uint32_t cyccnt_last;
static volatile uint64_t cycles64_base;
static volatile uint64_t ms_count64;
static uint8_t  dwt_available;

static uint32_t sys_clock_hz;
static uint32_t cycles_per_us_q32;


/**
 * @brief Enable and reset the DWT cycle counter when available.
 * @note Modifies CoreDebug and DWT registers.
 */
static void dwtInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    if (DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) {
        return;
    }

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief Update timing values derived from the core clock.
 * @param core_clock_hz Core clock frequency in hertz.
 */
static void recompute_clock_derived(uint32_t core_clock_hz)
{
    sys_clock_hz = core_clock_hz;
    if (core_clock_hz == 0U) {
        cycles_per_us_q32 = 0U;
        return;
    }

    cycles_per_us_q32 = (uint32_t)(((uint64_t)core_clock_hz << 32) / 1000000ULL);
}

/**
 * @brief Convert a cycle count to microseconds.
 * @param cycles Cycle count.
 * @param clock_hz Clock frequency in hertz.
 * @return Equivalent microseconds, or zero when clock_hz is zero.
 */
static uint64_t cycles_to_us_exact(uint64_t cycles, uint32_t clock_hz)
{
    if (clock_hz == 0U)
        return 0U;

    uint64_t whole_s    = cycles / clock_hz;
    uint64_t rem_cycles = cycles % clock_hz;
    return whole_s * 1000000ULL + (rem_cycles * 1000000ULL) / clock_hz;
}

/**
 * @brief Compute a SysTick reload value for a millisecond period.
 * @param core_clock_hz Core clock frequency in hertz.
 * @return Rounded number of cycles per millisecond.
 */
static uint32_t compute_reload(uint32_t core_clock_hz)
{
    return (core_clock_hz / 1000U) + (((core_clock_hz % 1000U) >= 500U) ? 1U : 0U);
}

/**
 * @brief Initialize SysTick and timing state.
 * @note Updates SystemCoreClock, configures DWT, and programs SysTick.
 */
void systick_init(void)
{
    SystemCoreClockUpdate();
    dwtInit();

    recompute_clock_derived(SystemCoreClock);

    cyccnt_last   = dwt_available ? DWT->CYCCNT : 0U;
    cycles64_base = 0U;
    ms_count64    = 0U;

    SysTick_Config(compute_reload(SystemCoreClock));
}

/**
 * @brief Reconfigure timing after a core-clock change.
 * @note Temporarily disables interrupts while updating timing state and SysTick.
 */
void systick_clock_updated(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (dwt_available) {
        uint32_t now = DWT->CYCCNT;
        cycles64_base += (uint32_t)(now - cyccnt_last);
        cyccnt_last = now;
    }

    SystemCoreClockUpdate();
    recompute_clock_derived(SystemCoreClock);
    SysTick_Config(compute_reload(SystemCoreClock));

    if (!primask) {
        __enable_irq();
    }
}

/**
 * @brief Handle a SysTick interrupt.
 * @note Updates the cycle or millisecond time base.
 */
void SysTick_Handler(void)
{
    if (dwt_available) {
        uint32_t now = DWT->CYCCNT;
        cycles64_base += (uint32_t)(now - cyccnt_last);
        cyccnt_last = now;
    } else {
        ++ms_count64;
    }
}

/**
 * @brief Read the extended DWT cycle count.
 * @return Current 64-bit cycle count.
 * @note Temporarily disables interrupts while reading shared state.
 */
static uint64_t read_cycles64(void)
{
    uint64_t base;
    uint32_t last;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    base = cycles64_base;
    last = cyccnt_last;
    if (!primask) {
        __enable_irq();
    }

    uint32_t now = DWT->CYCCNT;
    return base + (uint32_t)(now - last);
}

/**
 * @brief Read the fallback millisecond counter.
 * @return Current 64-bit millisecond count.
 * @note Temporarily disables interrupts while reading shared state.
 */
static uint64_t read_ms64_fallback(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint64_t ms = ms_count64;
    if (!primask) {
        __enable_irq();
    }
    return ms;
}

/**
 * @brief Check whether the DWT cycle counter is available.
 * @return Nonzero when the cycle counter is available.
 */
uint8_t systick_has_cycle_counter(void)
{
    return dwt_available;
}

/**
 * @brief Get the extended DWT cycle count.
 * @return Current cycle count, or zero when unavailable.
 */
uint64_t dwt_cycle_count64(void)
{
    return dwt_available ? read_cycles64() : 0U;
}

/**
 * @brief Get elapsed time in microseconds.
 * @return Current 64-bit microsecond count.
 */
uint64_t micros64(void)
{
    if (dwt_available) {
        return cycles_to_us_exact(read_cycles64(), sys_clock_hz);
    }
    return read_ms64_fallback() * 1000ULL;
}

/**
 * @brief Get elapsed time in milliseconds.
 * @return Current 64-bit millisecond count.
 */
uint64_t millis64(void)
{
    if (dwt_available) {
        return cycles_to_us_exact(read_cycles64(), sys_clock_hz) / 1000ULL;
    }
    return read_ms64_fallback();
}

/** @brief Get the low 32 bits of the microsecond count. @return Current microsecond count truncated to 32 bits. */
uint32_t micros(void) { return (uint32_t)micros64(); }
/** @brief Get the low 32 bits of the millisecond count. @return Current millisecond count truncated to 32 bits. */
uint32_t millis(void) { return (uint32_t)millis64(); }

/**
 * @brief Busy-wait for DWT cycle-counter progress.
 * @param cycles Number of cycles to wait.
 */
static void spin_cycles_dwt(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}

/**
 * @brief Busy-wait for SysTick cycle-counter progress.
 * @param cycles Number of cycles to wait.
 */
static void spin_cycles_systick(uint32_t cycles)
{
    uint32_t reload  = SysTick->LOAD;
    uint32_t last    = SysTick->VAL;
    uint32_t elapsed = 0U;

    while (elapsed < cycles) {
        uint32_t now = SysTick->VAL;
        uint32_t d = (now <= last) ? (last - now) : (last + (reload - now) + 1U);
        elapsed += d;
        last = now;
    }
}

/**
 * @brief Busy-wait for a number of cycles using the available counter.
 * @param cycles Number of cycles to wait.
 */
static void spin_cycles(uint32_t cycles)
{
    if (dwt_available) {
        spin_cycles_dwt(cycles);
    } else {
        spin_cycles_systick(cycles);
    }
}

/**
 * @brief Busy-wait for a number of microseconds.
 * @param us Microseconds to wait.
 */
static void spin_us(uint32_t us)
{
    while (us != 0U) {
        uint32_t chunk = (us > 1000U) ? 1000U : us;
        uint32_t cycles = (uint32_t)(((uint64_t)chunk * cycles_per_us_q32) >> 32) + 1U;
        spin_cycles(cycles);
        us -= chunk;
    }
}

/**
 * @brief Delay execution for a number of milliseconds.
 * @param ms Milliseconds to delay.
 * @note Waits for the time base and enters sleep while it advances.
 */
void delay_ms(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    uint64_t start      = micros64();
    uint64_t target      = start + (uint64_t)ms * 1000ULL; /* wraps only
        after ~584,942 years of continuous target values -- noted, not
        worth guarding in real firmware */
    uint64_t last_seen   = start;
    uint32_t stall_guard = 0U;

    for (;;) {
        uint64_t now = micros64();
        if (now >= target) {
            return;
        }
        if (now == last_seen) {
            if (++stall_guard > 4U) {
                spin_us((uint32_t)(target - now));
                return;
            }
        } else {
            stall_guard = 0U;
            last_seen = now;
        }
        __WFI();
    }
}

/**
 * @brief Delay execution for a number of microseconds.
 * @param us Microseconds to delay.
 */
void delay_us(uint32_t us)
{
    if (us == 0U) {
        return;
    }

    if (us >= 1000U) {
        uint32_t ms = us / 1000U;
        delay_ms(ms);
        us -= ms * 1000U;
        if (us == 0U) {
            return;
        }
    }

    spin_us(us);
}
