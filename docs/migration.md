# Migrating from the previous API

The first version of the library mixed camelCase names, returned `void`
from functions that wait on hardware, and halted through `panic()` on
invalid arguments. The current API uses module-prefixed snake_case names
and returns `DrvStatus`. Behavioural fixes that come with the new API are
listed after the table.

| Old | New |
|---|---|
| `EnableState` (`STATE_ENABLE`/`STATE_DISABLE`) | `bool` |
| `setClockSpeed(CLOCKSPEED_16MHZ/80MHZ/170MHZ)` | `clock_set_preset(CLOCK_PRESET_HSI_16MHZ/PLL_80MHZ/PLL_170MHZ)` |
| `switchClockSource`, `setSourceClockState(Async)`, `configurePLL`, `resetBusPrescalers`, `flashSetLatency(Async)` | internal to `clock_set_preset()`; see `clock_pll_check()`, `clock_flash_wait_states()` |
| `setRegulatorMode(REGULATORMODE_SMPS)` | `power_set_boost(true)` (the bit is R1MODE boost/normal, not SMPS/LDO) |
| `setRegulatorMode(REGULATORMODE_LDO)` | `power_set_boost(false)` |
| `setVoltageScaling(VOLTAGE_SCALE_RANGE_1)` | `power_set_voltage_range(POWER_RANGE_1)` |
| `systick_init()` | `time_init()` |
| `systick_clock_updated()` | `time_clock_changed()` (called by `clock_set_preset()`) |
| `millis()`, `micros()`, `millis64()`, `micros64()` | `time_millis()`, `time_micros()`, `time_millis64()`, `time_micros64()` |
| `delay_ms()`, `delay_us()` | `time_delay_ms()`, `time_delay_us()` |
| `dwt_cycle_count64()`, `systick_has_cycle_counter()` | `time_cycles()` |
| `setGPIOxClock(port, state)` | `gpio_set_clock(port, enable)` (usually unnecessary: `gpio_init*()` enables it) |
| `setPinMode/setPinPull/setPinOType/setPinSlew/setAlternateFunction` | `gpio_set_mode/pull/output_type/speed/af`, or one `gpio_init*()` call |
| `PINMODE_*`, `PINPULL_*`, `PINOTYPE_*`, `PINSLEW_*` | `GPIO_MODE_*`, `GPIO_PULL_*`, `GPIO_OTYPE_*`, `GPIO_SPEED_*` |
| `digitalWrite(port, pin, state)` / `digitalRead(port, pin)` | `gpio_write(port, pin, bool)` / `gpio_read(port, pin)` |
| `setADCxClock`, `calibrateADC`, `initADC`, `uninitADC` | `adc_init()` (does clock, regulator, calibration, enable), `adc_deinit()` |
| `setADCChannel(adc, ADC_CHANNEL_1, ADC_SAMPLE_47C5)` | `adc_config_channel(adc, 1, ADC_SAMPLE_47_5)` — see the channel fix below |
| `readADC(adc)`, `readADCChannel(...)` | `adc_read(adc, &value, timeout_us)`, `adc_read_channel(..., &value)` |
| `setDACxClock` + `initDAC(dac, DACCHANNEL_1)` | `dac_init(dac, DAC_CHANNEL_1)` |
| `dacWrite(dac, ch, v)` | `dac_write(dac, ch, v)` |
| `setTIMxClock`, `getTIMxClockEnableBit` | `tim_set_clock()` (usually unnecessary) |
| `initPWM(tim, ch, psc, arr)` | `pwm_init(tim, psc, arr)` + `pwm_enable_channel(tim, ch, polarity)`, or `pwm_init_frequency(tim, hz)` |
| `pwmWrite(tim, ch, value)` | `pwm_set_compare(tim, ch, value)` or `pwm_set_duty_permille(tim, ch, permille)` |
| `console_try_putc()` | `console_write(&c, 1)` (returns 0 or 1) |

## Behavioural fixes you may notice

- **ADC channel numbering**: the old `ADC_CHANNEL_1` was encoded as 0, so
  "channel 1" converted `ADCx_IN0`. Channel *n* now selects pin
  `ADCx_INn` as in the datasheet.
- **ADC clock**: the old `initADC()` never selected an ADC clock (the reset
  value of `RCC_CCIPR.ADC12SEL` is "no clock"), so calibration could never
  finish and the function hung. It also called an undefined
  `delayMicroseconds()`.
- **PWM**: `pwmWrite()` generated an update event on every call, restarting
  the counter and distorting the waveform; duty updates are now glitch-free.
  100 % duty is reachable (compare = period). TIM15/16/17 outputs now get
  their MOE bit set.
- **DAC**: the high-frequency interface mode (HFSEL) is now set when HCLK
  exceeds 80 MHz, DAC3/DAC4 use their internal mode, and `dac_init()` waits
  for the channel ready flag.
- **Clock**: 80 MHz now uses 2 Flash wait states (normal mode) instead of 4;
  the 170 MHz switch waits ≥ 1 µs at HCLK/2 before restoring /1, as RM0440
  requires; every wait is bounded.
- **Power**: the reserved `VOS = 0b11` value is rejected.
- **Time**: time no longer jumps backwards after a clock change and keeps
  counting while the core sleeps in `time_delay_ms()`.
- **Console**: ITM output waits (bounded) for the FIFO instead of dropping
  most characters at slow SWO rates.
- **Playground**: the user button B1 is active-high on this board; the old
  demo used a pull-up and treated "low" as pressed, so presses were never
  detected.
