#include "clock.h"

#include "drivers/internal/drv_internal.h"
#include "system/time/time.h"


#ifndef HSE_VALUE
#define HSE_VALUE 24000000UL /* 24 MHz crystal X3 on the Nucleo-G474RE */
#endif

/* Oscillator start-up and PLL lock take well under 1 ms; the clock switch
 * completes within a few cycles of the new source. */
#define OSC_TIMEOUT_US     5000U
#define SWITCH_TIMEOUT_US  5000U

/* Above this SYSCLK the AHB prescaler must be /2 around the switch. */
#define HCLK_STEP_THRESHOLD_HZ 80000000UL

#define PLL_VCO_IN_MIN_HZ   2660000UL
#define PLL_VCO_IN_MAX_HZ  16000000UL
#define PLL_VCO_OUT_MIN_HZ 96000000UL
#define PLL_VCO_OUT_MAX_HZ 344000000UL
#define SYSCLK_MAX_HZ     170000000UL


typedef struct {
    uint32_t sysclk_hz;
    bool     use_pll;
    uint32_t pll_m;
    uint32_t pll_n;
    uint32_t pll_r;
    bool     boost;
} PresetInfo;

static const PresetInfo PRESETS[] = {
    [CLOCK_PRESET_HSI_16MHZ]  = {  16000000UL, false, 0U,  0U, 0U, false },
    [CLOCK_PRESET_PLL_80MHZ]  = {  80000000UL, true,  2U, 20U, 2U, false },
    [CLOCK_PRESET_PLL_170MHZ] = { 170000000UL, true,  4U, 85U, 2U, true  },
};


/* --- Pure helpers ----------------------------------------------------------- */

int32_t clock_flash_wait_states(const uint32_t hclk_hz,
                                const PowerVoltageRange range,
                                const bool boost)
{
    /* Upper HCLK bound for 0, 1, 2, 3 and 4 wait states (RM0440 Table 9). */
    static const uint32_t boost_max[]  = { 34000000UL, 68000000UL, 102000000UL, 136000000UL, 170000000UL };
    static const uint32_t normal_max[] = { 30000000UL, 60000000UL,  90000000UL, 120000000UL, 150000000UL };
    static const uint32_t range2_max[] = { 12000000UL, 24000000UL,  26000000UL };

    const uint32_t *table;
    int32_t count;

    if (range == POWER_RANGE_2) {
        table = range2_max;
        count = 3;
    } else if (range == POWER_RANGE_1) {
        table = boost ? boost_max : normal_max;
        count = 5;
    } else {
        return -1;
    }

    for (int32_t ws = 0; ws < count; ++ws) {
        if (hclk_hz <= table[ws]) {
            return ws;
        }
    }
    return -1;
}

DrvStatus clock_pll_check(const uint32_t input_hz, const uint32_t m,
                          const uint32_t n, const uint32_t r,
                          uint32_t *output_hz)
{
    if ((m < 1U) || (m > 16U) || (n < 8U) || (n > 127U) ||
        (r < 2U) || (r > 8U) || ((r & 1U) != 0U)) {
        return DRV_ERR_PARAM;
    }

    const uint32_t vco_in = input_hz / m;
    const uint64_t vco_out = (uint64_t)vco_in * n;
    const uint64_t out = vco_out / r;

    if ((vco_in < PLL_VCO_IN_MIN_HZ) || (vco_in > PLL_VCO_IN_MAX_HZ) ||
        (vco_out < PLL_VCO_OUT_MIN_HZ) || (vco_out > PLL_VCO_OUT_MAX_HZ) ||
        (out > SYSCLK_MAX_HZ)) {
        return DRV_ERR_PARAM;
    }

    if (output_hz != NULL) {
        *output_hz = (uint32_t)out;
    }
    return DRV_OK;
}

static uint32_t ahb_divider(const uint32_t hpre)
{
    static const uint16_t div[8] = { 2U, 4U, 8U, 16U, 64U, 128U, 256U, 512U };
    return (hpre < 8U) ? 1U : div[hpre - 8U];
}

static uint32_t apb_divider(const uint32_t ppre)
{
    return (ppre < 4U) ? 1U : (1UL << (ppre - 3U));
}


/* --- Frequency queries ------------------------------------------------------ */

uint32_t clock_get_sysclk_hz(void)
{
    const uint32_t sws = RCC->CFGR & RCC_CFGR_SWS;

    if (sws == RCC_CFGR_SWS_HSI) {
        return CLOCK_HSI_HZ;
    }
    if (sws == RCC_CFGR_SWS_HSE) {
        return HSE_VALUE;
    }
    if (sws == RCC_CFGR_SWS_PLL) {
        const uint32_t cfg = RCC->PLLCFGR;
        const uint32_t src = cfg & RCC_PLLCFGR_PLLSRC;
        const uint32_t in  = (src == RCC_PLLCFGR_PLLSRC_HSE) ? HSE_VALUE : CLOCK_HSI_HZ;
        const uint32_t m   = ((cfg & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1U;
        const uint32_t n   = (cfg & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
        const uint32_t r   = (((cfg & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1U) * 2U;
        return (uint32_t)(((uint64_t)(in / m) * n) / r);
    }
    return 0U; /* not reachable: SWS has only the three encodings above */
}

uint32_t clock_get_hclk_hz(void)
{
    const uint32_t hpre = (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
    return clock_get_sysclk_hz() / ahb_divider(hpre);
}

uint32_t clock_get_pclk1_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return clock_get_hclk_hz() / apb_divider(ppre);
}

uint32_t clock_get_pclk2_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return clock_get_hclk_hz() / apb_divider(ppre);
}

uint32_t clock_get_apb1_timer_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return clock_get_pclk1_hz() * ((apb_divider(ppre) == 1U) ? 1U : 2U);
}

uint32_t clock_get_apb2_timer_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return clock_get_pclk2_hz() * ((apb_divider(ppre) == 1U) ? 1U : 2U);
}


/* --- Hardware sequencing ---------------------------------------------------- */

static void set_ahb_prescaler(const uint32_t hpre_bits)
{
    drv_reg_modify(&RCC->CFGR, RCC_CFGR_HPRE, hpre_bits);
}

static DrvStatus switch_sysclk(const uint32_t sw, const uint32_t sws)
{
    drv_reg_modify(&RCC->CFGR, RCC_CFGR_SW, sw);
    return drv_wait_bits(&RCC->CFGR, RCC_CFGR_SWS, sws, SWITCH_TIMEOUT_US);
}

static DrvStatus set_flash_wait_states(const uint32_t ws)
{
    uint32_t set = ws << FLASH_ACR_LATENCY_Pos;

    /* Instruction/data caches are on after reset; keep them on and use
     * prefetch whenever there are wait states to hide. */
    set |= FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    if (ws > 0U) {
        set |= FLASH_ACR_PRFTEN;
    }

    drv_reg_modify(&FLASH->ACR, FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN, set);

    /* The new latency must be read back before the frequency changes. */
    return drv_wait_bits(&FLASH->ACR, FLASH_ACR_LATENCY,
                         ws << FLASH_ACR_LATENCY_Pos, OSC_TIMEOUT_US);
}

/**
 * @brief Run from HSI16 with the PLL off and all prescalers at /1.
 * @note Leaves Flash latency unchanged (higher latency is always safe).
 */
static DrvStatus fall_back_to_hsi(void)
{
    DrvStatus status;

    drv_reg_modify(&RCC->CR, 0U, RCC_CR_HSION);
    status = drv_wait_set(&RCC->CR, RCC_CR_HSIRDY, OSC_TIMEOUT_US);
    if (status != DRV_OK) {
        return status;
    }

    status = switch_sysclk(RCC_CFGR_SW_HSI, RCC_CFGR_SWS_HSI);
    if (status != DRV_OK) {
        return status;
    }

    drv_reg_modify(&RCC->CFGR, RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2, 0U);

    drv_reg_modify(&RCC->CR, RCC_CR_PLLON, 0U);
    return drv_wait_clear(&RCC->CR, RCC_CR_PLLRDY, OSC_TIMEOUT_US);
}

/** @brief Busy-wait at least 1 µs at any HCLK up to 170 MHz. */
static void wait_1us(void)
{
    time_delay_us(2U);
}

static void finish_clock_change(void)
{
    SystemCoreClock = clock_get_hclk_hz();
    time_clock_changed();
}

DrvStatus clock_set_preset(const ClockPreset preset)
{
    if ((uint32_t)preset >= (sizeof PRESETS / sizeof PRESETS[0])) {
        return DRV_ERR_PARAM;
    }
    const PresetInfo *p = &PRESETS[preset];

    /* All presets run in Range 1, which allows every frequency we use. */
    DrvStatus status = power_set_voltage_range(POWER_RANGE_1);
    if (status != DRV_OK) {
        return status;
    }

    /* 1. Move to a known-safe state: HSI16, PLL off, prescalers /1.
     *    Lowering the frequency first means the current (possibly high)
     *    Flash latency stays valid throughout. */
    status = fall_back_to_hsi();
    if (status != DRV_OK) {
        finish_clock_change();
        return status;
    }
    /* Keep busy-wait delays and timeouts correct while at 16 MHz. SysTick
     * is reprogrammed once, at the end. */
    SystemCoreClock = clock_get_hclk_hz();

    /* 2. Configure boost mode and wait states for the target before
     *    raising the frequency (both are safe at 16 MHz). */
    const int32_t ws = clock_flash_wait_states(p->sysclk_hz, POWER_RANGE_1, p->boost);
    if (ws < 0) {
        return DRV_ERR_PARAM; /* inconsistent preset table */
    }

    const bool step_hclk = p->sysclk_hz > HCLK_STEP_THRESHOLD_HZ;
    if (step_hclk) {
        set_ahb_prescaler(RCC_CFGR_HPRE_DIV2);
    }

    power_set_boost(p->boost);

    status = set_flash_wait_states((uint32_t)ws);
    if (status != DRV_OK) {
        set_ahb_prescaler(RCC_CFGR_HPRE_DIV1);
        finish_clock_change();
        return status;
    }

    if (!p->use_pll) {
        finish_clock_change();
        return DRV_OK;
    }

    /* 3. Configure and lock the PLL (only PLLR output used). */
    if (clock_pll_check(CLOCK_HSI_HZ, p->pll_m, p->pll_n, p->pll_r, NULL) != DRV_OK) {
        set_ahb_prescaler(RCC_CFGR_HPRE_DIV1);
        finish_clock_change();
        return DRV_ERR_PARAM;
    }

    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSI |
                   ((p->pll_m - 1U) << RCC_PLLCFGR_PLLM_Pos) |
                   (p->pll_n << RCC_PLLCFGR_PLLN_Pos) |
                   (((p->pll_r / 2U) - 1U) << RCC_PLLCFGR_PLLR_Pos) |
                   RCC_PLLCFGR_PLLREN;

    drv_reg_modify(&RCC->CR, 0U, RCC_CR_PLLON);
    status = drv_wait_set(&RCC->CR, RCC_CR_PLLRDY, OSC_TIMEOUT_US);

    /* 4. Switch SYSCLK to the PLL. */
    if (status == DRV_OK) {
        status = switch_sysclk(RCC_CFGR_SW_PLL, RCC_CFGR_SWS_PLL);
    }

    if (status != DRV_OK) {
        /* Leave the device in a defined state: HSI16 with the (safe) higher
         * latency already programmed. */
        (void)fall_back_to_hsi();
        finish_clock_change();
        return status;
    }

    /* 5. After at least 1 µs at HCLK = SYSCLK / 2, restore HCLK = SYSCLK. */
    if (step_hclk) {
        SystemCoreClock = clock_get_hclk_hz();
        wait_1us();
        set_ahb_prescaler(RCC_CFGR_HPRE_DIV1);
    }

    finish_clock_change();
    return DRV_OK;
}
