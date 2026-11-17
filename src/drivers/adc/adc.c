#include "adc.h"

#include "drivers/internal/drv_internal.h"
#include "system/time/time.h"


/* Factory calibration values in system memory (RM0440 / DS12288). */
#define VREFINT_CAL_ADDRESS   0x1FFF75AAUL /* VREFINT at VDDA = 3.0 V */
#define TS_CAL1_ADDRESS       0x1FFF75A8UL /* temperature sensor at 30 °C, 3.0 V */
#define TS_CAL2_ADDRESS       0x1FFF75CAUL /* temperature sensor at 130 °C, 3.0 V */
#define CAL_VDDA_MV           3000U
#define TS_CAL1_TEMP_C        30
#define TS_CAL2_TEMP_C        130

/* Timing from the datasheet (maximum values). */
#define REGULATOR_STARTUP_US  20U   /* tADCVREG_STUP */
#define SENSOR_STARTUP_US     120U  /* temperature sensor tSTART (VREFINT: 12 µs) */
#define CALIBRATION_TIMEOUT_US 10000U
#define ENABLE_TIMEOUT_US     10000U
#define DISABLE_TIMEOUT_US    10000U

/* Highest permitted ADC clock. */
#define ADC_CLOCK_MAX_HZ      60000000UL


typedef struct {
    ADC_Common_TypeDef *common;
    DrvClockGate        gate;
    ADC_TypeDef        *members[3];
    uint32_t            member_count;
    uint32_t            ccipr_sel_mask;  /* RCC_CCIPR asynchronous clock selection */
} AdcGroup;

static bool adc_group(const ADC_TypeDef *adc, AdcGroup *group)
{
    if ((adc == ADC1) || (adc == ADC2)) {
        const AdcGroup g = {
            ADC12_COMMON, { DRV_BUS_AHB2, RCC_AHB2ENR_ADC12EN },
            { ADC1, ADC2, NULL }, 2U, RCC_CCIPR_ADC12SEL,
        };
        *group = g;
        return true;
    }
    if ((adc == ADC3) || (adc == ADC4) || (adc == ADC5)) {
        const AdcGroup g = {
            ADC345_COMMON, { DRV_BUS_AHB2, RCC_AHB2ENR_ADC345EN },
            { ADC3, ADC4, ADC5 }, 3U, RCC_CCIPR_ADC345SEL,
        };
        *group = g;
        return true;
    }
    return false;
}

static bool group_any(const AdcGroup *group, const uint32_t cr_mask)
{
    for (uint32_t i = 0U; i < group->member_count; ++i) {
        if ((group->members[i]->CR & cr_mask) != 0U) {
            return true;
        }
    }
    return false;
}

/** @brief Synchronous ADC clock divider that keeps f_ADC <= 60 MHz. */
static uint32_t ckmode_for_hclk(const uint32_t hclk_hz)
{
    return ((hclk_hz / 2U) <= ADC_CLOCK_MAX_HZ) ? ADC_CCR_CKMODE_1   /* HCLK/2 */
                                                : ADC_CCR_CKMODE;    /* HCLK/4 */
}

static uint32_t adc_clock_hz(const AdcGroup *group)
{
    switch (group->common->CCR & ADC_CCR_CKMODE) {
        case ADC_CCR_CKMODE_0: return SystemCoreClock;
        case ADC_CCR_CKMODE_1: return SystemCoreClock / 2U;
        case ADC_CCR_CKMODE:   return SystemCoreClock / 4U;
        default:               return 0U; /* asynchronous clock: unknown */
    }
}

static DrvStatus configure_group_clock(const AdcGroup *group)
{
    if (!group_any(group, ADC_CR_ADEN)) {
        /* Common register may be written: use the synchronous AHB clock. */
        drv_reg_modify(&group->common->CCR, ADC_CCR_CKMODE | ADC_CCR_PRESC,
                       ckmode_for_hclk(SystemCoreClock));
        return DRV_OK;
    }

    /* Another ADC of the group is running; its clock setting must be usable. */
    if (((group->common->CCR & ADC_CCR_CKMODE) == 0U) &&
        ((RCC->CCIPR & group->ccipr_sel_mask) == 0U)) {
        return DRV_ERR_STATE; /* asynchronous mode with no kernel clock */
    }
    return DRV_OK;
}

static DrvStatus stop_and_disable(ADC_TypeDef *adc)
{
    DrvStatus status = DRV_OK;

    if ((adc->CR & (ADC_CR_ADSTART | ADC_CR_JADSTART)) != 0U) {
        adc->CR |= ADC_CR_ADSTP | ADC_CR_JADSTP;
        status = drv_wait_clear(&adc->CR, ADC_CR_ADSTART | ADC_CR_JADSTART,
                                DISABLE_TIMEOUT_US);
        if (status != DRV_OK) {
            return status;
        }
    }

    if ((adc->CR & ADC_CR_ADEN) != 0U) {
        adc->CR |= ADC_CR_ADDIS;
        status = drv_wait_clear(&adc->CR, ADC_CR_ADEN, DISABLE_TIMEOUT_US);
    }
    return status;
}

DrvStatus adc_init(ADC_TypeDef *adc)
{
    AdcGroup group;
    if (!adc_group(adc, &group)) {
        return DRV_ERR_PARAM;
    }

    if (!drv_clock_gate_is_enabled(group.gate)) {
        drv_clock_gate_set(group.gate, true);
    }

    DrvStatus status = stop_and_disable(adc);
    if (status != DRV_OK) {
        return status;
    }

    status = configure_group_clock(&group);
    if (status != DRV_OK) {
        return status;
    }

    /* Leave deep-power-down and start the internal voltage regulator. */
    adc->CR &= ~ADC_CR_DEEPPWD;
    adc->CR |= ADC_CR_ADVREGEN;
    time_delay_us(REGULATOR_STARTUP_US);

    /* Single-ended calibration. */
    adc->CR &= ~ADC_CR_ADCALDIF;
    adc->CR |= ADC_CR_ADCAL;
    status = drv_wait_clear(&adc->CR, ADC_CR_ADCAL, CALIBRATION_TIMEOUT_US);
    if (status != DRV_OK) {
        return status;
    }

    /* ADEN may only be set 4 ADC clock cycles after calibration ends. */
    const uint32_t f_adc = adc_clock_hz(&group);
    time_delay_us((f_adc >= 4000000U) ? 1U : 10U);

    /* 12-bit, right aligned, single conversion, software trigger; a new
     * result overwrites an unread one (no overrun stall). */
    adc->CFGR = (adc->CFGR & ~(ADC_CFGR_RES | ADC_CFGR_ALIGN | ADC_CFGR_CONT |
                               ADC_CFGR_EXTEN | ADC_CFGR_EXTSEL | ADC_CFGR_DISCEN |
                               ADC_CFGR_DMAEN))
                | ADC_CFGR_OVRMOD;
    adc->SQR1 &= ~ADC_SQR1_L; /* one conversion in the sequence */

    adc->ISR = ADC_ISR_ADRDY; /* write 1 to clear */
    adc->CR |= ADC_CR_ADEN;
    status = drv_wait_set(&adc->ISR, ADC_ISR_ADRDY, ENABLE_TIMEOUT_US);
    if (status != DRV_OK) {
        return status;
    }
    adc->ISR = ADC_ISR_ADRDY | ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_EOSMP;
    return DRV_OK;
}

DrvStatus adc_deinit(ADC_TypeDef *adc)
{
    AdcGroup group;
    if (!adc_group(adc, &group)) {
        return DRV_ERR_PARAM;
    }
    if (!drv_clock_gate_is_enabled(group.gate)) {
        return DRV_OK; /* already off */
    }

    const DrvStatus status = stop_and_disable(adc);
    if (status != DRV_OK) {
        return status;
    }

    /* Regulator off before entering deep-power-down. */
    adc->CR &= ~ADC_CR_ADVREGEN;
    adc->CR |= ADC_CR_DEEPPWD;

    if (!group_any(&group, ADC_CR_ADEN)) {
        drv_clock_gate_set(group.gate, false);
    }
    return DRV_OK;
}

bool adc_is_enabled(const ADC_TypeDef *adc)
{
    AdcGroup group;
    return adc_group(adc, &group) && drv_clock_gate_is_enabled(group.gate) &&
           ((adc->CR & ADC_CR_ADEN) != 0U);
}

DrvStatus adc_config_channel(ADC_TypeDef *adc, const uint8_t channel,
                             const AdcSampleTime sample_time)
{
    AdcGroup group;
    if (!adc_group(adc, &group) || (channel > ADC_CHANNEL_MAX) ||
        ((uint32_t)sample_time > 7U)) {
        return DRV_ERR_PARAM;
    }
    if (!adc_is_enabled(adc) || ((adc->CR & ADC_CR_ADSTART) != 0U)) {
        return DRV_ERR_STATE;
    }

    /* Single conversion: rank 1 = channel. */
    adc->SQR1 = (adc->SQR1 & ~(ADC_SQR1_L | ADC_SQR1_SQ1)) |
                ((uint32_t)channel << ADC_SQR1_SQ1_Pos);

    /* SMPR1 holds channels 0-9, SMPR2 channels 10-18, 3 bits each. */
    volatile uint32_t *smpr = (channel <= 9U) ? &adc->SMPR1 : &adc->SMPR2;
    const uint32_t shift = ((channel <= 9U) ? channel : (channel - 10U)) * 3U;
    *smpr = (*smpr & ~(0x7UL << shift)) | ((uint32_t)sample_time << shift);
    return DRV_OK;
}

DrvStatus adc_read(ADC_TypeDef *adc, uint16_t *value, const uint32_t timeout_us)
{
    AdcGroup group;
    if (!adc_group(adc, &group) || (value == NULL)) {
        return DRV_ERR_PARAM;
    }
    if (!adc_is_enabled(adc) || ((adc->CR & ADC_CR_ADSTART) != 0U)) {
        return DRV_ERR_STATE;
    }

    /* Clear stale conversion flags (write 1 to clear the ones that are set). */
    adc->ISR = adc->ISR & (ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR | ADC_ISR_EOSMP);
    adc->CR |= ADC_CR_ADSTART;

    const DrvStatus status = drv_wait_set(&adc->ISR, ADC_ISR_EOC, timeout_us);
    if (status != DRV_OK) {
        /* Abort so the ADC is usable again. */
        adc->CR |= ADC_CR_ADSTP;
        (void)drv_wait_clear(&adc->CR, ADC_CR_ADSTART, DISABLE_TIMEOUT_US);
        return status;
    }

    *value = (uint16_t)(adc->DR & ADC_MAX_VALUE); /* reading DR clears EOC */
    return DRV_OK;
}

DrvStatus adc_read_channel(ADC_TypeDef *adc, const uint8_t channel,
                           const AdcSampleTime sample_time, uint16_t *value)
{
    const DrvStatus status = adc_config_channel(adc, channel, sample_time);
    if (status != DRV_OK) {
        return status;
    }
    return adc_read(adc, value, ADC_DEFAULT_TIMEOUT_US);
}

DrvStatus adc_enable_internal_channels(ADC_TypeDef *adc, const bool vrefint,
                                       const bool temp_sensor, const bool vbat)
{
    AdcGroup group;
    if (!adc_group(adc, &group)) {
        return DRV_ERR_PARAM;
    }

    if (!drv_clock_gate_is_enabled(group.gate)) {
        drv_clock_gate_set(group.gate, true);
    }

    if (group_any(&group, ADC_CR_ADSTART | ADC_CR_JADSTART)) {
        return DRV_ERR_STATE;
    }

    const uint32_t before = group.common->CCR;
    const uint32_t set = (vrefint ? ADC_CCR_VREFEN : 0U) |
                         (temp_sensor ? ADC_CCR_VSENSESEL : 0U) |
                         (vbat ? ADC_CCR_VBATSEL : 0U);

    drv_reg_modify(&group.common->CCR,
                   ADC_CCR_VREFEN | ADC_CCR_VSENSESEL | ADC_CCR_VBATSEL, set);

    /* Wait for newly enabled sources to stabilise. */
    if ((set & ~before) != 0U) {
        time_delay_us(SENSOR_STARTUP_US);
    }
    return DRV_OK;
}


/* --- Conversions ------------------------------------------------------------ */

uint32_t adc_calc_vdda_mv(const uint16_t vrefint_raw, const uint16_t vrefint_cal)
{
    if (vrefint_raw == 0U) {
        return 0U;
    }
    return (((uint32_t)CAL_VDDA_MV * vrefint_cal) + (vrefint_raw / 2U)) / vrefint_raw;
}

uint32_t adc_calc_mv(const uint16_t raw, const uint32_t vdda_mv)
{
    return (uint32_t)((((uint64_t)raw * vdda_mv) + (ADC_MAX_VALUE / 2U)) / ADC_MAX_VALUE);
}

int32_t adc_calc_temperature_centi_c(const uint16_t raw, const uint32_t vdda_mv,
                                     const uint16_t ts_cal1, const uint16_t ts_cal2)
{
    if (ts_cal2 <= ts_cal1) {
        return INT32_MIN; /* corrupt calibration data */
    }

    /* T = (raw * VDDA / 3.0 V - TS_CAL1) * (130 - 30) / (TS_CAL2 - TS_CAL1) + 30,
     * evaluated in hundredths of a degree with a single final division. */
    const int64_t num = (((int64_t)raw * (int64_t)vdda_mv) -
                         ((int64_t)ts_cal1 * CAL_VDDA_MV)) *
                        (int64_t)(TS_CAL2_TEMP_C - TS_CAL1_TEMP_C) * 100;
    const int64_t den = ((int64_t)ts_cal2 - (int64_t)ts_cal1) * CAL_VDDA_MV;

    /* Round to nearest (num may be negative below 30 °C). */
    const int64_t q = (num >= 0) ? ((num + (den / 2)) / den) : ((num - (den / 2)) / den);
    return (int32_t)q + (TS_CAL1_TEMP_C * 100);
}

uint16_t adc_vrefint_cal(void)
{
    return *(const volatile uint16_t *)VREFINT_CAL_ADDRESS;
}

uint16_t adc_ts_cal1(void)
{
    return *(const volatile uint16_t *)TS_CAL1_ADDRESS;
}

uint16_t adc_ts_cal2(void)
{
    return *(const volatile uint16_t *)TS_CAL2_ADDRESS;
}
