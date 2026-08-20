# stm32g474re-drivers

Bare-metal, register-level peripheral drivers for the STM32G474RE
(Nucleo-G474RE board), built directly on CMSIS with no vendor HAL.

## Implemented

- GPIO — pin mode, pull, output type, slew rate, digital read/write, clock gating
- DAC — channel enable, 12-bit value write
- Timer — RCC clock gating
- Power — voltage scaling, Range 1 boost/normal mode
- Clock — HSI/PLL switching, PLL config, Flash latency, bus prescalers, 16/80/170 MHz presets
- Time — SysTick + DWT-based millisecond/microsecond timekeeping, `delay_ms`/`delay_us`
- Console — ITM (SWO) debug output
- Panic — halt with message, debugger-aware breakpoint

## TODO

- ADC
- PWM (timer channel configuration)
- UART
