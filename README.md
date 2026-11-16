# stm32g474re-drivers

Bare-metal, register-level peripheral drivers for the **STM32G474RE**
(NUCLEO-G474RE board), written in C11 directly on the CMSIS device header.
No STM32 HAL or LL code is used.

The library is small (a full example image with clock, time base, UART and
console is ~11 KB of Flash), validates its arguments, never waits on hardware
without a timeout, and is unit-tested on the host against register mocks.

## Supported functionality

| Module | Header | What it does |
|---|---|---|
| Clock | `system/clock/clock.h` | 16 / 80 / 170 MHz presets from HSI16 with correct Flash wait states, Range 1 boost sequencing, PLL validation, and clock-tree frequency queries (SYSCLK, HCLK, PCLK1/2, timer clocks) |
| Time | `system/time/time.h` | 1 kHz SysTick time base (`time_millis`, `time_micros`), sleeping `time_delay_ms`, cycle-accurate `time_delay_us`, and wrap-safe timeouts |
| Console | `system/console/console.h` | Debug text output over ITM/SWO or any sink (e.g. a UART), with number formatting; `printf` support via `syscalls.c` |
| Panic | `system/panic/panic.h` | `panic()` / assertions, and HardFault/MemManage/BusFault/UsageFault handlers that print the fault registers and faulting PC |
| Power | `drivers/power/power.h` | Voltage range, Range 1 boost mode, USB-C dead-battery pull-down disable |
| GPIO | `drivers/gpio/gpio.h` | Pin configuration (mode, pull, type, speed, AF), atomic write/toggle, read |
| EXTI | `drivers/exti/exti.h` | Edge interrupts on GPIO lines 0-15 |
| UART | `drivers/uart/uart.h` | USART1-3, UART4-5, LPUART1: 7/8 data bits, parity, 1/2 stop bits, RTS/CTS, kernel-clock selection, automatic prescaler/oversampling with baud-error check, blocking I/O with timeouts, line-error reporting, interrupt-driven ring buffers |
| Timer | `drivers/tim/tim.h` | Time base for TIM1-8/15-17/20, frequency → PSC/ARR calculation, update interrupts |
| PWM | `drivers/pwm/pwm.h` | Edge-aligned PWM on channels 1-4, frequency or raw setup, polarity, glitch-free duty updates |
| ADC | `drivers/adc/adc.h` | ADC1-5 single conversions with calibration, sampling-time control, internal channels (VREFINT, temperature, VBAT) and mV / °C conversions using factory calibration |
| DAC | `drivers/dac/dac.h` | DAC1-4 12-bit output, automatic high-frequency interface mode, internal-only DAC3/DAC4 |
| IWDG | `drivers/iwdg/iwdg.h` | Independent watchdog with timeout calculation, debug freeze and reset-cause check |
| Board | `board/nucleo_g474re.h` | NUCLEO-G474RE LED, button and ST-LINK virtual COM port helpers |

Not covered (yet): SPI, I2C, CAN-FD, USB, DMA, RTC, comparators, op-amps,
HRTIM, low-power modes, and an HSE-based clock preset. See
[docs/architecture.md](docs/architecture.md#limitations).

## Quick start

Requirements: [PlatformIO Core](https://docs.platformio.org/page/core/installation.html)
(`pip install platformio`), and a NUCLEO-G474RE for running code. PlatformIO
downloads the ARM GCC 12.3 toolchain and CMSIS headers on first build.

```sh
git clone https://github.com/ThanapatFungpharit/stm32g474re-drivers.git
cd stm32g474re-drivers

pio run                                   # build the playground and every example
pio run -e example_uart_echo -t upload    # flash one example over ST-LINK
pio device monitor -b 115200              # open the virtual COM port
pio test -e native                        # host unit tests (needs gcc on the host)
```

A minimal program:

```c
#include "board/nucleo_g474re.h"
#include "system/clock/clock.h"
#include "system/time/time.h"

static UartHandle vcp;

int main(void)
{
    clock_set_preset(CLOCK_PRESET_PLL_170MHZ);
    time_init();
    board_led_init();
    board_vcp_init(&vcp, 115200U);

    for (;;) {
        board_led_toggle();
        uart_write_string(&vcp, "tick\r\n", 100U);
        time_delay_ms(500U);
    }
}
```

Real code should check every `DrvStatus`; the examples show how.

## Repository layout

```
src/
  drivers/          peripheral drivers (one directory per peripheral)
    common.h        DrvStatus, DRV_ASSERT, DRV_WAIT_FOREVER
    internal/       shared helpers (clock gating, bounded waits) - not public API
  system/           clock tree, time base, console, panic, newlib syscalls
  board/            NUCLEO-G474RE pin definitions and helpers
  main.c            "playground" demo: DAC + PWM waveforms, button, VCP
examples/           one small program per peripheral (own PlatformIO env each)
test/               host unit tests and register mocks
docs/               architecture, API guide, pin map, clocks, validation, troubleshooting
```

## Documentation

- [Architecture and conventions](docs/architecture.md) — layering, error handling, concurrency rules, limitations
- [API guide](docs/api.md) — every module, its functions and usage notes
- [Clocks](docs/clocks.md) — presets, clock tree, which peripheral runs from which clock
- [NUCLEO-G474RE pin map](docs/pinout.md) — board pins and alternate functions used by the library
- [Examples](docs/examples.md) — what each example does and how to run it
- [Testing](docs/testing.md) — host tests, mocks and their limits
- [Hardware validation](docs/hardware-validation.md) — step-by-step checks on a real board
- [Troubleshooting](docs/troubleshooting.md)
- [Migrating from the previous API](docs/migration.md)

Every public function is also documented in its header.

## Verification status

- All firmware environments build with GCC 12.3 and `-Wall -Wextra
  -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes` without
  warnings.
- 86 host unit tests pass under AddressSanitizer and UndefinedBehaviorSanitizer.
- The code has **not yet been run on hardware** in this revision; follow
  [docs/hardware-validation.md](docs/hardware-validation.md) to verify a board.

## License

No license has been specified by the repository owner yet.
