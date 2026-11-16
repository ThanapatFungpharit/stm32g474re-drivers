# API guide

This guide summarises every public function by module. The authoritative
reference, including every parameter and return value, is the Doxygen
comment in each header. See [architecture.md](architecture.md) for the
error-handling and naming conventions that apply throughout.

`DrvStatus` values: `DRV_OK`, `DRV_ERR_PARAM`, `DRV_ERR_STATE`,
`DRV_ERR_TIMEOUT`, `DRV_ERR_BUSY`, `DRV_ERR_UNSUPPORTED`, `DRV_ERR_PARITY`,
`DRV_ERR_FRAMING`, `DRV_ERR_NOISE`, `DRV_ERR_OVERRUN`.

---

## Clock — `system/clock/clock.h`

| Function | Purpose |
|---|---|
| `DrvStatus clock_set_preset(ClockPreset)` | Switch to `CLOCK_PRESET_HSI_16MHZ`, `CLOCK_PRESET_PLL_80MHZ` or `CLOCK_PRESET_PLL_170MHZ`. Returns `DRV_ERR_TIMEOUT` (and stays on HSI16) if the PLL does not lock. |
| `uint32_t clock_get_sysclk_hz/hclk_hz/pclk1_hz/pclk2_hz(void)` | Current bus frequencies, computed from RCC registers. |
| `uint32_t clock_get_apb1_timer_hz/apb2_timer_hz(void)` | Timer kernel clocks (×2 when the APB prescaler is not /1). |
| `int32_t clock_flash_wait_states(hz, range, boost)` | Minimum Flash latency for a frequency, or −1 if not allowed. |
| `DrvStatus clock_pll_check(in_hz, m, n, r, &out_hz)` | Validate PLL dividers and frequency limits. |

Details: [clocks.md](clocks.md).

## Time — `system/time/time.h`

| Function | Purpose |
|---|---|
| `void time_init(void)` | Start the 1 kHz SysTick time base (call after setting the clock). |
| `bool time_is_initialized(void)` | |
| `void time_clock_changed(void)` | Re-program SysTick after a clock change (called by `clock_set_preset`). |
| `uint32_t time_millis(void)` / `uint64_t time_millis64(void)` | Milliseconds since `time_init()`. |
| `uint32_t time_micros(void)` / `uint64_t time_micros64(void)` | Microseconds since `time_init()`. |
| `uint32_t time_cycles(void)` | Raw DWT cycle counter. |
| `void time_delay_ms(uint32_t)` | Delay; sleeps with `WFI` in thread mode, busy-waits elsewhere. |
| `void time_delay_us(uint32_t)` | Cycle-counted busy wait; works before `time_init()`. |
| `time_timeout_start_us/ms(&t, n)`, `bool time_timeout_expired(&t)` | Polling timeouts; `UINT32_MAX` (= `DRV_WAIT_FOREVER`) never expires. |
| `uint64_t time_us_to_cycles(us, hz)` | Conversion helper (rounds up). |

Limitations: `time_millis()` wraps after 49.7 days, `time_micros()` after
71.6 minutes; use the 64-bit variants for long intervals. Time stops
advancing if interrupts stay masked for more than 1 ms.

## Console — `system/console/console.h`

| Function | Purpose |
|---|---|
| `void console_init(void)` | Enable ITM stimulus port 0 if a debugger is attached. |
| `void console_set_output(fn, ctx)` | Redirect output, e.g. `console_set_output(uart_console_write, &uart)`; `NULL` restores ITM. |
| `console_putc`, `console_puts`, `console_write` | Text output. Best effort: drops output if nobody listens. |
| `console_put_dec`, `console_put_udec`, `console_put_hex(value, digits)` | Numbers without `printf`. |
| `console_format_udec`, `console_format_hex` | Format into a caller buffer. |
| `bool console_is_ready(void)` | |

`printf()` also works: `system/syscalls/syscalls.c` sends stdout/stderr to
the console (costs several KB of newlib code).

## Panic — `system/panic/panic.h`

| Function | Purpose |
|---|---|
| `_Noreturn void panic(const char *)` | Print and halt (or reset with `PANIC_RESET_ON_FAULT=1`). |
| `_Noreturn void panic_at(msg, file, line)` | Same, with a source location (used by `DRV_ASSERT`). |
| `void panic_enable_fault_exceptions(void)` | Enable MemManage/BusFault/UsageFault and divide-by-zero trapping. |

Fault handlers print `PC`, `LR`, `xPSR`, `EXC_RETURN`, `CFSR`, `HFSR` and,
when valid, `MMFAR`/`BFAR`. Define `PANIC_NO_FAULT_HANDLERS=1` to supply
your own.

## Power — `drivers/power/power.h`

| Function | Purpose |
|---|---|
| `void power_enable_clock(void)` | Enable the PWR bus clock. |
| `DrvStatus power_set_voltage_range(PowerVoltageRange)` | `POWER_RANGE_1` (≤ 170 MHz) or `POWER_RANGE_2` (≤ 26 MHz); waits for `VOSF`. |
| `PowerVoltageRange power_get_voltage_range(void)` | |
| `void power_set_boost(bool)` / `bool power_is_boost_enabled(void)` | Range 1 boost mode (needed above 150 MHz). |
| `void power_disable_ucpd_dead_battery(void)` | Remove the reset-time pull-downs on PB4/PB6. |

## GPIO — `drivers/gpio/gpio.h`

| Function | Purpose |
|---|---|
| `DrvStatus gpio_init(port, pin, const GpioConfig *)` | Full configuration; enables the port clock. |
| `gpio_init_output(port, pin, level)` | Push-pull output, preset level (no glitch). |
| `gpio_init_input(port, pin, pull)` | Digital input. |
| `gpio_init_analog(port, pin)` | Analog mode for ADC/DAC. |
| `gpio_init_af(port, pin, af, otype, pull, speed)` | Alternate function. |
| `gpio_set_mode/pull/output_type/speed/af(...)` | Change one field. |
| `DrvStatus gpio_set_clock(port, enable)` | Port clock gating. |
| `void gpio_write(port, pin, bool)`, `void gpio_toggle(port, pin)`, `bool gpio_read(port, pin)` | Fast path (assert on bad pin). |
| `int32_t gpio_port_index(port)` | GPIOA = 0 … GPIOG = 6, −1 if invalid. |

## EXTI — `drivers/exti/exti.h`

| Function | Purpose |
|---|---|
| `DrvStatus exti_enable_gpio(port, pin, trigger, priority)` | Route pin → EXTI line, select edges, unmask, enable NVIC. |
| `DrvStatus exti_disable(line)` | Mask the line. |
| `bool exti_take_pending(line)` | Check-and-clear inside the IRQ handler. |
| `IRQn_Type exti_irqn(line)` | `EXTI0_IRQn` … `EXTI15_10_IRQn`. |

## UART — `drivers/uart/uart.h`

| Function | Purpose |
|---|---|
| `UartConfig uart_default_config(baud)` | 8N1, no flow control, PCLK kernel clock. |
| `DrvStatus uart_init(&h, instance, &config)` | Reset + configure + enable; `DRV_ERR_UNSUPPORTED` if the baud rate is not reachable within 2.5 %. |
| `DrvStatus uart_deinit(&h)` | Disable, reset, gate clock. |
| `DrvStatus uart_enable_irq(&h, rx_buf, rx_size, tx_buf, tx_size, priority)` | Interrupt-driven RX and/or TX via ring buffers. |
| `void uart_irq_handler(&h)` | Call from `USARTx_IRQHandler` / `LPUART1_IRQHandler`. |
| `uart_write(&h, data, len, timeout_ms)`, `uart_write_string`, `uart_write_byte` | Transmit. |
| `DrvStatus uart_flush(&h, timeout_ms)` | Wait until the last stop bit is sent. |
| `uart_read(&h, buf, len, &received, timeout_ms)`, `uart_read_byte` | Receive; timeout covers the whole call. |
| `DrvStatus uart_read_line(&h, buf, size, timeout_ms)` | Line terminated by CR or LF. |
| `size_t uart_rx_available(&h)` | |
| `uint32_t uart_take_errors(&h)` | `UART_ERROR_PARITY/FRAMING/NOISE/OVERRUN/RX_OVERFLOW` since the last call. |
| `DrvStatus uart_calc_baud(kernel_hz, baud, lpuart, &setting)` | Register values and error for a baud rate. |
| `size_t uart_console_write(ctx, data, len)` | Adapter for `console_set_output()`. |

Error behaviour: in polled mode a line error ends `uart_read()` with the
matching status (an overrun still returns the valid byte); in interrupt
mode corrupted bytes are dropped and only recorded in the error flags.

## Timer — `drivers/tim/tim.h`

| Function | Purpose |
|---|---|
| `DrvStatus tim_init(tim, psc, arr)` / `tim_init_frequency(tim, hz)` | Up-counting time base, stopped. |
| `tim_start`, `tim_stop`, `tim_get_counter`, `tim_set_counter` | |
| `DrvStatus tim_enable_update_irq(tim, priority)` / `tim_disable_update_irq(tim)` | Update interrupt. |
| `bool tim_take_update(tim)` | Check-and-clear inside the IRQ handler. |
| `IRQn_Type tim_update_irqn(tim)` | Vector carrying the update interrupt. |
| `DrvStatus tim_calc_period(clock_hz, hz, max_arr, &psc, &arr)` | Finest-resolution PSC/ARR for a frequency. |
| `tim_get_clock_hz`, `tim_get_max_arr`, `tim_get_channel_count`, `tim_has_main_output`, `tim_is_valid` | Instance properties. |
| `DrvStatus tim_set_clock(tim, enable)`, `DrvStatus tim_deinit(tim)` | |

## PWM — `drivers/pwm/pwm.h`

| Function | Purpose |
|---|---|
| `DrvStatus pwm_init(tim, psc, arr)` / `pwm_init_frequency(tim, hz)` | Configure and start the timer (sets MOE where needed). |
| `DrvStatus pwm_enable_channel(tim, channel, polarity)` / `pwm_disable_channel` | PWM mode 1 with preload, starts at 0 %. |
| `void pwm_set_duty_permille(tim, channel, 0..1000)` | Duty in 0.1 % steps; 1000 = always on. |
| `void pwm_set_compare(tim, channel, ticks)` | Raw compare value, 0 … `pwm_get_period_ticks()`. |
| `uint32_t pwm_get_period_ticks(tim)`, `uint32_t pwm_duty_to_compare(period, permille)` | |

## ADC — `drivers/adc/adc.h`

| Function | Purpose |
|---|---|
| `DrvStatus adc_init(adc)` | Clock, regulator, calibration, enable. |
| `DrvStatus adc_deinit(adc)` | Disable, deep power-down. |
| `DrvStatus adc_config_channel(adc, channel, sample_time)` | Select channel 0-18 and sampling time. |
| `DrvStatus adc_read(adc, &value, timeout_us)` | One conversion of the selected channel. |
| `DrvStatus adc_read_channel(adc, channel, sample_time, &value)` | Both of the above. |
| `DrvStatus adc_enable_internal_channels(adc, vrefint, temp, vbat)` | Connect internal sources. |
| `adc_calc_vdda_mv`, `adc_calc_mv`, `adc_calc_temperature_centi_c` | Conversions. |
| `adc_vrefint_cal`, `adc_ts_cal1`, `adc_ts_cal2` | Factory calibration values. |
| `bool adc_is_enabled(adc)` | |

## DAC — `drivers/dac/dac.h`

| Function | Purpose |
|---|---|
| `DrvStatus dac_init(dac, channel)` | Enable a channel (buffered pin for DAC1/2, internal for DAC3/4) and wait for ready. |
| `DrvStatus dac_deinit(dac, channel)` | |
| `void dac_write(dac, channel, 0..4095)` | Output code. |
| `uint16_t dac_mv_to_code(mv, vref_mv)` | Conversion with clamping. |

## IWDG — `drivers/iwdg/iwdg.h`

| Function | Purpose |
|---|---|
| `DrvStatus iwdg_start(timeout_ms, freeze_in_debug)` | Start (cannot be stopped). 1 … 32768 ms. |
| `void iwdg_refresh(void)` | Reload the counter. |
| `bool iwdg_caused_reset(void)`, `void iwdg_clear_reset_flags(void)` | Reset cause. |
| `DrvStatus iwdg_calc(timeout_ms, lsi_hz, &setting)` | Prescaler/reload computation. |

## Board — `board/nucleo_g474re.h`

`board_led_init/set/toggle`, `board_button_init/pressed`,
`board_vcp_init(&uart, baud)` plus `BOARD_*` pin constants.
