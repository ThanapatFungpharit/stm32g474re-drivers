#include "stm32g474xx.h"

#include "system/clock/clock.h"
#include "drivers/dac/dac.h"
#include "drivers/gpio/gpio.h"
#include "drivers/pwm/pwm.h"
#include "system/console/console.h"
#include "system/time/time.h"

/*
 * Nucleo-G474RE connections used by this example:
 *
 *   PA5  - LD2, onboard green LED
 *   PC13 - B1, onboard user button (active low)
 *   PA4  - DAC1 channel 1 output
 *   PA6  - TIM3 channel 1 PWM output (AF2)
 *
 * PA4 and PA6 are available on the Arduino-compatible headers. Connect an
 * oscilloscope or LED/resistor to them to observe the DAC voltage and PWM
 * duty cycle. The DAC and PWM always use the same waveform value.
 */
enum {
    LED_PIN            = 5U,
    USER_BUTTON_PIN    = 13U,
    DAC1_OUT1_PIN      = 4U,
    PWM_OUT_PIN        = 6U,
    PWM_PERIOD         = 999U,
    PWM_PRESCALER      = 79U, /* 80 MHz / (79 + 1) / (999 + 1) = 1 kHz */
    BUTTON_DEBOUNCE_MS = 30U,
    BUTTON_HOLD_MS     = 700U,
    UPDATE_PERIOD_MS   = 100U
};

typedef enum {
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_SQUARE,
    WAVE_STEPS,
    WAVE_COUNT,
} WaveMode;

static void init_gpio(void)
{
    setGPIOxClock(GPIOA, STATE_ENABLE);
    setGPIOxClock(GPIOC, STATE_ENABLE);

    /* Onboard LED: push-pull digital output. */
    setPinMode(GPIOA, LED_PIN, PINMODE_DIGITAL_OUTPUT);
    setPinOType(GPIOA, LED_PIN, PINOTYPE_PUSHPULL);
    setPinPull(GPIOA, LED_PIN, PINPULL_NONE);
    setPinSlew(GPIOA, LED_PIN, PINSLEW_LOW);
    digitalWrite(GPIOA, LED_PIN, STATE_DISABLE);

    /* Onboard button: an internal pull-up makes the pressed state low. */
    setPinMode(GPIOC, USER_BUTTON_PIN, PINMODE_DIGITAL_INPUT);
    setPinPull(GPIOC, USER_BUTTON_PIN, PINPULL_UP);

    /* TIM3_CH1 is alternate function 2 on PA6. */
    setPinMode(GPIOA, PWM_OUT_PIN, PINMODE_AF);
    setAlternateFunction(GPIOA, PWM_OUT_PIN, 2U);
    setPinOType(GPIOA, PWM_OUT_PIN, PINOTYPE_PUSHPULL);
    setPinPull(GPIOA, PWM_OUT_PIN, PINPULL_NONE);
    setPinSlew(GPIOA, PWM_OUT_PIN, PINSLEW_HIGH);

    /* DAC outputs must be left in analog mode without a pull resistor. */
    setPinMode(GPIOA, DAC1_OUT1_PIN, PINMODE_ANALOG);
    setPinPull(GPIOA, DAC1_OUT1_PIN, PINPULL_NONE);
}

static void init_outputs(void)
{
    setDACxClock(DAC1, STATE_ENABLE);
    initDAC(DAC1, DACCHANNEL_1);
    dacWrite(DAC1, DACCHANNEL_1, 0U);

    initPWM(TIM3, PWM_CHANNEL_1, PWM_PRESCALER, PWM_PERIOD);
    pwmWrite(TIM3, PWM_CHANNEL_1, 0U);
}

static const char *wave_name(const WaveMode mode)
{
    switch (mode) {
        case WAVE_TRIANGLE:
            return "triangle (4 s)";
        case WAVE_SAWTOOTH:
            return "sawtooth (2 s)";
        case WAVE_SQUARE:
            return "square (1 Hz)";
        case WAVE_STEPS:
            return "eight voltage steps";
        default:
            return "unknown";
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
            return (uint16_t)(phase * DAC_MAX_VALUE / 1999U);

        case WAVE_SQUARE:
            return (elapsed_ms % 1000U < 500U) ? DAC_MAX_VALUE : 0U;

        case WAVE_STEPS:
            return (uint16_t)(((elapsed_ms / 300U) % 8U) * DAC_MAX_VALUE / 7U);

        default:
            return 0U;
    }
}

static void announce_mode(const WaveMode mode)
{
    console_puts("Mode: ");
    console_puts(wave_name(mode));
    console_puts("\r\n");
}

int main(void)
{
    WaveMode mode = WAVE_TRIANGLE;
    EnableState paused = STATE_DISABLE;
    EnableState button_down = STATE_DISABLE;
    EnableState hold_handled = STATE_DISABLE;
    uint32_t waveform_elapsed_ms = 0U;
    uint32_t button_pressed_ms = 0U;

    console_init();
    setClockSpeed(CLOCKSPEED_80MHZ);
    systick_init();

    init_gpio();
    init_outputs();

    console_puts("STM32G474RE driver playground started\r\n");
    console_puts("Short B1 press: next waveform; hold B1: pause/resume\r\n");
    console_puts("PA4 is DAC1_OUT1; PA6 is 1 kHz TIM3_CH1 PWM\r\n");
    announce_mode(mode);

    EnableState raw_button = digitalRead(GPIOC, USER_BUTTON_PIN);
    EnableState stable_button = raw_button;
    uint32_t last_update_ms = millis();
    uint32_t raw_changed_ms = last_update_ms;

    for (;;) {
        const uint32_t now_ms = millis();
        const uint32_t elapsed_ms = now_ms - last_update_ms;
        const EnableState new_raw_button = digitalRead(GPIOC, USER_BUTTON_PIN);

        last_update_ms = now_ms;

        /* Debounce B1 before acting on a press or release. */
        if (new_raw_button != raw_button) {
            raw_button = new_raw_button;
            raw_changed_ms = now_ms;
        }
        if ((raw_button != stable_button) &&
            ((now_ms - raw_changed_ms) >= BUTTON_DEBOUNCE_MS)) {
            stable_button = raw_button;

            if (stable_button == STATE_DISABLE) {
                button_down = STATE_ENABLE;
                hold_handled = STATE_DISABLE;
                button_pressed_ms = now_ms;
            } else if (button_down == STATE_ENABLE) {
                button_down = STATE_DISABLE;
                if (hold_handled == STATE_DISABLE) {
                    mode = (WaveMode)((mode + 1U) % WAVE_COUNT);
                    waveform_elapsed_ms = 0U;
                    announce_mode(mode);
                }
            }
        }

        if ((button_down == STATE_ENABLE) && (hold_handled == STATE_DISABLE) &&
            ((now_ms - button_pressed_ms) >= BUTTON_HOLD_MS)) {
            paused = paused == STATE_ENABLE ? STATE_DISABLE : STATE_ENABLE;
            hold_handled = STATE_ENABLE;
            console_puts(paused == STATE_ENABLE
                ? "Paused (hold B1 again to resume)\r\n"
                : "Running\r\n");
        }

        if (paused == STATE_DISABLE) {
            waveform_elapsed_ms += elapsed_ms;
        }

        uint16_t dac_value = wave_value(mode, waveform_elapsed_ms);
        dacWrite(DAC1, DACCHANNEL_1, dac_value);
        pwmWrite(TIM3, PWM_CHANNEL_1,
                 ((uint32_t)dac_value * PWM_PERIOD) / DAC_MAX_VALUE);

        /* LD2 gives a visible mode number: 1--4 flashes per second. */
        digitalWrite(GPIOA, LED_PIN,
            paused == STATE_ENABLE
                ? STATE_ENABLE
                : ((waveform_elapsed_ms % 1000U) < ((uint32_t)(mode + 1U) * 100U)
                    ? STATE_ENABLE
                    : STATE_DISABLE));

        delay_ms(UPDATE_PERIOD_MS);
    }
}
