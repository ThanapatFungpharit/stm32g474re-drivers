/*
 * ADC: measure the A0 pin (PA0 = ADC1_IN1), VDDA (via VREFINT) and the die
 * temperature twice per second; print them on the virtual COM port
 * (115200 8N1).
 *
 * Demonstrates: gpio_init_analog(), adc_enable_internal_channels(),
 * adc_init(), adc_read_channel(), factory calibration values and the
 * conversion helpers.
 *
 * Hardware check: VDDA should read close to 3300 mV on the Nucleo; A0 tied
 * to GND reads ~0 mV, tied to 3V3 reads ~VDDA; the temperature should be a
 * few degrees above room temperature and rise when you touch the chip.
 */

#include "board/nucleo_g474re.h"
#include "drivers/adc/adc.h"
#include "drivers/gpio/gpio.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

static UartHandle vcp;

static void check(const DrvStatus status, const char *what)
{
    if (status != DRV_OK) {
        console_puts(what);
        console_puts(": ");
        panic(drv_status_str(status));
    }
}

int main(void)
{
    if (clock_set_preset(CLOCK_PRESET_PLL_170MHZ) != DRV_OK) {
        panic("clock");
    }
    time_init();
    check(board_vcp_init(&vcp, 115200U), "vcp");
    console_set_output(uart_console_write, &vcp);

    check(gpio_init_analog(GPIOA, 0U), "pa0");
    check(adc_enable_internal_channels(ADC1, true, true, false), "internal channels");
    check(adc_init(ADC1), "adc_init");

    console_puts("\r\nADC1: A0 (PA0), VDDA and temperature\r\n");

    for (;;) {
        uint16_t vref_raw;
        uint16_t temp_raw;
        uint16_t a0_raw;

        check(adc_read_channel(ADC1, ADC_CHANNEL_VREFINT, ADC_SAMPLE_640_5, &vref_raw), "vrefint");
        check(adc_read_channel(ADC1, ADC_CHANNEL_TEMPSENSOR_ADC1, ADC_SAMPLE_640_5, &temp_raw), "temp");
        check(adc_read_channel(ADC1, 1U, ADC_SAMPLE_47_5, &a0_raw), "a0");

        const uint32_t vdda_mv = adc_calc_vdda_mv(vref_raw, adc_vrefint_cal());
        const int32_t temp_cc = adc_calc_temperature_centi_c(temp_raw, vdda_mv,
                                                             adc_ts_cal1(), adc_ts_cal2());

        console_puts("VDDA=");
        console_put_udec(vdda_mv);
        console_puts(" mV  A0=");
        console_put_udec(a0_raw);
        console_puts(" (");
        console_put_udec(adc_calc_mv(a0_raw, vdda_mv));
        console_puts(" mV)  T=");
        console_put_dec(temp_cc / 100);
        console_putc('.');
        console_put_udec((uint32_t)((temp_cc < 0) ? -temp_cc : temp_cc) % 100U / 10U);
        console_puts(" C\r\n");

        time_delay_ms(500U);
    }
}
