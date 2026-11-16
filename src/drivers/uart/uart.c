#include "uart.h"

#include "drivers/internal/drv_internal.h"
#include "system/clock/clock.h"
#include "system/time/time.h"


#define ENABLE_TIMEOUT_US        10000U
#define HSI_TIMEOUT_US           5000U
#define CONSOLE_WRITE_TIMEOUT_MS 100U

/* 16x oversampling is kept unless its baud error exceeds this (1 %). */
#define OVER16_PREFERRED_PPM     10000U

/* USART_ISR line-error flags and their ICR clear bits. */
#define ISR_ERRORS (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)
#define ICR_ERRORS (USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF)


/* --- Instance description --------------------------------------------------- */

typedef struct {
    DrvClockGate gate;
    uint32_t     ccipr_shift;  /* position of the 2-bit kernel clock selection */
    bool         lpuart;
    bool         apb2;
    IRQn_Type    irqn;
} UartInfo;

static bool uart_info(const USART_TypeDef *instance, UartInfo *info)
{
#define UART_ENTRY(inst, bus, bit, sel, lp, a2, irq)                       \
    if (instance == (inst)) {                                             \
        const UartInfo entry = { { (bus), (bit) }, (sel), (lp), (a2), (irq) }; \
        *info = entry;                                                    \
        return true;                                                      \
    }

    UART_ENTRY(USART1,  DRV_BUS_APB2,   RCC_APB2ENR_USART1EN,   RCC_CCIPR_USART1SEL_Pos,  false, true,  USART1_IRQn)
    UART_ENTRY(USART2,  DRV_BUS_APB1_1, RCC_APB1ENR1_USART2EN,  RCC_CCIPR_USART2SEL_Pos,  false, false, USART2_IRQn)
    UART_ENTRY(USART3,  DRV_BUS_APB1_1, RCC_APB1ENR1_USART3EN,  RCC_CCIPR_USART3SEL_Pos,  false, false, USART3_IRQn)
    UART_ENTRY(UART4,   DRV_BUS_APB1_1, RCC_APB1ENR1_UART4EN,   RCC_CCIPR_UART4SEL_Pos,   false, false, UART4_IRQn)
    UART_ENTRY(UART5,   DRV_BUS_APB1_1, RCC_APB1ENR1_UART5EN,   RCC_CCIPR_UART5SEL_Pos,   false, false, UART5_IRQn)
    UART_ENTRY(LPUART1, DRV_BUS_APB1_2, RCC_APB1ENR2_LPUART1EN, RCC_CCIPR_LPUART1SEL_Pos, true,  false, LPUART1_IRQn)

#undef UART_ENTRY
    return false;
}


/* --- Baud-rate calculation -------------------------------------------------- */

/* Kernel clock divider for each PRESC value. */
static const uint16_t PRESC_DIV[12] = { 1U, 2U, 4U, 6U, 8U, 10U, 12U, 16U, 32U, 64U, 128U, 256U };

static uint32_t error_ppm(const uint64_t actual, const uint32_t wanted)
{
    const uint64_t diff = (actual > wanted) ? (actual - wanted) : (wanted - actual);
    return (uint32_t)((diff * 1000000ULL) / wanted);
}

static uint64_t div_round(const uint64_t num, const uint64_t den)
{
    return (num + (den / 2U)) / den;
}

DrvStatus uart_calc_baud(const uint32_t kernel_hz, const uint32_t baud_rate,
                         const bool lpuart, UartBaudSetting *setting)
{
    if ((kernel_hz == 0U) || (baud_rate == 0U) || (setting == NULL)) {
        return DRV_ERR_PARAM;
    }

    bool found = false;
    UartBaudSetting best = { 0U, 0U, false, 0U, UINT32_MAX };

    for (uint32_t presc = 0U; presc < 12U; ++presc) {
        const uint64_t f = kernel_hz / PRESC_DIV[presc];

        if (lpuart) {
            /* BRR = 256 * f / baud, 0x300 <= BRR <= 0xFFFFF, 3 * baud <= f. */
            if (((uint64_t)baud_rate * 3U) > f) {
                break; /* larger prescalers only make it worse */
            }
            const uint64_t brr = div_round(f * 256U, baud_rate);
            if ((brr < 0x300U) || (brr > 0xFFFFFU)) {
                continue;
            }
            const uint64_t actual = div_round(f * 256U, brr);
            const UartBaudSetting s = { presc, (uint32_t)brr, false, (uint32_t)actual,
                                        error_ppm(actual, baud_rate) };
            best = s;
            found = true;
            break; /* smallest prescaler gives the finest resolution */
        }

        /* USART, oversampling by 16: BRR = f / baud, 16 <= BRR <= 0xFFFF. */
        const uint64_t div16 = div_round(f, baud_rate);
        if ((div16 >= 16U) && (div16 <= 0xFFFFU)) {
            const uint64_t actual = div_round(f, div16);
            const UartBaudSetting s = { presc, (uint32_t)div16, false, (uint32_t)actual,
                                        error_ppm(actual, baud_rate) };
            if (s.error_ppm < best.error_ppm) {
                best = s;
                found = true;
            }
        }

        /* Oversampling by 8 doubles the reachable rate. 16x sampling tolerates
         * more noise and clock error, so 8x is used only when 16x is
         * impossible or more than 1 % off and 8x is more accurate.
         * USARTDIV = 2 * f / baud; BRR[2:0] = USARTDIV[3:0] >> 1. */
        const uint64_t div8 = div_round(f * 2U, baud_rate);
        if ((div8 >= 16U) && (div8 <= 0xFFFFU)) {
            const uint64_t actual = div_round(f * 2U, div8);
            const uint32_t brr = (uint32_t)((div8 & 0xFFF0U) | ((div8 & 0xFU) >> 1));
            const UartBaudSetting s = { presc, brr, true, (uint32_t)actual,
                                        error_ppm(actual, baud_rate) };
            if ((s.error_ppm < best.error_ppm) &&
                (!found || (best.error_ppm > OVER16_PREFERRED_PPM))) {
                best = s;
                found = true;
            }
        }

        if (found) {
            break; /* a working setting with the smallest prescaler */
        }
    }

    if (!found || (best.error_ppm > UART_MAX_BAUD_ERROR_PPM)) {
        return DRV_ERR_UNSUPPORTED;
    }

    *setting = best;
    return DRV_OK;
}


/* --- Ring buffers ------------------------------------------------------------- */

static uint16_t ring_next(const UartRing *ring, const uint16_t index)
{
    return (uint16_t)(((uint32_t)index + 1U) % ring->size);
}

static bool ring_push(UartRing *ring, const uint8_t byte)
{
    const uint16_t head = ring->head;
    const uint16_t next = ring_next(ring, head);
    if (next == ring->tail) {
        return false; /* full */
    }
    ring->data[head] = byte;
    ring->head = next; /* publish after the data is stored */
    return true;
}

static bool ring_pop(UartRing *ring, uint8_t *byte)
{
    const uint16_t tail = ring->tail;
    if (tail == ring->head) {
        return false; /* empty */
    }
    *byte = ring->data[tail];
    ring->tail = ring_next(ring, tail);
    return true;
}

static size_t ring_count(const UartRing *ring)
{
    const uint32_t head = ring->head;
    const uint32_t tail = ring->tail;
    return (size_t)((head + ring->size - tail) % ring->size);
}


/* --- Helpers ------------------------------------------------------------------ */

static bool handle_ready(const UartHandle *uart)
{
    return (uart != NULL) && (uart->instance != NULL);
}

/** @brief True when this context can be interrupted by the UART IRQ. */
static bool interrupts_can_run(void)
{
    return (__get_PRIMASK() == 0U) && (__get_IPSR() == 0U);
}

static uint32_t kernel_clock_hz(const UartInfo *info, const UartClockSource source)
{
    switch (source) {
        case UART_CLOCK_SYSCLK: return clock_get_sysclk_hz();
        case UART_CLOCK_HSI16:  return CLOCK_HSI_HZ;
        case UART_CLOCK_PCLK:
        default:
            return info->apb2 ? clock_get_pclk2_hz() : clock_get_pclk1_hz();
    }
}

/** @brief Convert pending line-error flags to a status and record them. */
static DrvStatus record_errors(UartHandle *uart, const uint32_t isr)
{
    uint32_t flags = 0U;
    DrvStatus status = DRV_OK;

    if ((isr & USART_ISR_ORE) != 0U) { flags |= UART_ERROR_OVERRUN; status = DRV_ERR_OVERRUN; }
    if ((isr & USART_ISR_NE)  != 0U) { flags |= UART_ERROR_NOISE;   status = DRV_ERR_NOISE; }
    if ((isr & USART_ISR_FE)  != 0U) { flags |= UART_ERROR_FRAMING; status = DRV_ERR_FRAMING; }
    if ((isr & USART_ISR_PE)  != 0U) { flags |= UART_ERROR_PARITY;  status = DRV_ERR_PARITY; }

    if (flags != 0U) {
        uart->instance->ICR = ICR_ERRORS;
        const uint32_t primask = drv_critical_enter();
        uart->errors |= flags;
        drv_critical_exit(primask);
    }
    return status;
}

/**
 * @brief Move one queued byte to the transmitter by polling.
 * @note Used when the UART interrupt cannot run (interrupts masked or a
 *       handler is active). Runs with interrupts masked so it cannot race
 *       the interrupt handler.
 * @return true if a byte was moved or the queue is empty.
 */
static bool tx_poll_service(UartHandle *uart)
{
    USART_TypeDef *u = uart->instance;
    const uint32_t primask = drv_critical_enter();
    bool progressed = true;

    if (ring_count(&uart->tx) != 0U) {
        if ((u->ISR & USART_ISR_TXE_TXFNF) != 0U) {
            uint8_t byte;
            (void)ring_pop(&uart->tx, &byte);
            u->TDR = byte;
        } else {
            progressed = false;
        }
    }

    drv_critical_exit(primask);
    return progressed;
}


/* --- Initialisation ------------------------------------------------------------ */

DrvStatus uart_init(UartHandle *uart, USART_TypeDef *instance, const UartConfig *config)
{
    UartInfo info;

    if ((uart == NULL) || (config == NULL) || !uart_info(instance, &info) ||
        (config->baud_rate == 0U) ||
        ((config->data_bits != UART_DATA_BITS_7) && (config->data_bits != UART_DATA_BITS_8)) ||
        ((uint32_t)config->parity > (uint32_t)UART_PARITY_ODD) ||
        ((config->stop_bits != UART_STOP_BITS_1) && (config->stop_bits != UART_STOP_BITS_2)) ||
        ((uint32_t)config->flow_control > (uint32_t)UART_FLOW_RTS_CTS) ||
        ((uint32_t)config->clock_source > (uint32_t)UART_CLOCK_HSI16)) {
        return DRV_ERR_PARAM;
    }

    uart->instance = NULL; /* not usable until fully initialised */

    if (config->clock_source == UART_CLOCK_HSI16) {
        drv_reg_modify(&RCC->CR, 0U, RCC_CR_HSION);
        if (drv_wait_set(&RCC->CR, RCC_CR_HSIRDY, HSI_TIMEOUT_US) != DRV_OK) {
            return DRV_ERR_TIMEOUT;
        }
    }

    UartBaudSetting baud;
    const DrvStatus status = uart_calc_baud(kernel_clock_hz(&info, config->clock_source),
                                            config->baud_rate, info.lpuart, &baud);
    if (status != DRV_OK) {
        return status;
    }

    /* Start from reset values: clean registers and no stale flags. */
    NVIC_DisableIRQ(info.irqn);
    drv_clock_gate_set(info.gate, true);
    drv_clock_gate_reset(info.gate);

    drv_reg_modify(&RCC->CCIPR, 0x3UL << info.ccipr_shift,
                   (uint32_t)config->clock_source << info.ccipr_shift);

    USART_TypeDef *u = instance;

    /* Frame length = data bits + parity bit: 7 -> M=10, 8 -> M=00, 9 -> M=01. */
    const uint32_t frame_bits = (uint32_t)config->data_bits +
                                ((config->parity != UART_PARITY_NONE) ? 1U : 0U);
    uint32_t cr1 = 0U;
    if (frame_bits == 7U) {
        cr1 |= USART_CR1_M1;
    } else if (frame_bits == 9U) {
        cr1 |= USART_CR1_M0;
    }
    if (config->parity != UART_PARITY_NONE) {
        cr1 |= USART_CR1_PCE;
        if (config->parity == UART_PARITY_ODD) {
            cr1 |= USART_CR1_PS;
        }
    }
    if (baud.over8) {
        cr1 |= USART_CR1_OVER8;
    }

    u->CR1 = cr1; /* UE = 0 while configuring */
    u->CR2 = (config->stop_bits == UART_STOP_BITS_2) ? USART_CR2_STOP_1 : 0U;
    u->CR3 = (config->flow_control == UART_FLOW_RTS_CTS)
                 ? (USART_CR3_RTSE | USART_CR3_CTSE) : 0U;
    u->PRESC = baud.presc;
    u->BRR = baud.brr;

    u->CR1 = cr1 | USART_CR1_UE;
    u->CR1 = cr1 | USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    if ((drv_wait_set(&u->ISR, USART_ISR_TEACK | USART_ISR_REACK, ENABLE_TIMEOUT_US) != DRV_OK)) {
        u->CR1 = 0U;
        drv_clock_gate_set(info.gate, false);
        return DRV_ERR_TIMEOUT;
    }

    uart->data_mask = (config->data_bits == UART_DATA_BITS_7) ? 0x7FU : 0xFFU;
    uart->rx_irq = false;
    uart->tx_irq = false;
    uart->errors = 0U;
    uart->rx.data = NULL;
    uart->rx.size = 0U;
    uart->rx.head = 0U;
    uart->rx.tail = 0U;
    uart->tx = uart->rx;
    uart->instance = instance;
    return DRV_OK;
}

DrvStatus uart_deinit(UartHandle *uart)
{
    UartInfo info;
    if ((uart == NULL) || !uart_info(uart->instance, &info)) {
        return DRV_ERR_PARAM;
    }

    NVIC_DisableIRQ(info.irqn);
    uart->instance->CR1 = 0U;
    drv_clock_gate_reset(info.gate);
    drv_clock_gate_set(info.gate, false);
    NVIC_ClearPendingIRQ(info.irqn);

    uart->instance = NULL;
    uart->rx_irq = false;
    uart->tx_irq = false;
    return DRV_OK;
}

DrvStatus uart_enable_irq(UartHandle *uart, uint8_t *rx_buffer, const size_t rx_size,
                          uint8_t *tx_buffer, const size_t tx_size, const uint32_t priority)
{
    UartInfo info;

    if ((uart == NULL) || (priority > DRV_NVIC_PRIORITY_MAX) ||
        ((rx_buffer != NULL) && ((rx_size < 2U) || (rx_size > 0xFFFFU))) ||
        ((tx_buffer != NULL) && ((tx_size < 2U) || (tx_size > 0xFFFFU))) ||
        ((rx_buffer == NULL) && (tx_buffer == NULL))) {
        return DRV_ERR_PARAM;
    }
    if (!handle_ready(uart) || !uart_info(uart->instance, &info)) {
        return DRV_ERR_STATE;
    }

    USART_TypeDef *u = uart->instance;

    NVIC_DisableIRQ(info.irqn);

    if (rx_buffer != NULL) {
        uart->rx.data = rx_buffer;
        uart->rx.size = (uint16_t)rx_size;
        uart->rx.head = 0U;
        uart->rx.tail = 0U;
        uart->rx_irq = true;
        u->ICR = ICR_ERRORS;
        drv_reg_modify(&u->CR1, 0U, USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE);
        drv_reg_modify(&u->CR3, 0U, USART_CR3_EIE);
    }
    if (tx_buffer != NULL) {
        uart->tx.data = tx_buffer;
        uart->tx.size = (uint16_t)tx_size;
        uart->tx.head = 0U;
        uart->tx.tail = 0U;
        uart->tx_irq = true; /* TXEIE is enabled when data is queued */
    }

    drv_nvic_enable(info.irqn, priority);
    return DRV_OK;
}

void uart_irq_handler(UartHandle *uart)
{
    if (!handle_ready(uart)) {
        return;
    }

    USART_TypeDef *u = uart->instance;
    const uint32_t isr = u->ISR;
    const uint32_t cr1 = u->CR1;

    /* In transmit-only interrupt mode received data belongs to the polled
     * reader; leave it alone. */
    if (uart->rx_irq) {
        if ((isr & ISR_ERRORS) != 0U) {
            (void)record_errors(uart, isr);
        }

        if ((isr & USART_ISR_RXNE_RXFNE) != 0U) {
            const uint8_t byte = (uint8_t)(u->RDR & uart->data_mask);
            const bool corrupt = (isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE)) != 0U;
            if (!corrupt && !ring_push(&uart->rx, byte)) {
                uart->errors |= UART_ERROR_RX_OVERFLOW;
            }
        }
    }

    if (((cr1 & USART_CR1_TXEIE_TXFNFIE) != 0U) && ((isr & USART_ISR_TXE_TXFNF) != 0U)) {
        uint8_t byte;
        if (ring_pop(&uart->tx, &byte)) {
            u->TDR = byte;
        } else {
            u->CR1 &= ~USART_CR1_TXEIE_TXFNFIE; /* queue drained */
        }
    }
}


/* --- Transmit ------------------------------------------------------------------- */

static DrvStatus write_polled(UartHandle *uart, const uint8_t *bytes, const size_t len,
                              TimeTimeout *timeout)
{
    USART_TypeDef *u = uart->instance;

    for (size_t i = 0U; i < len; ++i) {
        while ((u->ISR & USART_ISR_TXE_TXFNF) == 0U) {
            if (time_timeout_expired(timeout)) {
                return DRV_ERR_TIMEOUT;
            }
        }
        u->TDR = bytes[i];
    }
    return DRV_OK;
}

static DrvStatus write_queued(UartHandle *uart, const uint8_t *bytes, const size_t len,
                              TimeTimeout *timeout)
{
    USART_TypeDef *u = uart->instance;

    for (size_t i = 0U; i < len; ++i) {
        while (!ring_push(&uart->tx, bytes[i])) {
            if (!interrupts_can_run()) {
                (void)tx_poll_service(uart);
            }
            if (time_timeout_expired(timeout)) {
                return DRV_ERR_TIMEOUT;
            }
        }
        drv_reg_modify(&u->CR1, 0U, USART_CR1_TXEIE_TXFNFIE);
    }
    return DRV_OK;
}

DrvStatus uart_write(UartHandle *uart, const void *data, const size_t len,
                     const uint32_t timeout_ms)
{
    if ((data == NULL) && (len > 0U)) {
        return DRV_ERR_PARAM;
    }
    if (!handle_ready(uart)) {
        return DRV_ERR_STATE;
    }

    TimeTimeout timeout;
    time_timeout_start_ms(&timeout, timeout_ms);

    return uart->tx_irq ? write_queued(uart, (const uint8_t *)data, len, &timeout)
                        : write_polled(uart, (const uint8_t *)data, len, &timeout);
}

DrvStatus uart_write_string(UartHandle *uart, const char *str, const uint32_t timeout_ms)
{
    if (str == NULL) {
        return DRV_ERR_PARAM;
    }
    size_t len = 0U;
    while (str[len] != '\0') {
        ++len;
    }
    return uart_write(uart, str, len, timeout_ms);
}

DrvStatus uart_write_byte(UartHandle *uart, const uint8_t byte, const uint32_t timeout_ms)
{
    return uart_write(uart, &byte, 1U, timeout_ms);
}

DrvStatus uart_flush(UartHandle *uart, const uint32_t timeout_ms)
{
    if (!handle_ready(uart)) {
        return DRV_ERR_STATE;
    }

    USART_TypeDef *u = uart->instance;
    TimeTimeout timeout;
    time_timeout_start_ms(&timeout, timeout_ms);

    if (uart->tx_irq) {
        while (ring_count(&uart->tx) != 0U) {
            if (!interrupts_can_run()) {
                (void)tx_poll_service(uart);
            }
            if (time_timeout_expired(&timeout)) {
                return DRV_ERR_TIMEOUT;
            }
        }
    }

    /* TC: last stop bit sent and transmit data register empty. */
    while ((u->ISR & (USART_ISR_TC | USART_ISR_TXE_TXFNF)) !=
           (USART_ISR_TC | USART_ISR_TXE_TXFNF)) {
        if (time_timeout_expired(&timeout)) {
            return DRV_ERR_TIMEOUT;
        }
    }
    return DRV_OK;
}


/* --- Receive -------------------------------------------------------------------- */

/**
 * @brief Receive one byte in polled mode.
 * @return DRV_OK, DRV_ERR_TIMEOUT or a line error.
 */
static DrvStatus read_byte_polled(UartHandle *uart, uint8_t *byte, TimeTimeout *timeout)
{
    USART_TypeDef *u = uart->instance;

    for (;;) {
        const uint32_t isr = u->ISR;

        if ((isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE)) != 0U) {
            (void)u->RDR; /* discard the corrupted character */
            return record_errors(uart, isr);
        }
        if ((isr & USART_ISR_RXNE_RXFNE) != 0U) {
            *byte = (uint8_t)(u->RDR & uart->data_mask);
            /* ORE: this byte is valid, but at least one later byte was lost. */
            return ((isr & USART_ISR_ORE) != 0U) ? record_errors(uart, isr) : DRV_OK;
        }
        if ((isr & USART_ISR_ORE) != 0U) {
            return record_errors(uart, isr);
        }
        if (time_timeout_expired(timeout)) {
            return DRV_ERR_TIMEOUT;
        }
    }
}

static DrvStatus read_byte_queued(UartHandle *uart, uint8_t *byte, TimeTimeout *timeout)
{
    for (;;) {
        if (ring_pop(&uart->rx, byte)) {
            return DRV_OK;
        }
        if (time_timeout_expired(timeout)) {
            return DRV_ERR_TIMEOUT;
        }
    }
}

static DrvStatus read_byte(UartHandle *uart, uint8_t *byte, TimeTimeout *timeout)
{
    return uart->rx_irq ? read_byte_queued(uart, byte, timeout)
                        : read_byte_polled(uart, byte, timeout);
}

DrvStatus uart_read(UartHandle *uart, void *data, const size_t len, size_t *received,
                    const uint32_t timeout_ms)
{
    size_t count = 0U;
    DrvStatus status = DRV_OK;

    if (received != NULL) {
        *received = 0U;
    }
    if ((data == NULL) && (len > 0U)) {
        return DRV_ERR_PARAM;
    }
    if (!handle_ready(uart)) {
        return DRV_ERR_STATE;
    }

    TimeTimeout timeout;
    time_timeout_start_ms(&timeout, timeout_ms);

    uint8_t *bytes = (uint8_t *)data;
    while (count < len) {
        uint8_t byte;
        status = read_byte(uart, &byte, &timeout);
        if ((status == DRV_OK) || (status == DRV_ERR_OVERRUN)) {
            bytes[count++] = byte; /* an overrun still delivers a valid byte */
        }
        if (status != DRV_OK) {
            break;
        }
    }

    if (received != NULL) {
        *received = count;
    }
    return status;
}

DrvStatus uart_read_byte(UartHandle *uart, uint8_t *byte, const uint32_t timeout_ms)
{
    return uart_read(uart, byte, 1U, NULL, timeout_ms);
}

DrvStatus uart_read_line(UartHandle *uart, char *line, const size_t size,
                         const uint32_t timeout_ms)
{
    if ((line == NULL) || (size < 2U)) {
        return DRV_ERR_PARAM;
    }
    line[0] = '\0';
    if (!handle_ready(uart)) {
        return DRV_ERR_STATE;
    }

    TimeTimeout timeout;
    time_timeout_start_ms(&timeout, timeout_ms);

    size_t n = 0U;
    for (;;) {
        uint8_t byte;
        const DrvStatus status = read_byte(uart, &byte, &timeout);
        if ((status != DRV_OK) && (status != DRV_ERR_OVERRUN)) {
            line[n] = '\0';
            return status;
        }
        if ((byte == (uint8_t)'\r') || (byte == (uint8_t)'\n')) {
            line[n] = '\0';
            return DRV_OK;
        }
        line[n++] = (char)byte;
        if (n == (size - 1U)) {
            line[n] = '\0';
            return DRV_ERR_BUSY;
        }
    }
}

size_t uart_rx_available(UartHandle *uart)
{
    if (!handle_ready(uart)) {
        return 0U;
    }
    if (uart->rx_irq) {
        return ring_count(&uart->rx);
    }
    return ((uart->instance->ISR & USART_ISR_RXNE_RXFNE) != 0U) ? 1U : 0U;
}

uint32_t uart_take_errors(UartHandle *uart)
{
    if (uart == NULL) {
        return 0U;
    }
    const uint32_t primask = drv_critical_enter();
    const uint32_t errors = uart->errors;
    uart->errors = 0U;
    drv_critical_exit(primask);
    return errors;
}

size_t uart_console_write(void *uart, const char *data, const size_t len)
{
    UartHandle *handle = (UartHandle *)uart;
    size_t written = 0U;

    while (written < len) {
        if (uart_write(handle, &data[written], 1U, CONSOLE_WRITE_TIMEOUT_MS) != DRV_OK) {
            break;
        }
        ++written;
    }
    return written;
}
