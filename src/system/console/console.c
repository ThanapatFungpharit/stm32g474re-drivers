#include "console.h"


/**
 * @brief Initialize ITM stimulus port 0 for console output.
 * @note Enables trace access and writes ITM control registers.
 */
void console_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    ITM->LAR = 0xC5ACCE55U;

    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk;
    ITM->TER = 1U;
}

/**
 * @brief Check whether ITM stimulus port 0 can accept output.
 * @return true when tracing and port 0 are enabled and ready.
 */
bool console_is_ready(void)
{
    return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
           ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U) &&
           ((ITM->TER & 1U) != 0U) &&
           (ITM->PORT[0U].u32 != 0U);
}

/**
 * @brief Attempt to write one character through ITM port 0.
 * @param c Character to write.
 * @return true when the character was accepted; otherwise false.
 */
bool console_try_putc(char c)
{
    if (!console_is_ready()) {
        return false;
    }

    ITM->PORT[0U].u8 = (uint8_t)c;
    return true;
}

/**
 * @brief Write one character through ITM port 0.
 * @param c Character to write.
 * @note Drops the character when ITM is unavailable.
 */
void console_putc(char c)
{
    (void)console_try_putc(c);
}

/**
 * @brief Write a null-terminated string through ITM port 0.
 * @param str String to write, or NULL.
 * @note Stops and drops remaining characters when ITM is unavailable.
 */
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

/**
 * @brief Write bytes through ITM port 0 without blocking.
 * @param buffer Bytes to write, or NULL.
 * @param len Number of bytes to write.
 * @return Number of bytes accepted by ITM.
 */
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
