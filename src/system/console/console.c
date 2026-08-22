#include "console.h"


void console_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    ITM->LAR = 0xC5ACCE55U;

    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk;
    ITM->TER = 1U;
}

bool console_is_ready(void)
{
    return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
           ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U) &&
           ((ITM->TER & 1U) != 0U) &&
           (ITM->PORT[0U].u32 != 0U);
}

bool console_try_putc(char c)
{
    if (!console_is_ready()) {
        return false;
    }

    ITM->PORT[0U].u8 = (uint8_t)c;
    return true;
}

void console_putc(char c)
{
    (void)console_try_putc(c);
}

void console_puts(const char *str)
{
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {
        if (!console_try_putc(*str++)) {
            return;
        }
    }
}

size_t console_write(const char *buffer, size_t len)
{
    size_t written = 0U;

    if (buffer == NULL) {
        return 0U;
    }

    while (written < len) {
        if (!console_try_putc(buffer[written])) {
            break;
        }
        ++written;
    }

    return written;
}
