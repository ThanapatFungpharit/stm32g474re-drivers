/* Host tests: ADC sequencing/channel setup and DAC configuration. */
#include <unity.h>

#include "drivers/adc/adc.h"
#include "drivers/dac/dac.h"

static bool calibration_completes;
static bool conversion_completes;
static bool dac_ready;
static uint16_t conversion_result;

/* Emulates ADC1 and DAC1/DAC3 status behaviour. */
static void analog_hardware(void)
{
    ADC_TypeDef *adc = &mock_ADC1;

    if (((adc->CR & ADC_CR_ADCAL) != 0U) && calibration_completes) {
        adc->CR &= ~ADC_CR_ADCAL;
    }
    if ((adc->CR & ADC_CR_ADDIS) != 0U) {
        adc->CR &= ~(ADC_CR_ADDIS | ADC_CR_ADEN);
    }
    if ((adc->CR & ADC_CR_ADEN) != 0U) {
        adc->ISR |= ADC_ISR_ADRDY;
    }
    if ((adc->CR & ADC_CR_ADSTP) != 0U) {
        adc->CR &= ~(ADC_CR_ADSTP | ADC_CR_ADSTART);
    }
    if (((adc->CR & ADC_CR_ADSTART) != 0U) && conversion_completes) {
        adc->DR = conversion_result;
        adc->ISR |= ADC_ISR_EOC;
        adc->CR &= ~ADC_CR_ADSTART;
    }

    DAC_TypeDef *dacs[] = { &mock_DAC1, &mock_DAC3 };
    for (unsigned i = 0U; i < 2U; ++i) {
        if (dac_ready && ((dacs[i]->CR & DAC_CR_EN1) != 0U)) dacs[i]->SR |= DAC_SR_DAC1RDY;
        if (dac_ready && ((dacs[i]->CR & DAC_CR_EN2) != 0U)) dacs[i]->SR |= DAC_SR_DAC2RDY;
    }
}

void setUp(void)
{
    mock_reset();
    calibration_completes = true;
    conversion_completes = true;
    dac_ready = true;
    conversion_result = 0U;
    mock_hw_hook = analog_hardware;
    SystemCoreClock = 170000000U;
}

void tearDown(void) {}

/* --- ADC ---------------------------------------------------------------------- */

static void test_adc_init_sequence(void)
{
    mock_ADC1.CR = ADC_CR_DEEPPWD; /* reset state */

    TEST_ASSERT_EQUAL(DRV_OK, adc_init(ADC1));

    TEST_ASSERT_BITS(RCC_AHB2ENR_ADC12EN, RCC_AHB2ENR_ADC12EN, mock_RCC.AHB2ENR);
    /* 170 MHz HCLK: synchronous clock HCLK/4 = 42.5 MHz. */
    TEST_ASSERT_EQUAL_HEX32(ADC_CCR_CKMODE, mock_ADC12_COMMON.CCR & ADC_CCR_CKMODE);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_ADC1.CR & ADC_CR_DEEPPWD);
    TEST_ASSERT_BITS(ADC_CR_ADVREGEN | ADC_CR_ADEN, ADC_CR_ADVREGEN | ADC_CR_ADEN, mock_ADC1.CR);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_ADC1.CR & ADC_CR_ADCALDIF); /* single-ended calibration */
    TEST_ASSERT_EQUAL_HEX32(0U, mock_ADC1.CFGR & (ADC_CFGR_RES | ADC_CFGR_CONT | ADC_CFGR_EXTEN));
    TEST_ASSERT_BITS(ADC_CFGR_OVRMOD, ADC_CFGR_OVRMOD, mock_ADC1.CFGR);
    TEST_ASSERT_TRUE(adc_is_enabled(ADC1));
}

static void test_adc_clock_divider_follows_hclk(void)
{
    SystemCoreClock = 80000000U;
    TEST_ASSERT_EQUAL(DRV_OK, adc_init(ADC1));
    TEST_ASSERT_EQUAL_HEX32(ADC_CCR_CKMODE_1, mock_ADC12_COMMON.CCR & ADC_CCR_CKMODE); /* HCLK/2 */
}

static void test_adc_calibration_timeout(void)
{
    calibration_completes = false;
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, adc_init(ADC1));
    TEST_ASSERT_FALSE(adc_is_enabled(ADC1));
}

static void test_adc_group_clock_conflict(void)
{
    /* ADC2 runs with the asynchronous clock but no kernel clock selected:
     * ADC1 cannot get a clock and must not hang in calibration. */
    mock_RCC.AHB2ENR |= RCC_AHB2ENR_ADC12EN;
    mock_ADC2.CR = ADC_CR_ADEN;
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, adc_init(ADC1));
}

static void test_adc_channel_and_sample_time_registers(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, adc_init(ADC1));

    TEST_ASSERT_EQUAL(DRV_OK, adc_config_channel(ADC1, 1U, ADC_SAMPLE_47_5));
    TEST_ASSERT_EQUAL_HEX32(1UL << ADC_SQR1_SQ1_Pos, mock_ADC1.SQR1);
    TEST_ASSERT_EQUAL_HEX32(4UL << 3, mock_ADC1.SMPR1); /* SMP1 */

    TEST_ASSERT_EQUAL(DRV_OK, adc_config_channel(ADC1, ADC_CHANNEL_VREFINT, ADC_SAMPLE_640_5));
    TEST_ASSERT_EQUAL_HEX32(18UL << ADC_SQR1_SQ1_Pos, mock_ADC1.SQR1);
    TEST_ASSERT_EQUAL_HEX32(7UL << 24, mock_ADC1.SMPR2); /* SMP18 */
    TEST_ASSERT_EQUAL_HEX32(4UL << 3, mock_ADC1.SMPR1);  /* untouched */

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, adc_config_channel(ADC1, 19U, ADC_SAMPLE_2_5));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, adc_config_channel(ADC1, 1U, (AdcSampleTime)8));
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, adc_config_channel(ADC2, 1U, ADC_SAMPLE_2_5)); /* not enabled */
}

static void test_adc_read(void)
{
    uint16_t value = 0U;

    TEST_ASSERT_EQUAL(DRV_OK, adc_init(ADC1));
    conversion_result = 0xABCU;
    TEST_ASSERT_EQUAL(DRV_OK, adc_read_channel(ADC1, 3U, ADC_SAMPLE_12_5, &value));
    TEST_ASSERT_EQUAL_UINT16(0xABCU, value);

    conversion_completes = false;
    mock_ADC1.ISR &= ~ADC_ISR_EOC; /* the mock does not emulate DR-read clearing */
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, adc_read(ADC1, &value, 50U));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_ADC1.CR & ADC_CR_ADSTART); /* aborted */

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, adc_read(ADC1, NULL, 50U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, adc_read((ADC_TypeDef *)&mock_DAC1, &value, 50U));
}

static void test_adc_internal_channels_and_deinit(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, adc_enable_internal_channels(ADC1, true, true, false));
    TEST_ASSERT_EQUAL_HEX32(ADC_CCR_VREFEN | ADC_CCR_VSENSESEL,
                            mock_ADC12_COMMON.CCR & (ADC_CCR_VREFEN | ADC_CCR_VSENSESEL | ADC_CCR_VBATSEL));

    TEST_ASSERT_EQUAL(DRV_OK, adc_init(ADC1));
    mock_ADC1.CR |= ADC_CR_ADSTART;
    conversion_completes = false;
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, adc_enable_internal_channels(ADC1, false, false, false));

    TEST_ASSERT_EQUAL(DRV_OK, adc_deinit(ADC1));
    TEST_ASSERT_BITS(ADC_CR_DEEPPWD | ADC_CR_ADVREGEN | ADC_CR_ADEN, ADC_CR_DEEPPWD, mock_ADC1.CR);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.AHB2ENR & RCC_AHB2ENR_ADC12EN);
}

static void test_adc_conversion_helpers(void)
{
    TEST_ASSERT_EQUAL_UINT32(3299U, adc_calc_vdda_mv(1505U, 1655U));
    TEST_ASSERT_EQUAL_UINT32(3000U, adc_calc_vdda_mv(1655U, 1655U));
    TEST_ASSERT_EQUAL_UINT32(0U, adc_calc_vdda_mv(0U, 1655U));

    TEST_ASSERT_EQUAL_UINT32(3300U, adc_calc_mv(4095U, 3300U));
    TEST_ASSERT_EQUAL_UINT32(1650U, adc_calc_mv(2048U, 3300U));
    TEST_ASSERT_EQUAL_UINT32(0U, adc_calc_mv(0U, 3300U));

    TEST_ASSERT_EQUAL_INT32(3000, adc_calc_temperature_centi_c(1000U, 3000U, 1000U, 1400U));
    TEST_ASSERT_EQUAL_INT32(13000, adc_calc_temperature_centi_c(1400U, 3000U, 1000U, 1400U));
    TEST_ASSERT_EQUAL_INT32(8000, adc_calc_temperature_centi_c(1200U, 3000U, 1000U, 1400U));
    /* Same physical temperature (30 °C) measured with VDDA = 3.3 V. */
    TEST_ASSERT_INT32_WITHIN(5, 3000, adc_calc_temperature_centi_c(909U, 3300U, 1000U, 1400U));
    TEST_ASSERT_EQUAL_INT32(-2000, adc_calc_temperature_centi_c(800U, 3000U, 1000U, 1400U));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, adc_calc_temperature_centi_c(800U, 3000U, 1400U, 1400U));
}

/* --- DAC ---------------------------------------------------------------------- */

static void test_dac1_init_buffered_pin_and_hfsel(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, dac_init(DAC1, DAC_CHANNEL_1));

    TEST_ASSERT_BITS(RCC_AHB2ENR_DAC1EN, RCC_AHB2ENR_DAC1EN, mock_RCC.AHB2ENR);
    TEST_ASSERT_EQUAL_HEX32(DAC_MCR_HFSEL_1, mock_DAC1.MCR & DAC_MCR_HFSEL); /* > 160 MHz */
    TEST_ASSERT_EQUAL_HEX32(0U, mock_DAC1.MCR & DAC_MCR_MODE1);             /* pin, buffered */
    TEST_ASSERT_EQUAL_HEX32(DAC_CR_EN1, mock_DAC1.CR);
}

static void test_dac_hfsel_thresholds(void)
{
    SystemCoreClock = 80000000U;
    TEST_ASSERT_EQUAL(DRV_OK, dac_init(DAC1, DAC_CHANNEL_1));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_DAC1.MCR & DAC_MCR_HFSEL);

    TEST_ASSERT_EQUAL(DRV_OK, dac_deinit(DAC1, DAC_CHANNEL_1));
    SystemCoreClock = 100000000U;
    TEST_ASSERT_EQUAL(DRV_OK, dac_init(DAC1, DAC_CHANNEL_1));
    TEST_ASSERT_EQUAL_HEX32(DAC_MCR_HFSEL_0, mock_DAC1.MCR & DAC_MCR_HFSEL);

    /* Channel 1 runs with the 80-160 MHz setting; channel 2 at 170 MHz
     * cannot change the shared HFSEL field. */
    SystemCoreClock = 170000000U;
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, dac_init(DAC1, DAC_CHANNEL_2));
}

static void test_dac3_is_internal_unbuffered(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, dac_init(DAC3, DAC_CHANNEL_2));
    TEST_ASSERT_EQUAL_HEX32(3UL << DAC_MCR_MODE2_Pos, mock_DAC3.MCR & DAC_MCR_MODE2);
    TEST_ASSERT_EQUAL_HEX32(DAC_CR_EN2, mock_DAC3.CR);
}

static void test_dac_errors(void)
{
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, dac_init(DAC2, DAC_CHANNEL_2));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, dac_init(DAC1, (DacChannel)3));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, dac_init((DAC_TypeDef *)&mock_ADC1, DAC_CHANNEL_1));

    dac_ready = false;
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, dac_init(DAC1, DAC_CHANNEL_1));
}

static void test_dac_write(void)
{
    TEST_ASSERT_EQUAL(DRV_OK, dac_init(DAC1, DAC_CHANNEL_2));
    dac_write(DAC1, DAC_CHANNEL_2, 4095U);
    TEST_ASSERT_EQUAL_UINT32(4095U, mock_DAC1.DHR12R2);
    dac_write(DAC1, DAC_CHANNEL_1, 7U);
    TEST_ASSERT_EQUAL_UINT32(7U, mock_DAC1.DHR12R1);

    TEST_ASSERT_TRUE(MOCK_PANICS(dac_write(DAC1, DAC_CHANNEL_1, 4096U)));
    TEST_ASSERT_TRUE(MOCK_PANICS(dac_write(DAC2, DAC_CHANNEL_2, 1U)));

    TEST_ASSERT_EQUAL_UINT16(0U, dac_mv_to_code(0U, 3300U));
    TEST_ASSERT_EQUAL_UINT16(2048U, dac_mv_to_code(1650U, 3300U));
    TEST_ASSERT_EQUAL_UINT16(4095U, dac_mv_to_code(3300U, 3300U)); /* clamped */
    TEST_ASSERT_EQUAL_UINT16(0U, dac_mv_to_code(1000U, 0U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_adc_init_sequence);
    RUN_TEST(test_adc_clock_divider_follows_hclk);
    RUN_TEST(test_adc_calibration_timeout);
    RUN_TEST(test_adc_group_clock_conflict);
    RUN_TEST(test_adc_channel_and_sample_time_registers);
    RUN_TEST(test_adc_read);
    RUN_TEST(test_adc_internal_channels_and_deinit);
    RUN_TEST(test_adc_conversion_helpers);
    RUN_TEST(test_dac1_init_buffered_pin_and_hfsel);
    RUN_TEST(test_dac_hfsel_thresholds);
    RUN_TEST(test_dac3_is_internal_unbuffered);
    RUN_TEST(test_dac_errors);
    RUN_TEST(test_dac_write);
    return UNITY_END();
}
