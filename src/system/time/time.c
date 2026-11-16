#include "time.h"

#include "stm32g474xx.h"


/* Milliseconds counted by SysTick. Written only by SysTick_Handler() and by
 * time_clock_changed() with interrupts masked. */
static volatile uint64_t ms_count;
static volatile bool     time_running;

/* Software cycle estimate used only if the core has no DWT cycle counter. */
static uint32_t soft_cycles;


/* --- Cycle counter ---------------------------------------------------------- */

/**
 * @brief Enable the DWT cycle counter.
 * @return false when the core does not implement CYCCNT.
 */
static bool dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U) {
        return false;
    }

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return true;
}

uint32_t time_cycles(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
        if (!dwt_enable()) {
            /* Every Cortex-M4 in the STM32G4 has CYCCNT, so this is a
             * safety net only. A polling loop iteration takes well over 8
             * cycles, so timeouts measured this way are longer, never
             * shorter, than requested. */
            soft_cycles += 8U;
            return soft_cycles;
        }
    }
    return DWT->CYCCNT;
}

uint64_t time_us_to_cycles(const uint64_t us, const uint32_t clock_hz)
{
    /* us < 2^32 and clock_hz < 2^32, so the product cannot overflow. */
    return ((us * clock_hz) + 999999ULL) / 1000000ULL;
}


/* --- Timeouts --------------------------------------------------------------- */

static void timeout_start_cycles(TimeTimeout *timeout, const uint64_t cycles,
                                 const bool forever)
{
    timeout->last    = time_cycles();
    timeout->elapsed = 0U;
    timeout->limit   = cycles;
    timeout->forever = forever;
}

void time_timeout_start_us(TimeTimeout *timeout, const uint32_t us)
{
    timeout_start_cycles(timeout,
                         time_us_to_cycles(us, SystemCoreClock),
                         us == UINT32_MAX);
}

void time_timeout_start_ms(TimeTimeout *timeout, const uint32_t ms)
{
    timeout_start_cycles(timeout,
                         time_us_to_cycles((uint64_t)ms * 1000ULL, SystemCoreClock),
                         ms == UINT32_MAX);
}

bool time_timeout_expired(TimeTimeout *timeout)
{
    if (timeout->forever) {
        return false;
    }

    const uint32_t now = time_cycles();
    timeout->elapsed += (uint32_t)(now - timeout->last);
    timeout->last = now;

    return timeout->elapsed >= timeout->limit;
}


/* --- SysTick time base ------------------------------------------------------ */

/**
 * @brief Program SysTick for a 1 ms period at the current SystemCoreClock.
 */
static void systick_program(void)
{
    const uint32_t reload = (SystemCoreClock + 500U) / 1000U;

    /* SysTick_Config() rejects reload values above 2^24; that would need a
     * core clock above 16.7 GHz, so failure here is impossible. */
    (void)SysTick_Config(reload);

    /* SysTick_Config() clears VAL; the counter reloads on the next clock.
     * Wait for that so time_micros64() never sees VAL == 0 with a fresh
     * millisecond count, which would look like time going backwards. */
    for (uint32_t i = 0U; (i < 16U) && (SysTick->VAL == 0U); ++i) {
    }
}

void time_init(void)
{
    (void)time_cycles(); /* enable the cycle counter */

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    ms_count = 0U;
    systick_program();
    time_running = true;

    if (primask == 0U) {
        __enable_irq();
    }
}

bool time_is_initialized(void)
{
    return time_running;
}

void time_clock_changed(void)
{
    if (!time_running) {
        return;
    }

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /* A tick that already happened but has not been handled yet. */
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0U) {
        SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
        ms_count = ms_count + 1U;
    }

    /* The partial millisecond in progress is rounded up so that time never
     * runs backwards. */
    ms_count = ms_count + 1U;
    systick_program();

    if (primask == 0U) {
        __enable_irq();
    }
}

void SysTick_Handler(void)
{
    ms_count = ms_count + 1U;
}

/**
 * @brief Take a consistent snapshot of the millisecond count and SysTick.
 * @param ms Millisecond count.
 * @param elapsed SysTick cycles elapsed in the current millisecond.
 * @param period SysTick cycles per millisecond.
 */
static void snapshot(uint64_t *ms, uint32_t *elapsed, uint32_t *period)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint64_t count = ms_count;
    const uint32_t load = SysTick->LOAD;
    uint32_t val = SysTick->VAL;

    /* If the counter wrapped while interrupts are masked, the tick is still
     * pending: count it and re-read VAL, which is now past the wrap. */
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) != 0U) {
        val = SysTick->VAL;
        ++count;
    }

    if (primask == 0U) {
        __enable_irq();
    }

    if (val > load) {
        val = load;
    }

    *ms = count;
    *elapsed = load - val;
    *period = load + 1U;
}

uint64_t time_millis64(void)
{
    uint64_t ms;
    uint32_t elapsed;
    uint32_t period;

    if (!time_running) {
        return 0U;
    }
    snapshot(&ms, &elapsed, &period);
    return ms;
}

uint64_t time_micros64(void)
{
    uint64_t ms;
    uint32_t elapsed;
    uint32_t period;

    if (!time_running) {
        return 0U;
    }
    snapshot(&ms, &elapsed, &period);
    return (ms * 1000ULL) + (((uint64_t)elapsed * 1000ULL) / period);
}

uint32_t time_millis(void) { return (uint32_t)time_millis64(); }
uint32_t time_micros(void) { return (uint32_t)time_micros64(); }


/* --- Delays ----------------------------------------------------------------- */

void time_delay_us(const uint32_t us)
{
    if (us == 0U) {
        return;
    }

    TimeTimeout timeout;
    time_timeout_start_us(&timeout, us);
    while (!time_timeout_expired(&timeout)) {
    }
}

void time_delay_ms(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    /* Sleeping is only safe if the SysTick interrupt will wake us up and
     * advance the millisecond count. */
    const uint32_t systick_on = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    const bool can_sleep = time_running &&
                           ((SysTick->CTRL & systick_on) == systick_on) &&
                           (__get_PRIMASK() == 0U) &&
                           (__get_IPSR() == 0U);

    if (!can_sleep) {
        /* SysTick cannot advance here; count cycles instead. Split the wait
         * so the microsecond argument cannot overflow. */
        while (ms > 0U) {
            const uint32_t chunk = (ms > 1000000U) ? 1000000U : ms;
            time_delay_us(chunk * 1000U);
            ms -= chunk;
        }
        return;
    }

    const uint64_t target = time_micros64() + ((uint64_t)ms * 1000ULL);
    while (time_micros64() < target) {
        __WFI();
    }
}
