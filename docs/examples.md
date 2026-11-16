# Examples

Each example is a single `main.c` under `examples/<name>/`, built against the
library by its own PlatformIO environment `example_<name>`:

```sh
pio run -e example_blink                 # build
pio run -e example_blink -t upload       # build and flash over ST-LINK
pio device monitor -b 115200             # serial output (where used)
```

`pio run` with no `-e` builds the playground and all examples.

| Example | Peripherals | Board wiring | What you should see |
|---|---|---|---|
| `blink` | clock, time, GPIO | none | LD2 blinks at 1 Hz |
| `button_exti` | EXTI, GPIO, UART | none | Each B1 press toggles LD2 and prints `press N` |
| `uart_echo` | UART (polled), LPUART1 VCP | USB to ST-LINK | Typed lines are echoed back with their length; LD2 toggles each idle second |
| `uart_irq` | UART (interrupt ring buffers) | USB to ST-LINK | Every character echoed, also during the 200 ms busy periods; line errors reported |
| `adc_read` | ADC1, internal channels | optional: signal on A0 (PA0) | VDDA ≈ 3300 mV, A0 voltage, die temperature twice per second |
| `dac_wave` | DAC1 | meter/scope on A2 (PA4) | 0 … 3.0 V in 0.5 V steps, then a 100 Hz triangle |
| `pwm_fade` | TIM2 PWM on PA5 | none | LD2 fades in and out (2 s cycle) |
| `timer_irq` | TIM6 update interrupt | none | LD2 blinks at 1 Hz from the interrupt; tick count +10 per second on the VCP |
| `watchdog` | IWDG | none | Holding B1 > 1 s resets the board; the next boot reports the watchdog reset |

`src/main.c` (environment `nucleo_g474re`) is a larger "playground"
combining DAC, PWM, button handling and VCP output.

All examples run at 170 MHz except the playground (80 MHz), print over the
ST-LINK virtual COM port at 115200 8N1 where they print at all, and stop in
`panic()` with a message on the console if any driver call fails.
