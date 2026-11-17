# Clocks

## Presets

`clock_set_preset()` is the only function needed to configure the clock tree.

| Preset | SYSCLK = HCLK = PCLK1 = PCLK2 | Source | PLL M / N / R | VCO | Voltage | Flash WS | Prefetch |
|---|---:|---|---|---:|---|---:|---|
| `CLOCK_PRESET_HSI_16MHZ` | 16 MHz | HSI16 | off | — | Range 1 normal | 0 | off |
| `CLOCK_PRESET_PLL_80MHZ` | 80 MHz | PLL ← HSI16 | /2, ×20, /2 | 160 MHz | Range 1 normal | 2 | on |
| `CLOCK_PRESET_PLL_170MHZ` | 170 MHz | PLL ← HSI16 | /4, ×85, /2 | 340 MHz | Range 1 **boost** | 4 | on |

After reset the MCU runs at 16 MHz from HSI16 with 0 wait states, so
calling nothing is equivalent to the 16 MHz preset.

## Switching sequence

`clock_set_preset()` follows RM0440 (§6.1.5 dynamic voltage scaling and the
RCC clock-switch rules):

1. Select voltage Range 1 and wait for `VOSF` to clear.
2. Make sure HSI16 runs, switch SYSCLK to it, set all prescalers to /1 and
   stop the PLL. Lowering the frequency first means the current Flash
   latency is always sufficient.
3. If the target is above 80 MHz, set the AHB prescaler to /2.
4. Select boost or normal mode (`PWR_CR5.R1MODE`) and program the Flash
   wait states for the target, reading `FLASH_ACR` back before continuing.
5. Configure the PLL, wait for lock, switch SYSCLK to it and wait for `SWS`.
6. If step 3 applied, wait at least 1 µs, then restore HCLK = SYSCLK.
7. Update `SystemCoreClock` and reprogram SysTick (`time_clock_changed()`).

Every wait has a timeout. If the PLL does not lock or the switch does not
complete, the function returns `DRV_ERR_TIMEOUT` and leaves the device on
HSI16 with prescalers at /1 and the (safe) higher wait-state count.

Flash wait states come from `clock_flash_wait_states()` (RM0440 Table 9):

| HCLK up to (MHz) | 0 WS | 1 WS | 2 WS | 3 WS | 4 WS |
|---|---:|---:|---:|---:|---:|
| Range 1 boost | 34 | 68 | 102 | 136 | 170 |
| Range 1 normal | 30 | 60 | 90 | 120 | 150 |
| Range 2 | 12 | 24 | 26 | — | — |

## Which clock feeds which peripheral

| Peripheral | Clock used by the driver | Notes |
|---|---|---|
| Core, SysTick, DWT | HCLK | `SystemCoreClock` |
| GPIO | HCLK (AHB2) | |
| ADC1-5 | HCLK/2 if HCLK ≤ 120 MHz, else HCLK/4 (synchronous) | max 60 MHz; one setting per group ADC1/2 and ADC3/4/5 |
| DAC1-4 | HCLK (AHB2) | HFSEL set automatically above 80 / 160 MHz |
| USART1 | PCLK2, SYSCLK or HSI16 | `UartConfig.clock_source` |
| USART2/3, UART4/5, LPUART1 | PCLK1, SYSCLK or HSI16 | |
| TIM1/8/15/16/17/20 | APB2 timer clock | PCLK2 ×1 if PPRE2 = /1, else ×2 |
| TIM2-7 | APB1 timer clock | PCLK1 ×1 if PPRE1 = /1, else ×2 |
| IWDG | LSI (~32 kHz) | started by the IWDG itself |
| EXTI / SYSCFG | PCLK2 | |

Query functions: `clock_get_sysclk_hz()`, `clock_get_hclk_hz()`,
`clock_get_pclk1_hz()`, `clock_get_pclk2_hz()`, `clock_get_apb1_timer_hz()`,
`clock_get_apb2_timer_hz()`, and `tim_get_clock_hz(TIMx)`. They read the
RCC registers, so they are correct even if application code changed the
prescalers.

## Changing the clock at run time

Timing that a driver computed at init does not follow a later clock change.
After `clock_set_preset()`:

- the time base (`time_*`) is updated automatically;
- re-run `uart_init()` (unless the UART uses `UART_CLOCK_HSI16`),
  `tim_init*()` / `pwm_init*()`, `dac_init()` (HFSEL) and `adc_init()`
  (clock divider; the ADC group must be disabled to change it).

## Custom clock trees

`clock_pll_check()` validates any PLL setting (VCO input 2.66-16 MHz, VCO
output 96-344 MHz, output ≤ 170 MHz). Application code that programs RCC
itself must set `SystemCoreClock` (or call `SystemCoreClockUpdate()`) and
then call `time_clock_changed()`.
