# stm32g474re-drivers

Bare-metal, register-level peripheral drivers for the STM32G474RE
(Nucleo-G474RE board), built directly on CMSIS with no vendor HAL.

## Implemented

- GPIO - pin mode, pull, output type, slew rate, digital read/write, clock gating
- DAC - channel enable, 12-bit value write
- Timer - RCC clock gating
- Power - voltage scaling, Range 1 boost/normal mode
- Clock - HSI/PLL switching, PLL configuration, Flash latency, bus prescalers, 16/80/170 MHz presets
- Time - SysTick + DWT-based millisecond/microsecond timekeeping, `delay_ms`/`delay_us`
- Console - ITM (SWO) debug output
- Panic - halt with message, debugger-aware breakpoint
- ADC - peripheral initialization, calibration, channel selection, sampling time, single software-triggered conversion
- PWM - PWM mode 1, prescaler/period configuration, 4-channel output, duty-cycle control

## TODO

- UART