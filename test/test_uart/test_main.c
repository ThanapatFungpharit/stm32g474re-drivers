/* Host tests: UART baud-rate calculation, register setup, transfers, errors. */
#include <string.h>
#include <unity.h>

#include "drivers/uart/uart.h"

static USART_TypeDef *hw_uart;
static bool hw_ack;
static bool hw_txe;

/* Emulates the USART: acknowledges TE/RE and keeps the transmitter ready. */
static void uart_hardware(void)
{
    if (hw_uart == NULL) {
        return;
    }
    if (hw_ack) {
        if ((hw_uart->CR1 & USART_CR1_TE) != 0U) {
            hw_uart->ISR |= USART_ISR_TEACK;
        }
        if ((hw_uart->CR1 & USART_CR1_RE) != 0U) {
            hw_uart->ISR |= USART_ISR_REACK;
        }
    }
    if (hw_txe) {
        hw_uart->ISR |= USART_ISR_TXE_TXFNF | USART_ISR_TC;
    }
}

static UartHandle uart;

void setUp(void)
{
    mock_reset();
    memset(&uart, 0, sizeof uart);
    hw_uart = &mock_USART2;
    hw_ack = true;
    hw_txe = true;
    mock_hw_hook = uart_hardware;
}

void tearDown(void) {}

static void init_usart2_8n1(void)
{
    const UartConfig config = uart_default_config(115200U);
    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, USART2, &config));
}

/* Deliver one received byte through the interrupt handler. */
static void irq_receive(const uint8_t byte, const uint32_t extra_flags)
{
    hw_uart->RDR = byte;
    hw_uart->ISR = (hw_uart->ISR & ~(USART_ISR_TXE_TXFNF)) | USART_ISR_RXNE_RXFNE | extra_flags;
    uart_irq_handler(&uart);
    hw_uart->ISR &= ~(USART_ISR_RXNE_RXFNE | extra_flags);
}

/* --- Baud-rate calculation ------------------------------------------------- */

static void test_baud_usart_115200_at_170mhz_oversampling_16(void)
{
    UartBaudSetting s;
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(170000000U, 115200U, false, &s));
    TEST_ASSERT_FALSE(s.over8);
    TEST_ASSERT_EQUAL_UINT32(0U, s.presc);
    TEST_ASSERT_EQUAL_UINT32(1476U, s.brr);
    TEST_ASSERT_UINT32_WITHIN(1000U, 0U, s.error_ppm);
}

static void test_baud_lpuart_115200_at_170mhz(void)
{
    UartBaudSetting s;
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(170000000U, 115200U, true, &s));
    TEST_ASSERT_EQUAL_UINT32(0U, s.presc);
    TEST_ASSERT_EQUAL_UINT32(377778U, s.brr);
    TEST_ASSERT_FALSE(s.over8);
}

static void test_baud_lpuart_9600_needs_prescaler(void)
{
    UartBaudSetting s;
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(170000000U, 9600U, true, &s));
    TEST_ASSERT_EQUAL_UINT32(3U, s.presc);          /* /6 */
    TEST_ASSERT_EQUAL_UINT32(755556U, s.brr);
    TEST_ASSERT_TRUE(s.brr <= 0xFFFFFU);
}

static void test_baud_usart_low_rate_needs_prescaler(void)
{
    UartBaudSetting s;
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(170000000U, 300U, false, &s));
    TEST_ASSERT_EQUAL_UINT32(5U, s.presc);          /* /10 */
    TEST_ASSERT_EQUAL_UINT32(56667U, s.brr);
}

static void test_baud_high_rates_use_oversampling_8(void)
{
    UartBaudSetting s;

    /* 2 Mbaud at 16 MHz: impossible with 16x, exact with 8x. */
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(16000000U, 2000000U, false, &s));
    TEST_ASSERT_TRUE(s.over8);
    TEST_ASSERT_EQUAL_HEX32(0x10U, s.brr);
    TEST_ASSERT_EQUAL_UINT32(2000000U, s.actual_baud);

    /* 921600 at 16 MHz: 16x is 2.1 % off, 8x 0.8 %: 8x wins.
     * USARTDIV = 35 -> BRR = 0x20 | (0x3 >> 1). */
    TEST_ASSERT_EQUAL(DRV_OK, uart_calc_baud(16000000U, 921600U, false, &s));
    TEST_ASSERT_TRUE(s.over8);
    TEST_ASSERT_EQUAL_HEX32(0x21U, s.brr);
    TEST_ASSERT_TRUE(s.error_ppm < 10000U);
}

static void test_baud_rejects_unreachable_and_bad_input(void)
{
    UartBaudSetting s;
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, uart_calc_baud(16000000U, 3000000U, false, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, uart_calc_baud(16000000U, 6000000U, true, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_calc_baud(0U, 9600U, false, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_calc_baud(16000000U, 0U, false, &s));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_calc_baud(16000000U, 9600U, false, NULL));
}

/* --- Initialisation ---------------------------------------------------------- */

static void test_init_8n1_registers(void)
{
    init_usart2_8n1();

    TEST_ASSERT_BITS(RCC_APB1ENR1_USART2EN, RCC_APB1ENR1_USART2EN, mock_RCC.APB1ENR1);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.CCIPR & RCC_CCIPR_USART2SEL);
    TEST_ASSERT_EQUAL_HEX32(USART_CR1_UE | USART_CR1_TE | USART_CR1_RE, mock_USART2.CR1);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_USART2.CR2);
    TEST_ASSERT_EQUAL_HEX32(0U, mock_USART2.CR3);
    TEST_ASSERT_EQUAL_UINT32(139U, mock_USART2.BRR); /* 16 MHz / 115200 */
    TEST_ASSERT_EQUAL_UINT32(0U, mock_USART2.PRESC);
}

static void test_init_frame_formats(void)
{
    UartConfig c = uart_default_config(9600U);

    c.data_bits = UART_DATA_BITS_7;
    c.parity = UART_PARITY_EVEN; /* 8-bit frame */
    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, USART2, &c));
    TEST_ASSERT_EQUAL_HEX32(USART_CR1_PCE, mock_USART2.CR1 &
                            (USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE | USART_CR1_PS));
    TEST_ASSERT_EQUAL_HEX8(0x7FU, uart.data_mask);

    c.data_bits = UART_DATA_BITS_8;
    c.parity = UART_PARITY_ODD; /* 9-bit frame */
    c.stop_bits = UART_STOP_BITS_2;
    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, USART2, &c));
    TEST_ASSERT_EQUAL_HEX32(USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS, mock_USART2.CR1 &
                            (USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE | USART_CR1_PS));
    TEST_ASSERT_EQUAL_HEX32(USART_CR2_STOP_1, mock_USART2.CR2 & USART_CR2_STOP);

    c.data_bits = UART_DATA_BITS_7;
    c.parity = UART_PARITY_NONE; /* 7-bit frame */
    c.stop_bits = UART_STOP_BITS_1;
    c.flow_control = UART_FLOW_RTS_CTS;
    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, USART2, &c));
    TEST_ASSERT_EQUAL_HEX32(USART_CR1_M1, mock_USART2.CR1 &
                            (USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE));
    TEST_ASSERT_EQUAL_HEX32(USART_CR3_RTSE | USART_CR3_CTSE, mock_USART2.CR3);
}

static void test_init_lpuart_with_hsi16_kernel_clock(void)
{
    hw_uart = &mock_LPUART1;
    UartConfig c = uart_default_config(115200U);
    c.clock_source = UART_CLOCK_HSI16;

    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, LPUART1, &c));
    TEST_ASSERT_EQUAL_HEX32(2UL << RCC_CCIPR_LPUART1SEL_Pos, mock_RCC.CCIPR & RCC_CCIPR_LPUART1SEL);
    TEST_ASSERT_BITS(RCC_APB1ENR2_LPUART1EN, RCC_APB1ENR2_LPUART1EN, mock_RCC.APB1ENR2);
    TEST_ASSERT_EQUAL_UINT32(35556U, mock_LPUART1.BRR); /* 256 * 16 MHz / 115200 */
}

static void test_init_rejects_invalid_configuration(void)
{
    UartConfig c = uart_default_config(115200U);

    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_init(&uart, (USART_TypeDef *)&mock_TIM1, &c));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_init(NULL, USART2, &c));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_init(&uart, USART2, NULL));
    c.data_bits = (UartDataBits)9;
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_init(&uart, USART2, &c));
    c = uart_default_config(0U);
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_init(&uart, USART2, &c));
    c = uart_default_config(5000000U); /* 16 MHz cannot make 5 Mbaud */
    TEST_ASSERT_EQUAL(DRV_ERR_UNSUPPORTED, uart_init(&uart, USART2, &c));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_USART2.CR1); /* nothing was touched */
}

static void test_init_timeout_when_peripheral_does_not_acknowledge(void)
{
    hw_ack = false;
    const UartConfig c = uart_default_config(115200U);

    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_init(&uart, USART2, &c));
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, uart_write_byte(&uart, 'x', 10U));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.APB1ENR1 & RCC_APB1ENR1_USART2EN);
}

/* --- Polled transfers --------------------------------------------------------- */

static void test_polled_write(void)
{
    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_write_string(&uart, "hi", 10U));
    TEST_ASSERT_EQUAL_UINT32('i', mock_USART2.TDR);
    TEST_ASSERT_EQUAL(DRV_OK, uart_flush(&uart, 10U));
}

static void test_polled_write_times_out_when_transmitter_stalls(void)
{
    init_usart2_8n1();
    hw_txe = false;
    mock_USART2.ISR &= ~(USART_ISR_TXE_TXFNF | USART_ISR_TC);

    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_write_byte(&uart, 'x', 2U));
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_flush(&uart, 2U));
}

static void test_polled_read_masks_to_data_bits(void)
{
    UartConfig c = uart_default_config(9600U);
    c.data_bits = UART_DATA_BITS_7;
    c.parity = UART_PARITY_EVEN;
    TEST_ASSERT_EQUAL(DRV_OK, uart_init(&uart, USART2, &c));

    mock_USART2.RDR = 0xC1U; /* parity bit in bit 7 */
    mock_USART2.ISR |= USART_ISR_RXNE_RXFNE;

    uint8_t byte = 0U;
    TEST_ASSERT_EQUAL(DRV_OK, uart_read_byte(&uart, &byte, 10U));
    TEST_ASSERT_EQUAL_HEX8(0x41U, byte);
}

static void test_polled_read_reports_line_errors(void)
{
    init_usart2_8n1();
    uint8_t buf[4];
    size_t got = 99U;

    mock_USART2.ISR |= USART_ISR_RXNE_RXFNE | USART_ISR_FE;
    TEST_ASSERT_EQUAL(DRV_ERR_FRAMING, uart_read(&uart, buf, sizeof buf, &got, 10U));
    TEST_ASSERT_EQUAL_UINT32(0U, got);
    TEST_ASSERT_BITS(USART_ICR_FECF, USART_ICR_FECF, mock_USART2.ICR);
    TEST_ASSERT_EQUAL_HEX32(UART_ERROR_FRAMING, uart_take_errors(&uart));
    TEST_ASSERT_EQUAL_HEX32(0U, uart_take_errors(&uart));

    mock_USART2.ISR = (mock_USART2.ISR & ~USART_ISR_FE) | USART_ISR_PE;
    TEST_ASSERT_EQUAL(DRV_ERR_PARITY, uart_read_byte(&uart, buf, 10U));

    /* Overrun: the byte in RDR is still valid and is returned. */
    mock_USART2.ISR = (mock_USART2.ISR & ~USART_ISR_PE) | USART_ISR_ORE;
    mock_USART2.RDR = 'z';
    TEST_ASSERT_EQUAL(DRV_ERR_OVERRUN, uart_read(&uart, buf, 1U, &got, 10U));
    TEST_ASSERT_EQUAL_UINT32(1U, got);
    TEST_ASSERT_EQUAL_UINT8('z', buf[0]);
    TEST_ASSERT_EQUAL_HEX32(UART_ERROR_PARITY | UART_ERROR_OVERRUN, uart_take_errors(&uart));
}

static void test_polled_read_timeout(void)
{
    init_usart2_8n1();
    uint8_t buf[2];
    size_t got = 99U;

    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_read(&uart, buf, sizeof buf, &got, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, got);
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_read(&uart, buf, sizeof buf, &got, 5U));
    TEST_ASSERT_EQUAL_UINT32(0U, uart_rx_available(&uart));
}

static void test_read_line_buffer_full(void)
{
    init_usart2_8n1();
    char line[4];

    mock_USART2.RDR = 'a';
    mock_USART2.ISR |= USART_ISR_RXNE_RXFNE;
    TEST_ASSERT_EQUAL(DRV_ERR_BUSY, uart_read_line(&uart, line, sizeof line, 10U));
    TEST_ASSERT_EQUAL_STRING("aaa", line);
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_read_line(&uart, line, 1U, 10U));
}

static void test_argument_checks(void)
{
    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_write(&uart, NULL, 3U, 10U));
    TEST_ASSERT_EQUAL(DRV_OK, uart_write(&uart, NULL, 0U, 10U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_read(&uart, NULL, 3U, NULL, 10U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_write_string(&uart, NULL, 10U));
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, uart_write_byte(NULL, 'x', 10U));
}

/* --- Interrupt mode ------------------------------------------------------------ */

static void test_irq_receive_into_ring_and_read_line(void)
{
    uint8_t rx[16];
    char line[16];

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_enable_irq(&uart, rx, sizeof rx, NULL, 0U, 6U));
    TEST_ASSERT_BITS(USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE,
                     USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE, mock_USART2.CR1);
    TEST_ASSERT_BITS(USART_CR3_EIE, USART_CR3_EIE, mock_USART2.CR3);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_nvic_enabled[USART2_IRQn]);
    TEST_ASSERT_EQUAL_UINT32(6U, mock_nvic_priority[USART2_IRQn]);

    irq_receive('o', 0U);
    irq_receive('k', 0U);
    irq_receive('!', USART_ISR_FE); /* corrupted: dropped and recorded */
    irq_receive('\r', 0U);
    TEST_ASSERT_EQUAL_UINT32(3U, uart_rx_available(&uart));

    TEST_ASSERT_EQUAL(DRV_OK, uart_read_line(&uart, line, sizeof line, 10U));
    TEST_ASSERT_EQUAL_STRING("ok", line);
    TEST_ASSERT_EQUAL_HEX32(UART_ERROR_FRAMING, uart_take_errors(&uart));
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_read_line(&uart, line, sizeof line, 0U));
}

static void test_irq_receive_overflow_is_reported(void)
{
    uint8_t rx[4]; /* holds 3 bytes */
    uint8_t out[8];
    size_t got = 0U;

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_enable_irq(&uart, rx, sizeof rx, NULL, 0U, 0U));
    for (uint8_t i = 0U; i < 5U; ++i) {
        irq_receive((uint8_t)('0' + i), 0U);
    }

    TEST_ASSERT_EQUAL_UINT32(3U, uart_rx_available(&uart));
    TEST_ASSERT_EQUAL(DRV_ERR_TIMEOUT, uart_read(&uart, out, sizeof out, &got, 0U));
    TEST_ASSERT_EQUAL_UINT32(3U, got);
    TEST_ASSERT_EQUAL_MEMORY("012", out, 3U);
    TEST_ASSERT_EQUAL_HEX32(UART_ERROR_RX_OVERFLOW, uart_take_errors(&uart));
}

static void test_irq_transmit_drains_through_handler(void)
{
    uint8_t tx[8];

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_enable_irq(&uart, NULL, 0U, tx, sizeof tx, 3U));
    TEST_ASSERT_EQUAL(DRV_OK, uart_write(&uart, "abc", 3U, 10U));
    TEST_ASSERT_BITS(USART_CR1_TXEIE_TXFNFIE, USART_CR1_TXEIE_TXFNFIE, mock_USART2.CR1);

    mock_USART2.ISR |= USART_ISR_TXE_TXFNF;
    uart_irq_handler(&uart);
    TEST_ASSERT_EQUAL_UINT32('a', mock_USART2.TDR);
    uart_irq_handler(&uart);
    uart_irq_handler(&uart);
    TEST_ASSERT_EQUAL_UINT32('c', mock_USART2.TDR);
    TEST_ASSERT_BITS(USART_CR1_TXEIE_TXFNFIE, USART_CR1_TXEIE_TXFNFIE, mock_USART2.CR1);
    uart_irq_handler(&uart); /* queue empty: interrupt disabled */
    TEST_ASSERT_EQUAL_HEX32(0U, mock_USART2.CR1 & USART_CR1_TXEIE_TXFNFIE);
    TEST_ASSERT_EQUAL(DRV_OK, uart_flush(&uart, 10U));
}

static void test_irq_transmit_with_interrupts_masked_does_not_deadlock(void)
{
    uint8_t tx[4]; /* holds 3 bytes */

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_enable_irq(&uart, NULL, 0U, tx, sizeof tx, 3U));

    mock_primask = 1U; /* e.g. inside panic() */
    TEST_ASSERT_EQUAL(DRV_OK, uart_write_string(&uart, "0123456789", 10U));
    TEST_ASSERT_EQUAL(DRV_OK, uart_flush(&uart, 10U));
    TEST_ASSERT_EQUAL_UINT32('9', mock_USART2.TDR);
    TEST_ASSERT_EQUAL_UINT32(9U, uart_console_write(&uart, "console..", 9U));
    mock_primask = 0U;
}

static void test_tx_only_irq_mode_leaves_received_data_to_polled_reader(void)
{
    uint8_t tx[8];
    uint8_t byte = 0U;

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_enable_irq(&uart, NULL, 0U, tx, sizeof tx, 3U));
    mock_USART2.RDR = 'q';
    mock_USART2.ISR |= USART_ISR_RXNE_RXFNE;
    uart_irq_handler(&uart);
    TEST_ASSERT_EQUAL(DRV_OK, uart_read_byte(&uart, &byte, 10U));
    TEST_ASSERT_EQUAL_UINT8('q', byte);
}

static void test_enable_irq_argument_checks(void)
{
    uint8_t buf[8];
    UartHandle idle;
    memset(&idle, 0, sizeof idle);

    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_enable_irq(&uart, NULL, 0U, NULL, 0U, 0U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_enable_irq(&uart, buf, 1U, NULL, 0U, 0U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_enable_irq(&uart, buf, sizeof buf, NULL, 0U, 16U));
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, uart_enable_irq(&idle, buf, sizeof buf, NULL, 0U, 0U));
}

static void test_deinit(void)
{
    init_usart2_8n1();
    TEST_ASSERT_EQUAL(DRV_OK, uart_deinit(&uart));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_RCC.APB1ENR1 & RCC_APB1ENR1_USART2EN);
    TEST_ASSERT_EQUAL(DRV_ERR_STATE, uart_write_byte(&uart, 'x', 1U));
    TEST_ASSERT_EQUAL(DRV_ERR_PARAM, uart_deinit(&uart));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_baud_usart_115200_at_170mhz_oversampling_16);
    RUN_TEST(test_baud_lpuart_115200_at_170mhz);
    RUN_TEST(test_baud_lpuart_9600_needs_prescaler);
    RUN_TEST(test_baud_usart_low_rate_needs_prescaler);
    RUN_TEST(test_baud_high_rates_use_oversampling_8);
    RUN_TEST(test_baud_rejects_unreachable_and_bad_input);
    RUN_TEST(test_init_8n1_registers);
    RUN_TEST(test_init_frame_formats);
    RUN_TEST(test_init_lpuart_with_hsi16_kernel_clock);
    RUN_TEST(test_init_rejects_invalid_configuration);
    RUN_TEST(test_init_timeout_when_peripheral_does_not_acknowledge);
    RUN_TEST(test_polled_write);
    RUN_TEST(test_polled_write_times_out_when_transmitter_stalls);
    RUN_TEST(test_polled_read_masks_to_data_bits);
    RUN_TEST(test_polled_read_reports_line_errors);
    RUN_TEST(test_polled_read_timeout);
    RUN_TEST(test_read_line_buffer_full);
    RUN_TEST(test_argument_checks);
    RUN_TEST(test_irq_receive_into_ring_and_read_line);
    RUN_TEST(test_irq_receive_overflow_is_reported);
    RUN_TEST(test_irq_transmit_drains_through_handler);
    RUN_TEST(test_irq_transmit_with_interrupts_masked_does_not_deadlock);
    RUN_TEST(test_tx_only_irq_mode_leaves_received_data_to_polled_reader);
    RUN_TEST(test_enable_irq_argument_checks);
    RUN_TEST(test_deinit);
    return UNITY_END();
}
