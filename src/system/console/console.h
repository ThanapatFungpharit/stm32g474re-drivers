#ifndef STM32G474RE_SYSTEM_CONSOLE_H
#define STM32G474RE_SYSTEM_CONSOLE_H

/**
 * @file console.h
 * @brief Minimal text console for debugging and panic messages.
 *
 * By default output goes to ITM stimulus port 0, which a debugger shows as
 * SWO output (pin PB3 on the STM32G474RE; the Nucleo's ST-LINK/V3E captures
 * it). The SWO baud rate and TPIU are configured by the debugger; set its
 * "core clock" to the frequency the firmware runs at.
 *
 * console_set_output() redirects output to any byte sink, for example a
 * UART through uart_console_write().
 *
 * Output is best effort: when no consumer is present, characters are
 * dropped rather than blocking the program.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Byte sink used by the console.
 * @param ctx Context pointer given to console_set_output().
 * @param data Bytes to write.
 * @param len Number of bytes.
 * @return Number of bytes accepted.
 * @note Must work with interrupts masked, because panic() and the fault
 *       handlers print with interrupts disabled.
 */
typedef size_t (*ConsoleWriteFn)(void *ctx, const char *data, size_t len);

/**
 * @brief Enable ITM stimulus port 0 when a debugger is attached.
 * @note Without a debugger the ITM stays disabled and output is dropped.
 */
void console_init(void);

/**
 * @brief Redirect console output.
 * @param fn Byte sink, or NULL to return to ITM output.
 * @param ctx Passed unchanged to @p fn.
 */
void console_set_output(ConsoleWriteFn fn, void *ctx);

/** @brief Return true when the selected output can currently accept data. */
bool console_is_ready(void);

/** @brief Write one character. */
void console_putc(char c);

/** @brief Write a NUL-terminated string; NULL is ignored. */
void console_puts(const char *str);

/**
 * @brief Write a buffer.
 * @param buffer Bytes to write, or NULL.
 * @param len Number of bytes.
 * @return Number of bytes accepted.
 */
size_t console_write(const char *buffer, size_t len);

/** @brief Write a signed decimal number. */
void console_put_dec(int32_t value);

/** @brief Write an unsigned decimal number. */
void console_put_udec(uint32_t value);

/**
 * @brief Write a hexadecimal number without prefix.
 * @param value Number to write.
 * @param digits Minimum number of digits (zero padded), 1 to 8.
 */
void console_put_hex(uint32_t value, uint32_t digits);

/**
 * @brief Format an unsigned number in decimal.
 * @param value Number to format.
 * @param buf Output buffer of at least 11 bytes.
 * @return Number of characters written, excluding the terminating NUL.
 */
size_t console_format_udec(uint32_t value, char *buf);

/**
 * @brief Format an unsigned number in upper-case hexadecimal.
 * @param value Number to format.
 * @param digits Minimum number of digits, clamped to 1..8.
 * @param buf Output buffer of at least 9 bytes.
 * @return Number of characters written, excluding the terminating NUL.
 */
size_t console_format_hex(uint32_t value, uint32_t digits, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* STM32G474RE_SYSTEM_CONSOLE_H */
