/* Host tests: independent watchdog timing and start-up sequence. */
#include <unity.h>

#include "drivers/iwdg/iwdg.h"

void setUp(void) { mock_reset(); }
void tearDown(void) {}

static void test_calc_picks_smallest_prescaler(void)
{
    IwdgSetting s;

    TEST_ASSERT_EQUAL(DRV_OK, iwdg_calc(1000U, 32000U, &s));
    TEST_ASSERT_EQUAL_UINT32(1U, s.prescaler_code); /* /8: 4000 ticks */
    TEST_ASSERT_EQUAL_UINT32(3999U, s.reload);
    TEST_ASSERT_EQUAL_UINT32(1000U, s.timeout_ms);

    TEST_ASSERT_EQUAL(DRV_OK, iwdg_calc(1U, 32000U, &s));
    TEST_ASSERT_EQUAL_UINT32(0U, s.prescaler_code);
    TEST_ASSERT_EQUAL_UINT32(7U, s.reload);

    TEST_ASSERT_EQUAL(DRV_OK, iwdg_calc(32768U, 32000U, &s));
    TEST_ASSERT_EQUAL_UINT32(6U, s.prescaler_code); /* /256 */
    TEST_ASSERT_EQUAL_UINT32(4095U, s.reload);
}

static void test_calc_never_shorter_than_requested(void)
{
    IwdgSetting s;
    for (uint32_t ms = 1U; ms <= 32768U; ms = (ms * 3U) + 1U) {
        TEST_ASSERT_EQUAL(DRV_OK, iwdg_calc(ms, 32000U, &s));
        const uint64_t ticks = (uint64_t)s.reload + 1U;
        const uint64_t actual_us = (ticks * (4ULL << s.prescaler_code) * 1000000ULL) / 32000ULL;
        TEST_ASSERT_TRUE(actual_us >= (uint64_t)ms * 1000ULL);
    }
}

static void test_calc_rejects_out_of_range(void)
{
    IwdgSetting s;
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, iwdg_calc(0U, 32000U, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, iwdg_calc(32769U, 32000U, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, iwdg_calc(10U, 0U, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, iwdg_calc(10U, 32000U, NULL));
}

static void test_start_sequence(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, iwdg_start(1000U, true));
    TEST_ASSERT_EQUAL_UINT32(1U, mock_IWDG.PR);
    TEST_ASSERT_EQUAL_UINT32(3999U, mock_IWDG.RLR);
    TEST_ASSERT_EQUAL_UINT32(0xFFFU, mock_IWDG.WINR);
    TEST_ASSERT_EQUAL_HEX32(0xAAAAU, mock_IWDG.KR); /* last key: refresh */
    TEST_ASSERT_BITS(DBGMCU_APB1FZR1_DBG_IWDG_STOP, DBGMCU_APB1FZR1_DBG_IWDG_STOP,
                     mock_DBGMCU.APB1FZR1);
}

static void test_start_times_out_if_update_never_completes(void)
{
    mock_IWDG.SR = IWDG_SR_RVU;
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, iwdg_start(100U, false));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_DBGMCU.APB1FZR1);
}

static void test_reset_flags(void)
{
    TEST_ASSERT_FALSE(iwdg_caused_reset());
    mock_RCC.CSR = RCC_CSR_IWDGRSTF;
    TEST_ASSERT_TRUE(iwdg_caused_reset());
    iwdg_clear_reset_flags();
    TEST_ASSERT_BITS(RCC_CSR_RMVF, RCC_CSR_RMVF, mock_RCC.CSR);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_calc_picks_smallest_prescaler);
    RUN_TEST(test_calc_never_shorter_than_requested);
    RUN_TEST(test_calc_rejects_out_of_range);
    RUN_TEST(test_start_sequence);
    RUN_TEST(test_start_times_out_if_update_never_completes);
    RUN_TEST(test_reset_flags);
    return UNITY_END();
}
