/* Host tests: clock tree sequencing, frequency queries and the PWR driver. */
#include <unity.h>

#include "drivers/power/power.h"
#include "system/clock/clock.h"
#include "system/time/time.h"

static bool pll_locks;
static uint32_t max_hpre_seen_while_pll;

/* Emulates RCC: oscillators become ready when enabled, SWS follows SW. */
static void rcc_hardware(void)
{
    if ((mock_RCC.CR & RCC_CR_HSION) != 0U) {
        mock_RCC.CR |= RCC_CR_HSIRDY;
    }
    if (((mock_RCC.CR & RCC_CR_PLLON) != 0U) && pll_locks) {
        mock_RCC.CR |= RCC_CR_PLLRDY;
    } else {
        mock_RCC.CR &= ~RCC_CR_PLLRDY;
    }
    const uint32_t sw = mock_RCC.CFGR & RCC_CFGR_SW;
    mock_RCC.CFGR = (mock_RCC.CFGR & ~RCC_CFGR_SWS) | (sw << RCC_CFGR_SWS_Pos);

    if (sw == RCC_CFGR_SW_PLL) {
        const uint32_t hpre = (mock_RCC.CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
        if (hpre > max_hpre_seen_while_pll) {
            max_hpre_seen_while_pll = hpre;
        }
    }
}

void setUp(void)
{
    mock_reset();
    pll_locks = true;
    max_hpre_seen_while_pll = 0U;
    mock_hw_hook = rcc_hardware;
}

void tearDown(void) {}

static void test_flash_wait_state_table_boundaries(void)
{
    TEST_ASSERT_EQUAL_INT32(0, clock_flash_wait_states(34000000U, POWER_RANGE_1, true));
    TEST_ASSERT_EQUAL_INT32(1, clock_flash_wait_states(34000001U, POWER_RANGE_1, true));
    TEST_ASSERT_EQUAL_INT32(4, clock_flash_wait_states(170000000U, POWER_RANGE_1, true));
    TEST_ASSERT_EQUAL_INT32(-1, clock_flash_wait_states(170000001U, POWER_RANGE_1, true));

    TEST_ASSERT_EQUAL_INT32(0, clock_flash_wait_states(30000000U, POWER_RANGE_1, false));
    TEST_ASSERT_EQUAL_INT32(2, clock_flash_wait_states(80000000U, POWER_RANGE_1, false));
    TEST_ASSERT_EQUAL_INT32(4, clock_flash_wait_states(150000000U, POWER_RANGE_1, false));
    TEST_ASSERT_EQUAL_INT32(-1, clock_flash_wait_states(150000001U, POWER_RANGE_1, false));

    TEST_ASSERT_EQUAL_INT32(0, clock_flash_wait_states(12000000U, POWER_RANGE_2, false));
    TEST_ASSERT_EQUAL_INT32(2, clock_flash_wait_states(26000000U, POWER_RANGE_2, true));
    TEST_ASSERT_EQUAL_INT32(-1, clock_flash_wait_states(26000001U, POWER_RANGE_2, false));
}

static void test_pll_check_limits(void)
{
    uint32_t out = 0U;

    TEST_ASSERT_EQUAL(DRV_OK, clock_pll_check(16000000U, 4U, 85U, 2U, &out));
    TEST_ASSERT_EQUAL_UINT32(170000000U, out);
    TEST_ASSERT_EQUAL(DRV_OK, clock_pll_check(16000000U, 2U, 20U, 2U, &out));
    TEST_ASSERT_EQUAL_UINT32(80000000U, out);

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 0U, 20U, 2U, NULL));  /* M */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 17U, 20U, 2U, NULL)); /* M */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 1U, 7U, 2U, NULL));   /* N */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 1U, 20U, 3U, NULL));  /* R odd */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 8U, 85U, 2U, NULL));  /* VCO in 2 MHz */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 1U, 30U, 2U, NULL));  /* VCO out 480 MHz */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_pll_check(16000000U, 2U, 43U, 2U, NULL));  /* out 172 MHz */
}

static void test_preset_170mhz_programs_boost_latency_and_pll(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_PLL_170MHZ));

    TEST_ASSERT_EQUAL_HEX32(RCC_CFGR_SWS_PLL, mock_RCC.CFGR & RCC_CFGR_SWS);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.CFGR & (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2));
    TEST_ASSERT_EQUAL_HEX32(RCC_PLLCFGR_PLLSRC_HSI | (3UL << RCC_PLLCFGR_PLLM_Pos) |
                            (85UL << RCC_PLLCFGR_PLLN_Pos) | RCC_PLLCFGR_PLLREN,
                            mock_RCC.PLLCFGR);
    TEST_ASSERT_EQUAL_HEX32(FLASH_ACR_LATENCY_4WS, mock_FLASH.ACR & FLASH_ACR_LATENCY);
    TEST_ASSERT_BITS(FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN,
                     FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN, mock_FLASH.ACR);
    TEST_ASSERT_TRUE(power_is_boost_enabled());
    TEST_ASSERT_EQUAL(POWER_RANGE_1, power_get_voltage_range());

    /* RM0440: HCLK must run at SYSCLK/2 when first switching above 80 MHz. */
    TEST_ASSERT_EQUAL_UINT32(8U, max_hpre_seen_while_pll); /* HPRE code 8 = /2 */

    TEST_ASSERT_EQUAL_UINT32(170000000U, SystemCoreClock);
    TEST_ASSERT_EQUAL_UINT32(170000000U, clock_get_sysclk_hz());
    TEST_ASSERT_EQUAL_UINT32(170000000U, clock_get_pclk1_hz());
    TEST_ASSERT_EQUAL_UINT32(170000000U, clock_get_apb2_timer_hz());
}

static void test_preset_80mhz_uses_normal_mode_and_2_wait_states(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_PLL_170MHZ));
    max_hpre_seen_while_pll = 0U;
    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_PLL_80MHZ));

    TEST_ASSERT_EQUAL_HEX32(FLASH_ACR_LATENCY_2WS, mock_FLASH.ACR & FLASH_ACR_LATENCY);
    TEST_ASSERT_FALSE(power_is_boost_enabled());
    TEST_ASSERT_EQUAL_HEX32((1UL << RCC_PLLCFGR_PLLM_Pos) | (20UL << RCC_PLLCFGR_PLLN_Pos),
                            mock_RCC.PLLCFGR & (RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLR));
    TEST_ASSERT_EQUAL_UINT32(80000000U, SystemCoreClock);
    /* 80 MHz is not above the threshold: no HCLK/2 transition step. */
    TEST_ASSERT_EQUAL_UINT32(0U, max_hpre_seen_while_pll);
}

static void test_preset_16mhz_turns_pll_off(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_PLL_80MHZ));
    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_HSI_16MHZ));

    TEST_ASSERT_EQUAL_HEX32(RCC_CFGR_SWS_HSI, mock_RCC.CFGR & RCC_CFGR_SWS);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.CR & RCC_CR_PLLON);
    TEST_ASSERT_EQUAL_HEX32(FLASH_ACR_LATENCY_0WS, mock_FLASH.ACR & FLASH_ACR_LATENCY);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_FLASH.ACR & FLASH_ACR_PRFTEN);
    TEST_ASSERT_EQUAL_UINT32(16000000U, SystemCoreClock);
}

static void test_pll_lock_failure_falls_back_to_hsi(void)
{
    pll_locks = false;

    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, clock_set_preset(CLOCK_PRESET_PLL_170MHZ));

    TEST_ASSERT_EQUAL_HEX32(RCC_CFGR_SWS_HSI, mock_RCC.CFGR & RCC_CFGR_SWS);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.CFGR & RCC_CFGR_HPRE);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.CR & RCC_CR_PLLON);
    TEST_ASSERT_EQUAL_UINT32(16000000U, SystemCoreClock);
    /* The higher latency stays programmed, which is safe at 16 MHz. */
    TEST_ASSERT_EQUAL_HEX32(FLASH_ACR_LATENCY_4WS, mock_FLASH.ACR & FLASH_ACR_LATENCY);
}

static void test_invalid_preset(void)
{
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, clock_set_preset((ClockPreset)3));
}

static void test_frequency_queries_with_prescalers(void)
{
    mock_RCC.CFGR = RCC_CFGR_SWS_HSI | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV1;
    TEST_ASSERT_EQUAL_UINT32(16000000U, clock_get_sysclk_hz());
    TEST_ASSERT_EQUAL_UINT32(8000000U, clock_get_hclk_hz());
    TEST_ASSERT_EQUAL_UINT32(2000000U, clock_get_pclk1_hz());
    TEST_ASSERT_EQUAL_UINT32(4000000U, clock_get_apb1_timer_hz()); /* x2 when divided */
    TEST_ASSERT_EQUAL_UINT32(8000000U, clock_get_pclk2_hz());
    TEST_ASSERT_EQUAL_UINT32(8000000U, clock_get_apb2_timer_hz());

    mock_RCC.CFGR = RCC_CFGR_SWS_HSI | RCC_CFGR_HPRE_DIV512;
    TEST_ASSERT_EQUAL_UINT32(31250U, clock_get_hclk_hz());
}

static void test_clock_change_keeps_time_monotonic(void)
{
    time_init();
    for (int i = 0; i < 5; ++i) {
        SysTick_Handler();
    }
    mock_systick.VAL = mock_systick.LOAD / 2U;
    const uint64_t before = time_micros64();

    TEST_ASSERT_EQUAL(DRV_OK, clock_set_preset(CLOCK_PRESET_PLL_170MHZ));
    mock_systick.VAL = mock_systick.LOAD; /* start of a fresh period */

    TEST_ASSERT_TRUE(time_micros64() >= before);
    TEST_ASSERT_EQUAL_UINT32(170000U - 1U, mock_systick.LOAD);
}

static void test_power_voltage_range_and_ucpd(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, power_set_voltage_range(POWER_RANGE_2));
    TEST_ASSERT_EQUAL_HEX32(PWR_CR1_VOS_1, mock_PWR.CR1 & PWR_CR1_VOS);
    TEST_ASSERT_EQUAL(POWER_RANGE_2, power_get_voltage_range());
    TEST_ASSERT_BITS(RCC_APB1ENR1_PWREN, RCC_APB1ENR1_PWREN, mock_RCC.APB1ENR1);

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, power_set_voltage_range((PowerVoltageRange)3));
    TEST_ASSERT_EQUAL_HEX32(PWR_CR1_VOS_1, mock_PWR.CR1 & PWR_CR1_VOS); /* unchanged */

    mock_PWR.SR2 = PWR_SR2_VOSF; /* regulator never settles */
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, power_set_voltage_range(POWER_RANGE_1));

    power_disable_ucpd_dead_battery();
    TEST_ASSERT_BITS(PWR_CR3_UCPD_DBDIS, PWR_CR3_UCPD_DBDIS, mock_PWR.CR3);

    power_set_boost(true);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_PWR.CR5 & PWR_CR5_R1MODE);
    power_set_boost(false);
    TEST_ASSERT_EQUAL_HEX32(PWR_CR5_R1MODE, mock_PWR.CR5 & PWR_CR5_R1MODE);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flash_wait_state_table_boundaries);
    RUN_TEST(test_pll_check_limits);
    RUN_TEST(test_preset_170mhz_programs_boost_latency_and_pll);
    RUN_TEST(test_preset_80mhz_uses_normal_mode_and_2_wait_states);
    RUN_TEST(test_preset_16mhz_turns_pll_off);
    RUN_TEST(test_pll_lock_failure_falls_back_to_hsi);
    RUN_TEST(test_invalid_preset);
    RUN_TEST(test_frequency_queries_with_prescalers);
    RUN_TEST(test_clock_change_keeps_time_monotonic);
    RUN_TEST(test_power_voltage_range_and_ucpd);
    return UNITY_END();
}
