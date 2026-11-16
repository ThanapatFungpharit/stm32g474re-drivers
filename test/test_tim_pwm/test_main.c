/* Host tests: timer period calculation, time-base setup and PWM channels. */
#include <unity.h>

#include "drivers/pwm/pwm.h"
#include "drivers/tim/tim.h"

void setUp(void)
{
    mock_reset();
    SystemCoreClock = 170000000U;
    /* SYSCLK = PLL 170 MHz, all prescalers /1. */
    mock_RCC.CFGR = RCC_CFGR_SWS_PLL;
    mock_RCC.PLLCFGR = RCC_PLLCFGR_PLLSRC_HSI | (3UL << RCC_PLLCFGR_PLLM_Pos) |
                       (85UL << RCC_PLLCFGR_PLLN_Pos);
}

void tearDown(void) {}

static void test_calc_period_16_and_32_bit(void)
{
    uint32_t psc;
    uint32_t arr;

    TEST_ASSERT_EQUAL(DRV_OK, tim_calc_period(170000000U, 1000U, 0xFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL_UINT32(2U, psc);      /* 170000 ticks need PSC + 1 = 3 */
    TEST_ASSERT_EQUAL_UINT32(56666U, arr);  /* 3 * 56667 = 170001 ticks */

    TEST_ASSERT_EQUAL(DRV_OK, tim_calc_period(170000000U, 1000U, 0xFFFFFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL_UINT32(0U, psc);
    TEST_ASSERT_EQUAL_UINT32(169999U, arr);

    TEST_ASSERT_EQUAL(DRV_OK, tim_calc_period(170000000U, 1U, 0xFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL_UINT32(2593U, psc);
    TEST_ASSERT_TRUE(arr <= 0xFFFFU);
    /* Within 0.01 % of the requested period. */
    TEST_ASSERT_UINT64_WITHIN(17000U, 170000000ULL, (uint64_t)(psc + 1U) * (arr + 1U));
}

static void test_calc_period_limits(void)
{
    uint32_t psc;
    uint32_t arr;

    /* Highest frequency: two ticks per period. */
    TEST_ASSERT_EQUAL(DRV_OK, tim_calc_period(170000000U, 85000000U, 0xFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL_UINT32(0U, psc);
    TEST_ASSERT_EQUAL_UINT32(1U, arr);
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, tim_calc_period(170000000U, 200000000U, 0xFFFFU, &psc, &arr));
    /* Too slow for a tiny counter even with the largest prescaler. */
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, tim_calc_period(170000000U, 1U, 0xFFU, &psc, &arr));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_calc_period(0U, 1U, 0xFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_calc_period(1000U, 0U, 0xFFFFU, &psc, &arr));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_calc_period(1000U, 1U, 0xFFFFU, NULL, &arr));
}

static void test_instance_properties(void)
{
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFU, tim_get_max_arr(TIM2));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFU, tim_get_max_arr(TIM3));
    TEST_ASSERT_EQUAL_UINT32(0U, tim_get_channel_count(TIM6));
    TEST_ASSERT_EQUAL_UINT32(2U, tim_get_channel_count(TIM15));
    TEST_ASSERT_EQUAL_UINT32(1U, tim_get_channel_count(TIM17));
    TEST_ASSERT_TRUE(tim_has_main_output(TIM1));
    TEST_ASSERT_TRUE(tim_has_main_output(TIM16));
    TEST_ASSERT_FALSE(tim_has_main_output(TIM4));
    TEST_ASSERT_FALSE(tim_is_valid((TIM_TypeDef *)&mock_USART1));
    TEST_ASSERT_EQUAL(TIM6_DAC_IRQn, tim_update_irqn(TIM6));
    TEST_ASSERT_EQUAL(TIM1_UP_TIM16_IRQn, tim_update_irqn(TIM16));
    TEST_ASSERT_EQUAL(TIM20_UP_IRQn, tim_update_irqn(TIM20));
    TEST_ASSERT_EQUAL_UINT32(170000000U, tim_get_clock_hz(TIM2));
    TEST_ASSERT_EQUAL_UINT32(170000000U, tim_get_clock_hz(TIM1));
}

static void test_timer_clock_doubles_when_apb_divided(void)
{
    mock_RCC.CFGR |= RCC_CFGR_PPRE1_DIV2;
    TEST_ASSERT_EQUAL_UINT32(170000000U, tim_get_clock_hz(TIM3)); /* 2 x 85 MHz */
    TEST_ASSERT_EQUAL_UINT32(170000000U, tim_get_clock_hz(TIM8));
}

static void test_init_programs_time_base(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, tim_init(TIM6, 16999U, 999U));

    TEST_ASSERT_BITS(RCC_APB1ENR1_TIM6EN, RCC_APB1ENR1_TIM6EN, mock_RCC.APB1ENR1);
    TEST_ASSERT_EQUAL_UINT32(16999U, mock_TIM6.PSC);
    TEST_ASSERT_EQUAL_UINT32(999U, mock_TIM6.ARR);
    TEST_ASSERT_EQUAL_HEX32(TIM_CR1_ARPE | TIM_CR1_URS, mock_TIM6.CR1); /* not started */
    TEST_ASSERT_EQUAL_HEX32(TIM_EGR_UG, mock_TIM6.EGR);

    tim_start(TIM6);
    TEST_ASSERT_BITS(TIM_CR1_CEN, TIM_CR1_CEN, mock_TIM6.CR1);
    tim_stop(TIM6);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_TIM6.CR1 & TIM_CR1_CEN);

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_init(TIM6, 0U, 0x10000U)); /* 16-bit ARR */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_init(TIM6, 0x10000U, 10U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_init(TIM6, 0U, 0U));
    TEST_ASSERT_EQUAL(DRV_OK, tim_init(TIM5, 0U, 0xFFFFFFFFU));
}

static void test_init_frequency_and_update_irq(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, tim_init_frequency(TIM7, 10U));
    /* 17e6 ticks are not exactly representable: within 0.01 %. */
    TEST_ASSERT_UINT64_WITHIN(1700U, 17000000ULL, (uint64_t)(mock_TIM7.PSC + 1U) * (mock_TIM7.ARR + 1U));

    TEST_ASSERT_EQUAL(DRV_OK, tim_enable_update_irq(TIM7, 4U));
    TEST_ASSERT_BITS(TIM_DIER_UIE, TIM_DIER_UIE, mock_TIM7.DIER);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_nvic_enabled[TIM7_DAC_IRQn]);
    TEST_ASSERT_EQUAL_UINT32(4U, mock_nvic_priority[TIM7_DAC_IRQn]);
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, tim_enable_update_irq(TIM7, 16U));

    mock_TIM7.SR = 0U;
    TEST_ASSERT_FALSE(tim_take_update(TIM7));
    mock_TIM7.SR = TIM_SR_UIF;
    TEST_ASSERT_TRUE(tim_take_update(TIM7));
    TEST_ASSERT_EQUAL_HEX32(~(uint32_t)TIM_SR_UIF, mock_TIM7.SR); /* rc_w0 write */

    TEST_ASSERT_EQUAL(DRV_OK, tim_disable_update_irq(TIM7));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_TIM7.DIER & TIM_DIER_UIE);
    TEST_ASSERT_EQUAL(DRV_OK, tim_deinit(TIM7));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.APB1ENR1 & RCC_APB1ENR1_TIM7EN);
}

static void test_counter_masks_uifcpy_on_16_bit_timers(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, tim_init(TIM3, 0U, 1000U));
    mock_TIM3.CNT = 0x80000123U;
    TEST_ASSERT_EQUAL_UINT32(0x123U, tim_get_counter(TIM3));
}

static void test_pwm_channel_configuration(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, pwm_init(TIM3, 0U, 999U));
    TEST_ASSERT_BITS(TIM_CR1_CEN, TIM_CR1_CEN, mock_TIM3.CR1);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_TIM3.BDTR & TIM_BDTR_MOE);

    mock_TIM3.CCMR1 = 0xFFFFFFFFU; /* channel 1 fields must be preserved */
    TEST_ASSERT_EQUAL(DRV_OK, pwm_enable_channel(TIM3, PWM_CHANNEL_2, PWM_POLARITY_ACTIVE_LOW));

    const uint32_t ch2_fields = 0x0100FF00UL;
    TEST_ASSERT_EQUAL_HEX32(0xFEFF00FFUL, mock_TIM3.CCMR1 & ~ch2_fields);
    TEST_ASSERT_EQUAL_HEX32((6UL << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE,
                            mock_TIM3.CCMR1 & ch2_fields);
    TEST_ASSERT_EQUAL_HEX32(TIM_CCER_CC2E | TIM_CCER_CC2P, mock_TIM3.CCER);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_TIM3.CCR2);

    TEST_ASSERT_EQUAL(DRV_OK, pwm_enable_channel(TIM3, PWM_CHANNEL_4, PWM_POLARITY_ACTIVE_HIGH));
    TEST_ASSERT_EQUAL_HEX32((6UL << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE, mock_TIM3.CCMR2);
    TEST_ASSERT_BITS(TIM_CCER_CC4E | TIM_CCER_CC4P, TIM_CCER_CC4E, mock_TIM3.CCER);

    TEST_ASSERT_EQUAL(DRV_OK, pwm_disable_channel(TIM3, PWM_CHANNEL_2));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_TIM3.CCER & TIM_CCER_CC2E);
}

static void test_pwm_main_output_on_advanced_and_15_17_timers(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, pwm_init_frequency(TIM1, 20000U));
    TEST_ASSERT_BITS(TIM_BDTR_MOE, TIM_BDTR_MOE, mock_TIM1.BDTR);
    TEST_ASSERT_EQUAL_UINT32(8500U, pwm_get_period_ticks(TIM1));

    TEST_ASSERT_EQUAL(DRV_OK, pwm_init_frequency(TIM16, 1000U));
    TEST_ASSERT_BITS(TIM_BDTR_MOE, TIM_BDTR_MOE, mock_TIM16.BDTR);
}

static void test_pwm_rejects_invalid_channels(void)
{
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, pwm_init(TIM6, 0U, 99U));          /* basic timer */
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, pwm_enable_channel(TIM16, PWM_CHANNEL_2, PWM_POLARITY_ACTIVE_HIGH));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, pwm_enable_channel(TIM15, PWM_CHANNEL_3, PWM_POLARITY_ACTIVE_HIGH));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, pwm_enable_channel(TIM3, (PwmChannel)0, PWM_POLARITY_ACTIVE_HIGH));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, pwm_enable_channel(TIM3, PWM_CHANNEL_1, (PwmPolarity)2));
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, pwm_init_frequency(TIM3, 200000000U));
    TEST_ASSERT_TRUE(MOCK_PANICS(pwm_set_compare(TIM16, PWM_CHANNEL_2, 1U)));
}

static void test_pwm_duty_and_compare(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, pwm_init(TIM3, 0U, 999U));
    TEST_ASSERT_EQUAL(DRV_OK, pwm_enable_channel(TIM3, PWM_CHANNEL_1, PWM_POLARITY_ACTIVE_HIGH));

    pwm_set_duty_permille(TIM3, PWM_CHANNEL_1, 250U);
    TEST_ASSERT_EQUAL_UINT32(250U, mock_TIM3.CCR1);
    pwm_set_duty_permille(TIM3, PWM_CHANNEL_1, 1000U);
    TEST_ASSERT_EQUAL_UINT32(1000U, mock_TIM3.CCR1); /* > ARR: always active, 100 % */
    pwm_set_duty_permille(TIM3, PWM_CHANNEL_1, 5000U);
    TEST_ASSERT_EQUAL_UINT32(1000U, mock_TIM3.CCR1);
    pwm_set_compare(TIM3, PWM_CHANNEL_1, 123456U);
    TEST_ASSERT_EQUAL_UINT32(1000U, mock_TIM3.CCR1);
    /* Duty updates must not generate an update event (no counter reset). */
    mock_TIM3.EGR = 0U;
    pwm_set_compare(TIM3, PWM_CHANNEL_1, 10U);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_TIM3.EGR);

    TEST_ASSERT_EQUAL_UINT32(0U, pwm_duty_to_compare(8500U, 0U));
    TEST_ASSERT_EQUAL_UINT32(4250U, pwm_duty_to_compare(8500U, 500U));
    TEST_ASSERT_EQUAL_UINT32(9U, pwm_duty_to_compare(8500U, 1U)); /* 8.5 rounds up */
    TEST_ASSERT_EQUAL_UINT32(8500U, pwm_duty_to_compare(8500U, 1001U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_calc_period_16_and_32_bit);
    RUN_TEST(test_calc_period_limits);
    RUN_TEST(test_instance_properties);
    RUN_TEST(test_timer_clock_doubles_when_apb_divided);
    RUN_TEST(test_init_programs_time_base);
    RUN_TEST(test_init_frequency_and_update_irq);
    RUN_TEST(test_counter_masks_uifcpy_on_16_bit_timers);
    RUN_TEST(test_pwm_channel_configuration);
    RUN_TEST(test_pwm_main_output_on_advanced_and_15_17_timers);
    RUN_TEST(test_pwm_rejects_invalid_channels);
    RUN_TEST(test_pwm_duty_and_compare);
    return UNITY_END();
}
