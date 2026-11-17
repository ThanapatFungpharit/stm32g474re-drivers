# Troubleshooting

### `pio run` fails with "Could not find the package ... toolchain-gccarmnoneeabi"

The default ST platform toolchain (GCC 7) has no build for some hosts (e.g.
Linux on ARM64). `platformio.ini` pins GCC 12.3
(`platformio/toolchain-gccarmnoneeabi@~1.120301.0`), which is published for
Linux x86-64/ARM64, macOS and Windows. If you removed that line, restore it.

### `pio test -e native` cannot find `stm32g474xx.h`

The host tests use the CMSIS headers from the PlatformIO package
`framework-cmsis-stm32g4`. Build the firmware once (`pio run -e
nucleo_g474re`) to install it.

### Nothing on the virtual COM port

- Use the ST-LINK USB port and 115200 8N1 (`pio device monitor -b 115200`).
- Check that the program calls `board_vcp_init()` after `clock_set_preset()`;
  the baud rate is computed from the clock at init.
- If you changed the clock after `uart_init()`, call `uart_init()` again or
  use `UART_CLOCK_HSI16`.
- Some terminals need a newline: the examples print `\r\n`.

### Garbled UART characters

Baud-rate mismatch. `uart_init()` returns `DRV_ERR_UNSUPPORTED` if the
requested rate is more than 2.5 % off; check that both ends use the same
rate, data bits, parity and stop bits. HSI16 is accurate to about ±1 %,
which is enough for standard rates; for long cables or picky receivers use
a crystal-based clock.

### The program stops with `PANIC:` on the console

A driver call returned an error (the examples' `check()` prints the call
and status) or a `DRV_ASSERT` failed (the message shows the expression,
file and line). Typical causes: wrong pin number, a channel the timer does
not have, writing to a DAC channel that was not initialised.

### `clock_set_preset()` returns `DRV_ERR_TIMEOUT`

The PLL did not lock or the regulator did not settle. The MCU keeps running
from HSI16 at 16 MHz. Check the supply (VDD, VDDA) and that no debugger
script reprograms RCC at the same time.

### `adc_init()` returns `DRV_ERR_STATE`

Another ADC of the same group (ADC1/2 or ADC3/4/5) is enabled and the group
has no usable clock setting. Initialise all ADCs of a group through
`adc_init()`, or deinitialise the other one first.

### ADC readings are noisy or wrong

- Put the pin in analog mode (`gpio_init_analog()`), with no pull.
- Use a longer sampling time for high-impedance sources.
- Internal channels need long sampling (`ADC_SAMPLE_640_5`) and
  `adc_enable_internal_channels()` before reading.
- Use `adc_calc_vdda_mv()` instead of assuming exactly 3.3 V.

### The DAC output does not move

- Configure the pin as analog.
- On the Nucleo, PA5 (DAC1_OUT2) is loaded by LD2.
- Call `dac_init()` after `clock_set_preset()` (interface mode depends on HCLK).

### PWM output stays low

- Configure the pin's alternate function for that timer channel (e.g. PA6 is
  TIM3_CH1 on AF2).
- Call `pwm_enable_channel()` after `pwm_init*()`.
- Check that the timer actually has that channel (`tim_get_channel_count()`).

### The button never seems pressed

The NUCLEO-G474RE B1 is active-high: use `GPIO_PULL_DOWN`
(`board_button_init()`) and a rising-edge interrupt. An internal pull-up
makes the pin always read high.

### PB4 or PB6 inputs always read low

Call `power_disable_ucpd_dead_battery()`; after reset these pins have
USB Type-C dead-battery pull-downs.

### No SWO/ITM output

- `console_init()` only enables the ITM if a debugger is attached at that
  moment.
- The SWO viewer must use the real core clock (170 MHz for most examples).
- Alternatively send the console to the UART:
  `console_set_output(uart_console_write, &vcp)`.

### The watchdog resets the board while debugging

Start it with `iwdg_start(timeout, true)` to freeze it while the core is
halted.

### `time_delay_ms()` takes far too long

Interrupts are disabled or the SysTick interrupt is blocked (e.g. by a
`BASEPRI` setting). `time_delay_ms()` falls back to busy waiting when
`PRIMASK` is set or in handler mode, but cannot detect `BASEPRI` masking.
