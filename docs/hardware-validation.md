# Hardware validation

The host tests check register programming against mocks. The following
checks confirm behaviour on a real NUCLEO-G474RE. Record the board revision,
date and results when you run them.

**Equipment:** NUCLEO-G474RE, USB cable, a serial terminal
(`pio device monitor -b 115200`), a multimeter; an oscilloscope or logic
analyser for the timing checks; jumper wires.

Flash each example with `pio run -e example_<name> -t upload`.

## 1. Clock and time base — `blink`

- [ ] LD2 toggles every 500 ms. Over 60 s count 60 on/off cycles
      (stopwatch, ±1 %: HSI16 accuracy).
- [ ] With a scope on PA5: period 1.000 s ± 1 %.
- [ ] Debugger: after `clock_set_preset()` the registers read
      `RCC_CFGR.SWS = PLL`, `FLASH_ACR.LATENCY = 4`, `PWR_CR5.R1MODE = 0`.

## 2. UART polled — `uart_echo`

- [ ] Banner `UART echo ready` appears at 115200 8N1.
- [ ] Typing `hello⏎` returns `you typed "hello" (5 chars)`.
- [ ] Lines longer than 63 characters report `line too long`.
- [ ] Set the terminal to 57600 baud and type: the board reports
      `receive error: DRV_ERR_FRAMING` (or NOISE); restoring 115200 recovers.
- [ ] LD2 toggles once per second while idle (read timeout path).

## 3. UART interrupt mode — `uart_irq`

- [ ] Each typed character is echoed immediately.
- [ ] Paste a 400-character block: it is echoed completely and no
      `[errors ...]` line appears (512-byte buffers cover the 200 ms busy time;
      at 115200 baud about 2.3 KB/s arrive, so 200 ms is ~460 bytes).
- [ ] Temporarily shrink `rx_storage` to 16 bytes and paste a long block:
      `[errors 0x10]` (`UART_ERROR_RX_OVERFLOW`) is reported once per
      second and the board keeps running.

## 4. GPIO interrupt — `button_exti`

- [ ] Each press of B1 prints exactly one `press N` line and toggles LD2.
- [ ] Holding B1 does not produce repeats; releasing does not count.

## 5. Timer interrupt — `timer_irq`

- [ ] `ticks:` increases by exactly 10 per line.
- [ ] LD2 blinks at 1 Hz; on a scope the PA5 period is 1.000 s ± 1 %.

## 6. PWM — `pwm_fade`

- [ ] LD2 fades smoothly in and out over ~2 s, without flicker.
- [ ] Scope on PA5: 1.000 kHz ± 1 %, duty sweeping 0-100 %, no runt or
      missing pulses when the duty changes (persistence mode helps).

## 7. DAC — `dac_wave`

- [ ] Multimeter on PA4 (A2) reads the printed voltages (0, 500, … 3000 mV)
      within ±30 mV (the 0 V step reads slightly above 0: buffer offset).
- [ ] Scope: 100 Hz triangle, ~3.3 V peak.
- [ ] `dac_init()` returned `DRV_OK` (the example would panic otherwise),
      confirming the DAC1RDY flag behaviour at 170 MHz with HFSEL set.

## 8. ADC — `adc_read`

- [ ] `VDDA` reads 3250-3350 mV (Nucleo 3.3 V regulator).
- [ ] A0 (PA0) to GND: < 20 mV. A0 to 3V3: within 1 % of VDDA.
- [ ] A0 to the DAC output of `dac_wave` running on a second board, or to a
      potentiometer divider: readings follow the voltmeter within ±1 %.
- [ ] Temperature: 25-45 °C at room temperature; rises when the chip is
      warmed with a finger.

## 9. Watchdog — `watchdog`

- [ ] Normal operation: no resets for at least 1 minute.
- [ ] Hold B1 for > 1.1 s: the board resets, prints
      `*** Reset by the independent watchdog ***` and LD2 flashes fast.
- [ ] Halting in the debugger for > 1 s does not reset the board
      (`freeze_in_debug = true`).

## 10. Playground — `nucleo_g474re`

- [ ] VCP shows the banner and `Mode: triangle (4 s)`.
- [ ] Short press of B1 cycles the waveform (message printed); holding B1
      pauses/resumes.
- [ ] PA4 shows the selected waveform; PA6 shows 1 kHz PWM whose duty
      follows the DAC level.

## 11. Fault reporting

Temporarily add to any example, after `panic_enable_fault_exceptions()`
and `console_set_output(uart_console_write, &vcp)`:

```c
volatile uint32_t zero = 0U;
volatile uint32_t x = 1U / zero;   /* UsageFault: divide by zero */
(void)x;
```

- [ ] The VCP shows `PANIC: UsageFault`, a PC inside `main`, and
      `CFSR=0x02000000` (DIVBYZERO).
- [ ] With a debugger attached, execution stops at a breakpoint in
      `halt()`.

## 12. ITM console

- [ ] In an SWO viewer (STM32CubeIDE SWV, STM32CubeProgrammer, or your
      debugger's equivalent) set the core clock to the firmware's HCLK and
      enable stimulus port 0: `console_puts()` output appears. Without a
      debugger, the program runs at full speed (output dropped).
