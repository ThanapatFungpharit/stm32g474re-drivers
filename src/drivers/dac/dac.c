#include "dac.h"

#include "drivers/internal/drv_internal.h"


/* Channel wake-up time is a few µs (datasheet tWAKEUP); allow margin. */
#define READY_TIMEOUT_US 1000U

/* MODEx encodings (DAC_MCR). */
#define MODE_PIN_BUFFERED      0x0UL /* normal mode, external pin, buffer on */
#define MODE_INTERNAL_UNBUFFERED 0x3UL /* normal mode, on-chip peripherals, buffer off */

typedef struct {
    DrvClockGate gate;
    uint32_t     channels;
    bool         has_pins;
} DacInfo;

static bool dac_info(const DAC_TypeDef *dac, DacInfo *info)
{
    if (dac == DAC1) { const DacInfo i = { { DRV_BUS_AHB2, RCC_AHB2ENR_DAC1EN }, 2U, true  }; *info = i; return true; }
    if (dac == DAC2) { const DacInfo i = { { DRV_BUS_AHB2, RCC_AHB2ENR_DAC2EN }, 1U, true  }; *info = i; return true; }
    if (dac == DAC3) { const DacInfo i = { { DRV_BUS_AHB2, RCC_AHB2ENR_DAC3EN }, 2U, false }; *info = i; return true; }
    if (dac == DAC4) { const DacInfo i = { { DRV_BUS_AHB2, RCC_AHB2ENR_DAC4EN }, 2U, false }; *info = i; return true; }
    return false;
}

static bool valid(const DAC_TypeDef *dac, const DacChannel channel, DacInfo *info)
{
    return dac_info(dac, info) &&
           ((uint32_t)channel >= 1U) && ((uint32_t)channel <= info->channels);
}

/** @brief HFSEL setting required for the current HCLK. */
static uint32_t hfsel_for_hclk(const uint32_t hclk_hz)
{
    if (hclk_hz > 160000000UL) {
        return DAC_MCR_HFSEL_1;
    }
    if (hclk_hz > 80000000UL) {
        return DAC_MCR_HFSEL_0;
    }
    return 0U;
}

DrvStatus dac_init(DAC_TypeDef *dac, const DacChannel channel)
{
    DacInfo info;
    if (!valid(dac, channel, &info)) {
        return DRV_ERR_PARAM;
    }

    if (!drv_clock_gate_is_enabled(info.gate)) {
        drv_clock_gate_set(info.gate, true);
    }

    /* Register fields of channel 2 are 16 bits above those of channel 1. */
    const uint32_t shift = (channel == DAC_CHANNEL_2) ? 16U : 0U;
    const uint32_t en = DAC_CR_EN1 << shift;
    const uint32_t other_en = (channel == DAC_CHANNEL_2) ? DAC_CR_EN1 : DAC_CR_EN2;
    const uint32_t hfsel = hfsel_for_hclk(SystemCoreClock);

    /* MODE may only change while the channel is disabled. */
    drv_reg_modify(&dac->CR, en, 0U);

    if ((dac->CR & other_en) == 0U) {
        drv_reg_modify(&dac->MCR, DAC_MCR_HFSEL, hfsel);
    } else if ((dac->MCR & DAC_MCR_HFSEL) != hfsel) {
        return DRV_ERR_STATE;
    }

    const uint32_t mode = info.has_pins ? MODE_PIN_BUFFERED : MODE_INTERNAL_UNBUFFERED;
    drv_reg_modify(&dac->MCR, DAC_MCR_MODE1 << shift, (mode << DAC_MCR_MODE1_Pos) << shift);

    /* No trigger: data register writes go straight to the output. */
    drv_reg_modify(&dac->CR, (DAC_CR_TEN1 | DAC_CR_DMAEN1 | DAC_CR_WAVE1) << shift, 0U);

    if (channel == DAC_CHANNEL_1) {
        dac->DHR12R1 = 0U;
    } else {
        dac->DHR12R2 = 0U;
    }

    drv_reg_modify(&dac->CR, 0U, en);

    const uint32_t ready = (channel == DAC_CHANNEL_1) ? DAC_SR_DAC1RDY : DAC_SR_DAC2RDY;
    return drv_wait_set(&dac->SR, ready, READY_TIMEOUT_US);
}

DrvStatus dac_deinit(DAC_TypeDef *dac, const DacChannel channel)
{
    DacInfo info;
    if (!valid(dac, channel, &info)) {
        return DRV_ERR_PARAM;
    }
    if (!drv_clock_gate_is_enabled(info.gate)) {
        return DRV_OK;
    }

    drv_reg_modify(&dac->CR, (channel == DAC_CHANNEL_2) ? DAC_CR_EN2 : DAC_CR_EN1, 0U);

    if ((dac->CR & (DAC_CR_EN1 | DAC_CR_EN2)) == 0U) {
        drv_clock_gate_set(info.gate, false);
    }
    return DRV_OK;
}

void dac_write(DAC_TypeDef *dac, const DacChannel channel, const uint16_t value)
{
    DRV_ASSERT(value <= DAC_MAX_VALUE);
    DRV_ASSERT((channel == DAC_CHANNEL_1) ||
               ((channel == DAC_CHANNEL_2) && (dac != DAC2)));

    if (channel == DAC_CHANNEL_2) {
        dac->DHR12R2 = value;
    } else {
        dac->DHR12R1 = value;
    }
}

uint16_t dac_mv_to_code(const uint32_t mv, const uint32_t vref_mv)
{
    if (vref_mv == 0U) {
        return 0U;
    }
    const uint64_t code = (((uint64_t)mv * 4096U) + (vref_mv / 2U)) / vref_mv;
    return (code > DAC_MAX_VALUE) ? (uint16_t)DAC_MAX_VALUE : (uint16_t)code;
}
