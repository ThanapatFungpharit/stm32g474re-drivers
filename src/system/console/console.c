#include "console.h"

#include "stm32g474xx.h"
#include "system/time/time.h"


/* Longest wait for the ITM FIFO before a character is dropped. At the
 * slowest common SWO rates one byte takes well under 1 ms. */
#define ITM_FIFO_TIMEOUT_US 1000U

static ConsoleWriteFn output_fn;
static void *output_ctx;

/* Set after the ITM FIFO failed to drain in time; further characters are
 * dropped immediately until it drains again. */
static bool itm_stalled;


void console_init(void)
{
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0U) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->LAR = 0xC5ACCE55U; /* unlock ITM register writes */
    ITM->TCR |= ITM_TCR_ITMENA_Msk;
    ITM->TER |= 1U;
    itm_stalled = false;
}

void console_set_output(const ConsoleWriteFn fn, void *ctx)
{
    output_fn  = fn;
    output_ctx = ctx;
}

static bool itm_enabled(void)
{
    return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
           ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U) &&
           ((ITM->TER & 1U) != 0U);
}

static bool itm_fifo_ready(void)
{
    return ITM->PORT[0U].u32 != 0U;
}

bool console_is_ready(void)
{
    if (output_fn != NULL) {
        return true;
    }
    return itm_enabled() && itm_fifo_ready();
}

static bool itm_putc(const char c)
{
    if (!itm_enabled()) {
        return false;
    }

    if (!itm_fifo_ready()) {
        if (itm_stalled) {
            return false;
        }

        TimeTimeout timeout;
        time_timeout_start_us(&timeout, ITM_FIFO_TIMEOUT_US);
        while (!itm_fifo_ready()) {
            if (time_timeout_expired(&timeout)) {
                itm_stalled = true;
                return false;
            }
        }
    }

    itm_stalled = false;
    ITM->PORT[0U].u8 = (uint8_t)c;
    return true;
}

size_t console_write(const char *buffer, const size_t len)
{
    if ((buffer == NULL) || (len == 0U)) {
        return 0U;
    }

    if (output_fn != NULL) {
        return output_fn(output_ctx, buffer, len);
    }

    size_t written = 0U;
    while ((written < len) && itm_putc(buffer[written])) {
        ++written;
    }
    return written;
}

void console_putc(const char c)
{
    (void)console_write(&c, 1U);
}

void console_puts(const char *str)
{
    if (str == NULL) {
        return;
    }

    size_t len = 0U;
    while (str[len] != '\0') {
        ++len;
    }
    (void)console_write(str, len);
}


/* --- Number formatting ------------------------------------------------------ */

size_t console_format_udec(uint32_t value, char *buf)
{
    char tmp[10];
    size_t n = 0U;

    do {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    for (size_t i = 0U; i < n; ++i) {
        buf[i] = tmp[n - 1U - i];
    }
    buf[n] = '\0';
    return n;
}

size_t console_format_hex(const uint32_t value, uint32_t digits, char *buf)
{
    static const char hex[] = "0123456789ABCDEF";

    if (digits < 1U) {
        digits = 1U;
    } else if (digits > 8U) {
        digits = 8U;
    }

    /* Count significant digits. */
    uint32_t needed = 1U;
    for (uint32_t v = value >> 4; v != 0U; v >>= 4) {
        ++needed;
    }
    if (needed > digits) {
        digits = needed;
    }

    for (uint32_t i = 0U; i < digits; ++i) {
        buf[digits - 1U - i] = hex[(value >> (4U * i)) & 0xFU];
    }
    buf[digits] = '\0';
    return digits;
}

void console_put_udec(const uint32_t value)
{
    char buf[11];
    (void)console_write(buf, console_format_udec(value, buf));
}

void console_put_dec(const int32_t value)
{
    if (value < 0) {
        console_putc('-');
        /* Negate in unsigned arithmetic so INT32_MIN works. */
        console_put_udec(0U - (uint32_t)value);
    } else {
        console_put_udec((uint32_t)value);
    }
}

void console_put_hex(const uint32_t value, const uint32_t digits)
{
    char buf[9];
    (void)console_write(buf, console_format_hex(value, digits, buf));
}
