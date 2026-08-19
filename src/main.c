#include "stm32g474xx.h"

#include  "system/clock/clock.h"
#include "drivers/dac/dac.h"
#include "drivers/gpio/gpio.h"
#include "system/console/console.h"
#include "system/time/time.h"

/* Nucleo-G474RE: LD2 is connected to PA5. DAC1_OUT1 is available on PA4. */
enum {
    LED_PIN = 5U,
    DAC1_OUT1_PIN = 4U,
    DAC_MID_SCALE = 2048U,
};

int main(void)
{
    console_init();
    setClockSpeed(CLOCKSPEED_80MHZ);
    systick_init();

    /* GPIO output: drive the onboard green LED. */
    setGPIOxClock(GPIOA, STATE_ENABLE);
    setPinMode(GPIOA, LED_PIN, PINMODE_DIGITAL_OUTPUT);
    setPinOType(GPIOA, LED_PIN, PINOTYPE_PUSHPULL);
    setPinPull(GPIOA, LED_PIN, PINPULL_NONE);

    /* DAC output: PA4 is DAC1 channel 1, with a 0--3.3 V output range. */
    setPinMode(GPIOA, DAC1_OUT1_PIN, PINMODE_ANALOG);
    setPinPull(GPIOA, DAC1_OUT1_PIN, PINPULL_NONE);
    setDACxClock(DAC1, STATE_ENABLE);
    initDAC(DAC1, DACCHANNEL_1);
    dacWrite(DAC1, DACCHANNEL_1, DAC_MID_SCALE);

    console_puts("Driver example started\r\n");

    for (;;) {
        digitalWrite(GPIOA, LED_PIN, STATE_ENABLE);
        dacWrite(DAC1, DACCHANNEL_1, 0U);
        delay_ms(1000U);

        digitalWrite(GPIOA, LED_PIN, STATE_DISABLE);
        dacWrite(DAC1, DACCHANNEL_1, DAC_MID_SCALE);
        delay_ms(1000U);
    }
}
