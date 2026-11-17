/*
 * Driver playground for the NUCLEO-G474RE.
 *
 *   PA5  - LD2 green LED: blinks the mode number (1-4 flashes per second)
 *   PC13 - B1 user button (active high): short press = next waveform,
 *          hold = pause/resume
 *   PA4  - DAC1 channel 1 output (Arduino A2)
 *   PA6  - TIM3 channel 1, 1 kHz PWM (AF2, Arduino D12); duty follows the DAC
 *   PA2/PA3 - LPUART1 on the ST-LINK virtual COM port, 115200 8N1
 *
 * Connect an oscilloscope or an LED with a series resistor to PA4 and PA6.
 * Status messages appear on the virtual COM port and, with a debugger
 * attached, on the SWO/ITM console.
 *
 * Small single-peripheral examples live in examples/.
 */

#include "stm32g474xx.h"

#include "board/nucleo_g474re.h"
#include "drivers/dac/dac.h"
#include "drivers/gpio/gpio.h"
#include "drivers/pwm/pwm.h"
#include "drivers/uart/uart.h"
#include "system/clock/clock.h"
#include "system/console/console.h"
#include "system/panic/panic.h"
#include "system/time/time.h"

enum {
    DAC_OUT_PIN        = 4U,  /* PA4 = DAC1_OUT1 */
    PWM_OUT_PIN        = 6U,  /* PA6 = TIM3_CH1 */
    PWM_OUT_AF         = 2U,
    PWM_FREQUENCY_HZ   = 1000U,
    BUTTON_DEBOUNCE_MS = 30U,
    BUTTON_HOLD_MS     = 700U,
    UPDATE_PERIOD_MS   = 10U,
    UART_TIMEOUT_MS    = 100U,
};

typedef enum {
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_SQUARE,
    WAVE_STEPS,
    WAVE_COUNT,
} WaveMode;

static UartHandle vcp;

/** @brief Write to the VCP and the ITM console. */
static void say(const char *text)
{
    (void)uart_write_string(&vcp, text, UART_TIMEOUT_MS);
    console_puts(text);
}

/** @brief Panic with the failing call's status if it did not succeed. */
static void check(const DrvStatus status, const char *what)
{
    if (status != DRV_OK) {
        console_puts(what);
        console_puts(": ");
        panic(drv_status_str(status));
    }
}

static const char *wave_name(const WaveMode mode)
{
    switch (mode) {
        case WAVE_TRIANGLE: return "triangle (4 s)";
        case WAVE_SAWTOOTH: return "sawtooth (2 s)";
        case WAVE_SQUARE:   return "square (1 Hz)";
        case WAVE_STEPS:    return "eight voltage steps";
        default:            return "unknown";
    }
}

static uint16_t wave_value(const WaveMode mode, const uint32_t elapsed_ms)
{
    uint32_t phase;

    switch (mode) {
        case WAVE_TRIANGLE:
            phase = elapsed_ms % 4000U;
            return (uint16_t)((phase <= 2000U)
                ? (phase * DAC_MAX_VALUE) / 2000U
                : ((4000U - phase) * DAC_MAX_VALUE) / 2000U);

        case WAVE_SAWTOOTH:
            phase = elapsed_ms % 2000U;
            return (uint16_t)((phase * DAC_MAX_VALUE) / 1999U);

        case WAVE_SQUARE:
            return ((elapsed_ms % 1000U) < 500U) ? (uint16_t)DAC_MAX_VALUE : 0U;

        case WAVE_STEPS:
            return (uint16_t)((((elapsed_ms / 300U) % 8U) * DAC_MAX_VALUE) / 7U);

        default:
            return 0U;
    }
}

static void announce_mode(const WaveMode mode)
{
    say("Mode: ");
    say(wave_name(mode));
    say("\r\n");
}

int main(void)
{
    WaveMode mode = WAVE_TRIANGLE;
    bool paused = false;
    bool button_down = false;
    bool hold_handled = false;
    uint32_t waveform_elapsed_ms = 0U;
    uint32_t button_pressed_ms = 0U;

    panic_enable_fault_exceptions();
    console_init();
    check(clock_set_preset(CLOCK_PRESET_PLL_80MHZ), "clock");
    time_init();

    check(board_led_init(), "led");
    check(board_button_init(), "button");
    check(board_vcp_init(&vcp, 115200U), "vcp");

    check(gpio_init_analog(GPIOA, DAC_OUT_PIN), "dac pin");
    check(dac_init(DAC1, DAC_CHANNEL_1), "dac");

    check(gpio_init_af(GPIOA, PWM_OUT_PIN, PWM_OUT_AF, GPIO_OTYPE_PUSH_PULL,
                       GPIO_PULL_NONE, GPIO_SPEED_LOW), "pwm pin");
    check(pwm_init_frequency(TIM3, PWM_FREQUENCY_HZ), "pwm");
    check(pwm_enable_channel(TIM3, PWM_CHANNEL_1, PWM_POLARITY_ACTIVE_HIGH), "pwm channel");

    say("\r\nSTM32G474RE driver playground started\r\n");
    say("Short B1 press: next waveform; hold B1: pause/resume\r\n");
    say("PA4 is DAC1_OUT1; PA6 is 1 kHz TIM3_CH1 PWM\r\n");
    announce_mode(mode);

    bool raw_button = board_button_pressed();
    bool stable_button = raw_button;
    uint32_t last_update_ms = time_millis();
    uint32_t raw_changed_ms = last_update_ms;

    for (;;) {
        const uint32_t now_ms = time_millis();
        const uint32_t elapsed_ms = now_ms - last_update_ms;
        const bool new_raw_button = board_button_pressed();

        last_update_ms = now_ms;

        /* Debounce B1 before acting on a press or release. */
        if (new_raw_button != raw_button) {
            raw_button = new_raw_button;
            raw_changed_ms = now_ms;
        }
        if ((raw_button != stable_button) &&
            ((now_ms - raw_changed_ms) >= BUTTON_DEBOUNCE_MS)) {
            stable_button = raw_button;

            if (stable_button) {
                button_down = true;
                hold_handled = false;
                button_pressed_ms = now_ms;
            } else if (button_down) {
                button_down = false;
                if (!hold_handled) {
                    mode = (WaveMode)(((uint32_t)mode + 1U) % (uint32_t)WAVE_COUNT);
                    waveform_elapsed_ms = 0U;
                    announce_mode(mode);
                }
            }
        }

        if (button_down && !hold_handled &&
            ((now_ms - button_pressed_ms) >= BUTTON_HOLD_MS)) {
            paused = !paused;
            hold_handled = true;
            say(paused ? "Paused (hold B1 again to resume)\r\n" : "Running\r\n");
        }

        if (!paused) {
            waveform_elapsed_ms += elapsed_ms;
        }

        const uint16_t dac_value = wave_value(mode, waveform_elapsed_ms);
        dac_write(DAC1, DAC_CHANNEL_1, dac_value);
        pwm_set_duty_permille(TIM3, PWM_CHANNEL_1,
                              ((uint32_t)dac_value * 1000U) / DAC_MAX_VALUE);

        /* LD2 shows the mode number as 1-4 flashes per second. */
        board_led_set(paused ||
                      (((waveform_elapsed_ms % 1000U) / 125U) < (((uint32_t)mode + 1U) * 2U) &&
                       (((waveform_elapsed_ms % 1000U) / 125U) % 2U) == 0U));

        time_delay_ms(UPDATE_PERIOD_MS);
    }
}
