# NUCLEO-G474RE pin map

Pins and alternate functions used by the library, board helpers and
examples. The alternate-function numbers come from the STM32G474xx
datasheet (DS12288, "Alternate function" table) and match ST's
STM32G4 HAL/BSP definitions. Always check that table before using a pin
not listed here.

## On-board connections

| Function | Pin | Details |
|---|---|---|
| LD2 (green LED) | PA5 | Active high. Also Arduino D13, DAC1_OUT2 and TIM2_CH1 (AF1) |
| B1 (user button) | PC13 | **Active high**: reads 1 while pressed. Use the internal pull-down (`board_button_init()`), rising edge = press |
| ST-LINK virtual COM port | PA2 (TX), PA3 (RX) | LPUART1, AF12 (`board_vcp_init()`) |
| SWD | PA13 (SWDIO), PA14 (SWCLK) | Do not reconfigure, or the debugger disconnects |
| SWO | PB3 | ITM console output |
| HSE crystal | PF0, PF1 | 24 MHz; not used by the presets |
| USB-C dead-battery pull-downs | PB4, PB6 | Active after reset; call `power_disable_ucpd_dead_battery()` before using these pins |

## Pins used by the examples

| Example | Pin | Function | AF |
|---|---|---|---:|
| blink, playground | PA5 | GPIO output (LD2) | — |
| button_exti, watchdog, playground | PC13 | GPIO input / EXTI13 | — |
| uart_echo, uart_irq, others | PA2 / PA3 | LPUART1_TX / LPUART1_RX | 12 |
| pwm_fade | PA5 | TIM2_CH1 (drives LD2) | 1 |
| playground | PA6 (Arduino D12) | TIM3_CH1 PWM | 2 |
| dac_wave, playground | PA4 (Arduino A2) | DAC1_OUT1 (analog mode) | — |
| adc_read | PA0 (Arduino A0) | ADC1_IN1 (analog mode) | — |

## UART pins (common choices)

| Instance | TX | RX | AF |
|---|---|---|---:|
| USART1 | PA9, PC4 | PA10, PC5 | 7 |
| USART2 | PA2, PB3 | PA3, PB4 | 7 |
| USART3 | PB10, PC10 | PB11, PC11 | 7 |
| UART4 | PC10 | PC11 | 5 |
| UART5 | PC12 | PD2 | 5 |
| LPUART1 | PA2 | PA3 | 12 |

PA2/PA3 are connected to the ST-LINK. To use them for USART2 instead of
the virtual COM port, check the board's solder-bridge configuration in
UM2505.

## Analog pins

Pin `ADCx_INn` is ADC channel `n` (`adc_read_channel(ADCx, n, ...)`).
Arduino analog header on the Nucleo-64 (verify against UM2505 for your
board revision):

| Header | Pin | ADC channels |
|---|---|---|
| A0 | PA0 | ADC1_IN1, ADC2_IN1 |
| A1 | PA1 | ADC1_IN2, ADC2_IN2 |
| A2 | PA4 | ADC2_IN17 (also DAC1_OUT1) |
| A3 | PB0 | ADC1_IN15, ADC3_IN12 |
| A4 | PC1 | ADC1_IN7, ADC2_IN7 |
| A5 | PC0 | ADC1_IN6, ADC2_IN6 |

Internal ADC channels:

| Signal | Channel | ADCs |
|---|---:|---|
| Temperature sensor | 16 | ADC1 |
| Temperature sensor | 4 | ADC5 |
| VBAT / 3 | 17 | ADC1, ADC3, ADC5 |
| VREFINT | 18 | ADC1, ADC3, ADC4, ADC5 |

DAC outputs: DAC1_OUT1 = PA4, DAC1_OUT2 = PA5 (loaded by LD2 on the
Nucleo), DAC2_OUT1 = PA6. DAC3 and DAC4 have no pins.

## Electrical notes

- Pins used for ADC or DAC must be in analog mode without pull resistors
  (`gpio_init_analog()`).
- Use the lowest GPIO speed that works; `GPIO_SPEED_LOW` is fine for UART
  up to about 1 Mbaud and PWM in the kHz range.
- I/O is 3.3 V. Most pins are 5 V tolerant only as digital inputs (FT),
  never in analog mode; check the datasheet pinout table.
