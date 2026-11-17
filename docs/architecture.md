# Architecture and conventions

## Layers

```
 application (src/main.c, examples/*/main.c)
        │
 board/nucleo_g474re      pin names and helpers for one board
        │
 drivers/*                one module per peripheral: gpio, exti, uart, tim,
        │                 pwm, adc, dac, iwdg, power
 system/*                 clock tree, time base, console, panic, syscalls
        │
 drivers/internal         clock gating, bounded waits, critical sections
        │
 CMSIS device header      stm32g474xx.h (register definitions only)
```

- **Drivers depend only on CMSIS, `drivers/internal` and `system/*`.**
  `system/clock` uses `drivers/power`; drivers that derive timing from the
  clock (UART, TIM, ADC, DAC) query `system/clock`.
- `drivers/internal/drv_internal.h` is private. It provides:
  - `drv_clock_gate_set/_reset/_is_enabled` – one implementation of RCC
    clock enable / peripheral reset for every driver (enable and reset bits
    share positions on the STM32G4);
  - `drv_wait_bits/_set/_clear` – the only way the library waits on a
    hardware flag, always with a timeout;
  - `drv_reg_modify` – read-modify-write with interrupts masked;
  - `drv_nvic_enable` – priority + clear pending + enable.
- Peripheral instance properties (bus, enable bit, IRQ number, counter
  width, channel count, ...) are looked up from a single table function per
  driver (`tim_info`, `uart_info`, `adc_group`, `dac_info`), not repeated
  in each function.

## Naming

| Kind | Convention | Example |
|---|---|---|
| Functions | `module_verb[_object]`, snake_case | `uart_write_string`, `gpio_init_af` |
| Types | `ModuleThing`, PascalCase | `UartConfig`, `GpioMode`, `DrvStatus` |
| Enum constants / macros | `MODULE_THING`, upper case | `GPIO_PULL_UP`, `ADC_CHANNEL_VREFINT` |
| Peripheral instance parameters | CMSIS pointer | `USART2`, `TIM3`, `GPIOA` |

Pins are passed as `(GPIO_TypeDef *port, uint8_t pin)` with pin 0-15.
Channels use the numbering of the reference manual / datasheet: PWM channel
1-4, DAC channel 1-2, and ADC channel *n* for pin `ADCx_INn`.

## Error handling

- Functions that configure or wait on hardware return `DrvStatus`:
  - `DRV_ERR_PARAM` – an argument is invalid; **no register is modified**.
  - `DRV_ERR_STATE` – the call is not allowed in the current state (for
    example converting with a disabled ADC, or using an uninitialised UART).
  - `DRV_ERR_TIMEOUT` – the hardware did not reach the expected state.
    Initialisation functions return this instead of reporting success when a
    peripheral did not become ready (oscillator, PLL, regulator, ADC
    calibration, DAC ready flag, UART enable acknowledge, IWDG update).
  - `DRV_ERR_UNSUPPORTED` – a valid request the hardware cannot meet
    (unreachable baud rate or timer frequency).
  - `DRV_ERR_PARITY / FRAMING / NOISE / OVERRUN` – UART line errors.
  - `drv_status_str()` turns a status into text.
- Fast-path functions that are called at high rate (`gpio_write`,
  `gpio_read`, `gpio_toggle`, `dac_write`, `pwm_set_compare`,
  `pwm_set_duty_permille`, `exti_take_pending`, `tim_start` ...) return
  nothing and check arguments with `DRV_ASSERT()`. A failed assertion calls
  `panic_at()` with the expression, file and line. Define
  `DRV_ASSERT_ENABLED=0` to compile the checks out.
- `panic()` halts (or, with `PANIC_RESET_ON_FAULT=1` and no debugger,
  resets). It is used only for programming errors, never for hardware
  conditions a caller could handle.

## Timeouts and time

All hardware waits use the DWT cycle counter through `TimeTimeout`, so they
work before `time_init()`, inside interrupt handlers and with interrupts
masked. Elapsed cycles are accumulated in 64 bits, so a timeout keeps
working across the 32-bit counter wrap (25 s at 170 MHz).

The millisecond time base is driven by SysTick (not by CYCCNT), because the
DWT cycle counter stops while the core sleeps in `WFI`. Microseconds are
interpolated from the SysTick down-counter; a tick that is pending while
interrupts are masked is accounted for. When the core clock changes,
`clock_set_preset()` calls `time_clock_changed()`, which reprograms SysTick
and rounds the partial millisecond **up**, so time never runs backwards.

## Concurrency rules

- Configuration functions use `drv_reg_modify()` for shared registers
  (RCC enable registers, GPIO mode registers, NVIC-facing enables), so an
  interrupt handler configuring another field of the same register cannot
  lose its update.
- `gpio_write()` uses BSRR and is atomic. `gpio_toggle()` reads ODR first
  and is not atomic against another context writing the same pin.
- A `UartHandle` may be used for receive in one context and transmit in
  another, but not for transmit (or receive) from two contexts at once.
  Interrupt mode uses single-producer / single-consumer ring buffers that
  need no locking.
- If interrupt-driven UART transmission is used while interrupts are masked
  (for example by `panic()` printing through the UART console), the driver
  drains the queue by polling instead of deadlocking.

## Global state

The library keeps only the state that must be global:

- `system/time`: the millisecond counter (written by `SysTick_Handler`);
- `system/console`: the selected output function and an ITM "stalled" flag.

Driver state that the application owns (UART buffers and error flags) lives
in caller-allocated handles. Interrupt vectors are defined by the
application, which calls the driver's handler with its handle
(`uart_irq_handler(&uart)`, `tim_take_update(TIM6)`, `exti_take_pending(13)`).
The library defines only `SysTick_Handler` and the four fault handlers.

## Limitations

- Clock presets use HSI16 (±1 %). There is no HSE preset yet, although
  `clock_get_sysclk_hz()` understands an HSE or HSE-fed PLL configured by
  application code (`HSE_VALUE`, default 24 MHz as on the Nucleo).
- Peripherals that derive timing from the clock (UART, timers, DAC
  interface mode, ADC clock divider) read it at init; re-initialise them
  after `clock_set_preset()`. UARTs on `UART_CLOCK_HSI16` keep their baud
  rate across clock changes.
- UART: 7 or 8 data bits (frames of 7, 8 or 9 bits including parity);
  9 data bits, 0.5/1.5 stop bits, FIFO mode, DMA, LIN, IrDA, smartcard,
  RS-485 driver enable and auto-baud are not implemented.
- ADC: single software-triggered conversions; no sequences, continuous mode,
  injected channels, differential inputs, oversampling or DMA.
- DAC: software updates only; no triggers, waveform generation, DMA or
  sample-and-hold.
- PWM: edge-aligned mode 1 only; no complementary outputs, dead time, break,
  centre-aligned mode, input capture or encoder mode.
- EXTI: GPIO lines 0-15 only.
- Low-power modes (Stop, Standby), SPI, I2C, FDCAN, USB, DMA, RTC, COMP,
  OPAMP, HRTIM and CORDIC/FMAC are outside the current scope.
