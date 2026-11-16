# Testing

## Host unit tests

```sh
pio test -e native            # all suites
pio test -e native -f test_uart
```

Requirements: a host C compiler (`gcc`) with AddressSanitizer and
UndefinedBehaviorSanitizer support (standard on Linux; on macOS use the
Xcode clang `gcc`). PlatformIO downloads Unity and the STM32G4 CMSIS
headers. If the CMSIS package is missing, run `pio run -e nucleo_g474re`
once to install it.

| Suite | Covers |
|---|---|
| `test_gpio_exti` | GPIO field encoding (MODER/PUPDR/OTYPER/OSPEEDR/AFR low/high), clock enabling, argument validation without side effects, BSRR writes, assertions; EXTI routing, edge selection, IRQ mapping, pending flags |
| `test_clock_power` | Flash wait-state table boundaries, PLL limits, the full preset sequence against an emulated RCC (PLL config, wait states, boost, HCLK/2 transition step), PLL-lock failure fallback, prescaler-aware frequency queries, monotonic time across clock changes, PWR voltage range/boost/UCPD |
| `test_uart` | Baud-rate selection (prescaler, 16×/8× oversampling, LPUART), frame-format encodings, kernel clock selection, enable-acknowledge timeout, polled TX/RX, data-bit masking, parity/framing/overrun reporting, read timeouts, line reading, interrupt-mode RX/TX ring buffers, overflow, TX with interrupts masked, TX-only interrupt mode |
| `test_tim_pwm` | PSC/ARR calculation incl. 32-bit timers and limits, instance properties, timer clock doubling, time-base registers, update IRQ, UIFCPY masking, PWM CCMR/CCER encoding, MOE handling, channel validation, duty/compare maths |
| `test_adc_dac` | ADC init sequence, clock divider selection, calibration timeout, shared-clock conflict, channel/sampling registers (SMPR1/SMPR2), conversions and timeouts, internal channels, deinit, VDDA/mV/temperature maths; DAC mode/HFSEL selection, internal DAC3/4, ready timeout, writes and asserts |
| `test_time_console` | Cycle conversion, timeouts (incl. counter wrap and "forever"), SysTick interpolation with pending ticks, clock-change monotonicity, delay fallback, number formatting, output redirection, ITM with/without debugger and stalled FIFO |
| `test_iwdg` | Prescaler/reload selection, never-shorter-than-requested property, range checks, start sequence, update timeout, reset flags |

## How the mocks work

- `test/mock/core_cm4.h` replaces the CMSIS core header. The real
  `stm32g474xx.h` device header is compiled unchanged, so every register
  name, offset and bit mask in the drivers is the real one.
- `test/mock/mock_device.h` is force-included into every file and redirects
  each peripheral macro (`GPIOA`, `RCC`, `USART2`, ...) to a structure in
  host RAM. Driver sources are compiled without modification.
- Every access to `DWT` advances the cycle counter, so timeouts expire
  deterministically, and calls `mock_hw_hook`. Tests install a hook to
  emulate hardware: ready flags after enable bits, PLL lock, clock switch
  status, ADC calibration completing, and so on — or leave flags unset to
  exercise timeout paths.
- `panic()` is replaced by a `longjmp`-based version so `MOCK_PANICS(expr)`
  can assert that an assertion fired.

## What host tests cannot prove

The mocks check that the drivers write the intended values in the intended
order and handle every status path. They do not prove that those values
make the silicon behave as expected: register side effects (write-1-to-clear,
read-to-clear, hardware-set flags) are only as accurate as the hooks, and
analog behaviour, timing and electrical characteristics are untested.
Those require the procedure in [hardware-validation.md](hardware-validation.md).

## Firmware build checks

`pio run` builds the playground and all examples with
`-Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
-Wpointer-arith -Wcast-align -Wwrite-strings -Wconversion` applied to the
project sources. The build is expected to be warning-free.
